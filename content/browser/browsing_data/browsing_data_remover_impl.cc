// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_remover_impl.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "content/browser/browsing_data/storage_partition_http_cache_data_remover.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/features/features.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

using base::UserMetricsAction;

namespace content {

namespace {

// Returns whether |origin| matches |origin_type_mask| given the special
// storage |policy|; and if |predicate| is not null, then also whether
// it matches |predicate|. If |origin_type_mask| contains embedder-specific
// datatypes, |embedder_matcher| must not be null; the decision for those
// datatypes will be delegated to it.
bool DoesOriginMatchMaskAndURLs(
    int origin_type_mask,
    const base::Callback<bool(const GURL&)>& predicate,
    const BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher&
        embedder_matcher,
    const GURL& origin,
    storage::SpecialStoragePolicy* policy) {
  if (!predicate.is_null() && !predicate.Run(origin))
    return false;

  const std::vector<std::string>& schemes = url::GetWebStorageSchemes();
  bool is_web_scheme =
      (std::find(schemes.begin(), schemes.end(), origin.GetOrigin().scheme()) !=
       schemes.end());

  // If a websafe origin is unprotected, it matches iff UNPROTECTED_WEB.
  if ((!policy || !policy->IsStorageProtected(origin.GetOrigin())) &&
      is_web_scheme &&
      (origin_type_mask & BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB)) {
    return true;
  }
  origin_type_mask &= ~BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

  // Hosted applications (protected and websafe origins) iff PROTECTED_WEB.
  if (policy && policy->IsStorageProtected(origin.GetOrigin()) &&
      is_web_scheme &&
      (origin_type_mask & BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB)) {
    return true;
  }
  origin_type_mask &= ~BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;

  DCHECK(embedder_matcher || !origin_type_mask)
      << "The mask contains embedder-defined origin types, but there is no "
      << "embedder delegate matcher to process them.";

  if (!embedder_matcher.is_null())
    return embedder_matcher.Run(origin_type_mask, origin, policy);

  return false;
}

void ClearHttpAuthCacheOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    base::Time delete_begin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::HttpNetworkSession* http_session = context_getter->GetURLRequestContext()
                                              ->http_transaction_factory()
                                              ->GetSession();
  DCHECK(http_session);
  http_session->http_auth_cache()->ClearEntriesAddedWithin(base::Time::Now() -
                                                           delete_begin);
  http_session->CloseAllConnections();
}

void OnClearedChannelIDsOnIOThread(net::URLRequestContextGetter* rq_context,
                                   base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Need to close open SSL connections which may be using the channel ids we
  // are deleting.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  rq_context->GetURLRequestContext()
      ->ssl_config_service()
      ->NotifySSLConfigChange();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(callback));
}

void ClearChannelIDsOnIOThread(
    const base::Callback<bool(const std::string&)>& domain_predicate,
    base::Time delete_begin,
    base::Time delete_end,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDService* channel_id_service =
      request_context->GetURLRequestContext()->channel_id_service();
  channel_id_service->GetChannelIDStore()->DeleteForDomainsCreatedBetween(
      domain_predicate, delete_begin, delete_end,
      base::Bind(&OnClearedChannelIDsOnIOThread,
                 base::RetainedRef(std::move(request_context)),
                 base::Passed(std::move(callback))));
}

}  // namespace

BrowsingDataRemoverImpl::BrowsingDataRemoverImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      remove_mask_(-1),
      origin_type_mask_(-1),
      is_removing_(false),
      storage_partition_for_testing_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(browser_context_);
}

BrowsingDataRemoverImpl::~BrowsingDataRemoverImpl() {
  if (!task_queue_.empty()) {
    VLOG(1) << "BrowsingDataRemoverImpl shuts down with " << task_queue_.size()
            << " pending tasks";
  }

  UMA_HISTOGRAM_EXACT_LINEAR("History.ClearBrowsingData.TaskQueueAtShutdown",
                             task_queue_.size(), 10);

  // If we are still removing data, notify observers that their task has been
  // (albeit unsucessfuly) processed, so they can unregister themselves.
  // TODO(bauerb): If it becomes a problem that browsing data might not actually
  // be fully cleared when an observer is notified, add a success flag.
  while (!task_queue_.empty()) {
    if (observer_list_.HasObserver(task_queue_.front().observer))
      task_queue_.front().observer->OnBrowsingDataRemoverDone();
    task_queue_.pop();
  }
}

void BrowsingDataRemoverImpl::SetRemoving(bool is_removing) {
  DCHECK_NE(is_removing_, is_removing);
  is_removing_ = is_removing;
}

void BrowsingDataRemoverImpl::SetEmbedderDelegate(
    BrowsingDataRemoverDelegate* embedder_delegate) {
  embedder_delegate_ = embedder_delegate;
}

bool BrowsingDataRemoverImpl::DoesOriginMatchMask(
    int origin_type_mask,
    const GURL& origin,
    storage::SpecialStoragePolicy* policy) const {
  BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher embedder_matcher;
  if (embedder_delegate_)
    embedder_matcher = embedder_delegate_->GetOriginTypeMatcher();

  return DoesOriginMatchMaskAndURLs(origin_type_mask,
                                    base::Callback<bool(const GURL&)>(),
                                    embedder_matcher, origin, policy);
}

void BrowsingDataRemoverImpl::Remove(const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     int remove_mask,
                                     int origin_type_mask) {
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<BrowsingDataFilterBuilder>(), nullptr);
}

void BrowsingDataRemoverImpl::RemoveAndReply(const base::Time& delete_begin,
                                             const base::Time& delete_end,
                                             int remove_mask,
                                             int origin_type_mask,
                                             Observer* observer) {
  DCHECK(observer);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<BrowsingDataFilterBuilder>(), observer);
}

void BrowsingDataRemoverImpl::RemoveWithFilter(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
  DCHECK(filter_builder);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::move(filter_builder), nullptr);
}

void BrowsingDataRemoverImpl::RemoveWithFilterAndReply(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK(filter_builder);
  DCHECK(observer);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::move(filter_builder), observer);
}

void BrowsingDataRemoverImpl::RemoveInternal(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK(!observer || observer_list_.HasObserver(observer))
      << "Every observer must register itself (by calling AddObserver()) "
      << "before observing a removal task.";

  // Remove() and RemoveAndReply() pass a null pointer to indicate no filter.
  // No filter is equivalent to one that |IsEmptyBlacklist()|.
  if (!filter_builder) {
    filter_builder =
        BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST);
    DCHECK(filter_builder->IsEmptyBlacklist());
  }

  task_queue_.emplace(delete_begin, delete_end, remove_mask, origin_type_mask,
                      std::move(filter_builder), observer);

  // If this is the only scheduled task, execute it immediately. Otherwise,
  // it will be automatically executed when all tasks scheduled before it
  // finish.
  if (task_queue_.size() == 1) {
    SetRemoving(true);
    RunNextTask();
  }
}

void BrowsingDataRemoverImpl::RunNextTask() {
  DCHECK(!task_queue_.empty());
  const RemovalTask& removal_task = task_queue_.front();

  RemoveImpl(removal_task.delete_begin, removal_task.delete_end,
             removal_task.remove_mask, *removal_task.filter_builder,
             removal_task.origin_type_mask);
}

void BrowsingDataRemoverImpl::RemoveImpl(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    const BrowsingDataFilterBuilder& filter_builder,
    int origin_type_mask) {
  // =============== README before adding more storage backends ===============
  //
  // If you're adding a data storage backend that is included among
  // RemoveDataMask::FILTERABLE_DATATYPES, you must do one of the following:
  // 1. Support one of the filters generated by |filter_builder|.
  // 2. Add a comment explaining why is it acceptable in your case to delete all
  //    data without filtering URLs / origins / domains.
  // 3. Do not support partial deletion, i.e. only delete your data if
  //    |filter_builder.IsEmptyBlacklist()|. Add a comment explaining why this
  //    is acceptable.
  base::OnceClosure synchronous_clear_operations(
      CreatePendingTaskCompletionClosure());

  // crbug.com/140910: Many places were calling this with base::Time() as
  // delete_end, even though they should've used base::Time::Max().
  DCHECK_NE(base::Time(), delete_end);

  delete_begin_ = delete_begin;
  delete_end_ = delete_end;
  remove_mask_ = remove_mask;
  origin_type_mask_ = origin_type_mask;

  // Record the combined deletion of cookies and cache.
  CookieOrCacheDeletionChoice choice = NEITHER_COOKIES_NOR_CACHE;
  if (remove_mask & DATA_TYPE_COOKIES &&
      origin_type_mask_ & ORIGIN_TYPE_UNPROTECTED_WEB) {
    choice =
        remove_mask & DATA_TYPE_CACHE ? BOTH_COOKIES_AND_CACHE : ONLY_COOKIES;
  } else if (remove_mask & DATA_TYPE_CACHE) {
    choice = ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCache", choice,
      MAX_CHOICE_VALUE);

  //////////////////////////////////////////////////////////////////////////////
  // INITIALIZATION
  base::Callback<bool(const GURL& url)> filter =
      filter_builder.BuildGeneralFilter();

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_DOWNLOADS
  if ((remove_mask & DATA_TYPE_DOWNLOADS) &&
      (!embedder_delegate_ || embedder_delegate_->MayRemoveDownloadHistory())) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Downloads"));
    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser_context_);
    download_manager->RemoveDownloadsByURLAndTime(filter, delete_begin_,
                                                  delete_end_);
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_CHANNEL_IDS
  // Channel IDs are not separated for protected and unprotected web
  // origins. We check the origin_type_mask_ to prevent unintended deletion.
  if (remove_mask & DATA_TYPE_CHANNEL_IDS &&
      origin_type_mask_ & ORIGIN_TYPE_UNPROTECTED_WEB) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ChannelIDs"));
    // Since we are running on the UI thread don't call GetURLRequestContext().
    scoped_refptr<net::URLRequestContextGetter> request_context =
        BrowserContext::GetDefaultStoragePartition(browser_context_)
            ->GetURLRequestContext();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ClearChannelIDsOnIOThread,
                       filter_builder.BuildChannelIDFilter(), delete_begin_,
                       delete_end_, std::move(request_context),
                       CreatePendingTaskCompletionClosure()));
  }

  //////////////////////////////////////////////////////////////////////////////
  // STORAGE PARTITION DATA
  uint32_t storage_partition_remove_mask = 0;

  // We ignore the DATA_TYPE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request DATA_TYPE_SITE_DATA with another origin type
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and other origin types.
  if (remove_mask & DATA_TYPE_COOKIES &&
      origin_type_mask_ & ORIGIN_TYPE_UNPROTECTED_WEB) {
    storage_partition_remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  }
  if (remove_mask & DATA_TYPE_LOCAL_STORAGE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  }
  if (remove_mask & DATA_TYPE_INDEXED_DB) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  }
  if (remove_mask & DATA_TYPE_WEB_SQL) {
    storage_partition_remove_mask |= StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  }
  if (remove_mask & DATA_TYPE_APP_CACHE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  }
  if (remove_mask & DATA_TYPE_SERVICE_WORKERS) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  }
  if (remove_mask & DATA_TYPE_CACHE_STORAGE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  }
  if (remove_mask & DATA_TYPE_FILE_SYSTEMS) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  }

  // Content Decryption Modules used by Encrypted Media store licenses in a
  // private filesystem. These are different than content licenses used by
  // Flash (which are deleted father down in this method).
  if (remove_mask & DATA_TYPE_MEDIA_LICENSES) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA;
  }

  StoragePartition* storage_partition;
  if (storage_partition_for_testing_) {
    storage_partition = storage_partition_for_testing_;
  } else {
    storage_partition =
        BrowserContext::GetDefaultStoragePartition(browser_context_);
  }

  if (storage_partition_remove_mask) {
    uint32_t quota_storage_remove_mask =
        ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;

    if (delete_begin_ == base::Time() ||
        ((origin_type_mask_ & ~ORIGIN_TYPE_UNPROTECTED_WEB) != 0)) {
      // If we're deleting since the beginning of time, or we're removing
      // protected origins, then remove persistent quota data.
      quota_storage_remove_mask |=
          StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
    }

    // If cookies are supposed to be conditionally deleted from the storage
    // partition, create a cookie matcher function.
    StoragePartition::CookieMatcherFunction cookie_matcher;
    if (!filter_builder.IsEmptyBlacklist() &&
        (storage_partition_remove_mask &
         StoragePartition::REMOVE_DATA_MASK_COOKIES)) {
      cookie_matcher = filter_builder.BuildCookieFilter();
    }

    BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher embedder_matcher;
    if (embedder_delegate_)
      embedder_matcher = embedder_delegate_->GetOriginTypeMatcher();

    storage_partition->ClearData(
        storage_partition_remove_mask, quota_storage_remove_mask,
        base::BindRepeating(&DoesOriginMatchMaskAndURLs, origin_type_mask_,
                            filter, embedder_matcher),
        cookie_matcher, delete_begin_, delete_end_,
        CreatePendingTaskCompletionClosure());
  }

  //////////////////////////////////////////////////////////////////////////////
  // CACHE
  if (remove_mask & DATA_TYPE_CACHE) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Cache"));

    // TODO(msramek): Clear the cache of all renderers.

    storage_partition->ClearHttpAndMediaCaches(
        delete_begin, delete_end,
        filter_builder.IsEmptyBlacklist() ? base::Callback<bool(const GURL&)>()
                                          : filter,
        CreatePendingTaskCompletionClosure());

    // When clearing cache, wipe accumulated network related data
    // (TransportSecurityState and HttpServerPropertiesManager data).
    storage_partition->GetNetworkContext()->ClearNetworkingHistorySince(
        delete_begin, CreatePendingTaskCompletionClosure());

    // Tell the shader disk cache to clear.
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ShaderCache"));
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Auth cache.
  if (remove_mask & DATA_TYPE_COOKIES) {
    scoped_refptr<net::URLRequestContextGetter> request_context =
        BrowserContext::GetDefaultStoragePartition(browser_context_)
            ->GetURLRequestContext();
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ClearHttpAuthCacheOnIOThread,
                       std::move(request_context), delete_begin_),
        CreatePendingTaskCompletionClosure());
  }

  //////////////////////////////////////////////////////////////////////////////
  // Embedder data.
  if (embedder_delegate_) {
    embedder_delegate_->RemoveEmbedderData(
        delete_begin_, delete_end_, remove_mask, filter_builder,
        origin_type_mask, CreatePendingTaskCompletionClosure());
  }

  // Notify in case all actions taken were synchronous.
  std::move(synchronous_clear_operations).Run();
}

void BrowsingDataRemoverImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BrowsingDataRemoverImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowsingDataRemoverImpl::SetWouldCompleteCallbackForTesting(
    const base::Callback<void(const base::Closure& continue_to_completion)>&
        callback) {
  would_complete_callback_ = callback;
}

void BrowsingDataRemoverImpl::OverrideStoragePartitionForTesting(
    StoragePartition* storage_partition) {
  storage_partition_for_testing_ = storage_partition;
}

const base::Time& BrowsingDataRemoverImpl::GetLastUsedBeginTime() {
  return delete_begin_;
}

const base::Time& BrowsingDataRemoverImpl::GetLastUsedEndTime() {
  return delete_end_;
}

int BrowsingDataRemoverImpl::GetLastUsedRemovalMask() {
  return remove_mask_;
}

int BrowsingDataRemoverImpl::GetLastUsedOriginTypeMask() {
  return origin_type_mask_;
}

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer)
    : delete_begin(delete_begin),
      delete_end(delete_end),
      remove_mask(remove_mask),
      origin_type_mask(origin_type_mask),
      filter_builder(std::move(filter_builder)),
      observer(observer) {}

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(
    RemovalTask&& other) noexcept = default;

BrowsingDataRemoverImpl::RemovalTask::~RemovalTask() {}

void BrowsingDataRemoverImpl::Notify() {
  // Some tests call |RemoveImpl| directly, without using the task scheduler.
  // TODO(msramek): Improve those tests so we don't have to do this. Tests
  // relying on |RemoveImpl| do so because they need to pass in
  // BrowsingDataFilterBuilder while still keeping ownership of it. Making
  // BrowsingDataFilterBuilder copyable would solve this.
  if (!is_removing_) {
    DCHECK(task_queue_.empty());
    return;
  }

  // Inform the observer of the current task unless it has unregistered
  // itself in the meantime.
  DCHECK(!task_queue_.empty());

  if (task_queue_.front().observer != nullptr &&
      observer_list_.HasObserver(task_queue_.front().observer)) {
    task_queue_.front().observer->OnBrowsingDataRemoverDone();
  }

  task_queue_.pop();

  if (task_queue_.empty()) {
    // All removal tasks have finished. Inform the observers that we're idle.
    SetRemoving(false);
    return;
  }

  // Yield to the UI thread before executing the next removal task.
  // TODO(msramek): Consider also adding a backoff if too many tasks
  // are scheduled.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&BrowsingDataRemoverImpl::RunNextTask, GetWeakPtr()));
}

void BrowsingDataRemoverImpl::OnTaskComplete() {
  // TODO(brettw) http://crbug.com/305259: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.

  DCHECK_GT(num_pending_tasks_, 0);
  num_pending_tasks_--;

  if (num_pending_tasks_ > 0)
    return;

  if (!would_complete_callback_.is_null()) {
    would_complete_callback_.Run(
        base::Bind(&BrowsingDataRemoverImpl::Notify, GetWeakPtr()));
    return;
  }

  Notify();
}

base::OnceClosure
BrowsingDataRemoverImpl::CreatePendingTaskCompletionClosure() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  num_pending_tasks_++;
  return base::Bind(&BrowsingDataRemoverImpl::OnTaskComplete, GetWeakPtr());
}

base::WeakPtr<BrowsingDataRemoverImpl> BrowsingDataRemoverImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::WeakPtr<BrowsingDataRemoverImpl> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();

  // Immediately bind the weak pointer to the UI thread. This makes it easier
  // to discover potential misuse on the IO thread.
  weak_ptr.get();

  return weak_ptr;
}

}  // namespace content
