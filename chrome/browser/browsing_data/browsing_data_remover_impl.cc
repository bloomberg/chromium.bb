// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover_impl.h"

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
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/content/storage_partition_http_cache_data_remover.h"
#include "components/prefs/pref_service.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
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
using content::BrowserContext;
using content::BrowserThread;
using content::BrowsingDataFilterBuilder;
using content::DOMStorageContext;

namespace {

template <typename T>
void IgnoreArgumentHelper(const base::Closure& callback, T unused_argument) {
  callback.Run();
}

// Another convenience method to turn a callback without arguments into one that
// accepts (and ignores) a single argument.
template <typename T>
base::Callback<void(T)> IgnoreArgument(const base::Closure& callback) {
  return base::Bind(&IgnoreArgumentHelper<T>, callback);
}

// Helper to create callback for BrowsingDataRemoverImpl::DoesOriginMatchMask.
bool DoesOriginMatchMaskAndUrls(
    int origin_type_mask,
    const base::Callback<bool(const GURL&)>& predicate,
    const GURL& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return predicate.Run(origin) &&
         BrowsingDataHelper::DoesOriginMatchMask(origin, origin_type_mask,
                                                 special_storage_policy);
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
                                   const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Need to close open SSL connections which may be using the channel ids we
  // are deleting.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  rq_context->GetURLRequestContext()
      ->ssl_config_service()
      ->NotifySSLConfigChange();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void ClearChannelIDsOnIOThread(
    const base::Callback<bool(const std::string&)>& domain_predicate,
    base::Time delete_begin,
    base::Time delete_end,
    scoped_refptr<net::URLRequestContextGetter> rq_context,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDService* channel_id_service =
      rq_context->GetURLRequestContext()->channel_id_service();
  channel_id_service->GetChannelIDStore()->DeleteForDomainsCreatedBetween(
      domain_predicate, delete_begin, delete_end,
      base::Bind(&OnClearedChannelIDsOnIOThread,
                 base::RetainedRef(std::move(rq_context)), callback));
}

}  // namespace

BrowsingDataRemoverImpl::CompletionInhibitor*
    BrowsingDataRemoverImpl::completion_inhibitor_ = nullptr;

BrowsingDataRemoverImpl::SubTask::SubTask(const base::Closure& forward_callback)
    : is_pending_(false),
      forward_callback_(forward_callback),
      weak_ptr_factory_(this) {
  DCHECK(!forward_callback_.is_null());
}

BrowsingDataRemoverImpl::SubTask::~SubTask() {}

void BrowsingDataRemoverImpl::SubTask::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_pending_);
  is_pending_ = true;
}

base::Closure BrowsingDataRemoverImpl::SubTask::GetCompletionCallback() {
  return base::Bind(&BrowsingDataRemoverImpl::SubTask::CompletionCallback,
                    weak_ptr_factory_.GetWeakPtr());
}

void BrowsingDataRemoverImpl::SubTask::CompletionCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(is_pending_);
  is_pending_ = false;
  forward_callback_.Run();
}

BrowsingDataRemoverImpl::BrowsingDataRemoverImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      remove_mask_(-1),
      origin_type_mask_(-1),
      is_removing_(false),
      sub_task_forward_callback_(
          base::Bind(&BrowsingDataRemoverImpl::NotifyIfDone,
                     base::Unretained(this))),
      synchronous_clear_operations_(sub_task_forward_callback_),
      clear_embedder_data_(sub_task_forward_callback_),
      clear_cache_(sub_task_forward_callback_),
      clear_channel_ids_(sub_task_forward_callback_),
      clear_http_auth_cache_(sub_task_forward_callback_),
      clear_storage_partition_data_(sub_task_forward_callback_),
      weak_ptr_factory_(this) {
  DCHECK(browser_context_);
}

BrowsingDataRemoverImpl::~BrowsingDataRemoverImpl() {
  if (!task_queue_.empty()) {
    VLOG(1) << "BrowsingDataRemoverImpl shuts down with " << task_queue_.size()
            << " pending tasks";
  }

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

void BrowsingDataRemoverImpl::Shutdown() {
  embedder_delegate_.reset();
}

void BrowsingDataRemoverImpl::SetRemoving(bool is_removing) {
  DCHECK_NE(is_removing_, is_removing);
  is_removing_ = is_removing;
}

void BrowsingDataRemoverImpl::SetEmbedderDelegate(
    std::unique_ptr<BrowsingDataRemoverDelegate> embedder_delegate) {
  embedder_delegate_ = std::move(embedder_delegate);
}

BrowsingDataRemoverDelegate*
BrowsingDataRemoverImpl::GetEmbedderDelegate() const {
  return embedder_delegate_.get();
}

void BrowsingDataRemoverImpl::Remove(const base::Time& delete_begin,
                                 const base::Time& delete_end,
                                 int remove_mask,
                                 int origin_type_mask) {
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<BrowsingDataFilterBuilder>(), nullptr);
}

void BrowsingDataRemoverImpl::RemoveAndReply(
    const base::Time& delete_begin,
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
  DCHECK_EQ(0, remove_mask & ~FILTERABLE_DATATYPES);
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
  DCHECK_EQ(0, remove_mask & ~FILTERABLE_DATATYPES);
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
    filter_builder = BrowsingDataFilterBuilder::Create(
        BrowsingDataFilterBuilder::BLACKLIST);
    DCHECK(filter_builder->IsEmptyBlacklist());
  }

  task_queue_.emplace(
      delete_begin,
      delete_end,
      remove_mask,
      origin_type_mask,
      std::move(filter_builder),
      observer);

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

  RemoveImpl(removal_task.delete_begin,
             removal_task.delete_end,
             removal_task.remove_mask,
             *removal_task.filter_builder,
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
  synchronous_clear_operations_.Start();

  // crbug.com/140910: Many places were calling this with base::Time() as
  // delete_end, even though they should've used base::Time::Max().
  DCHECK_NE(base::Time(), delete_end);

  delete_begin_ = delete_begin;
  delete_end_ = delete_end;
  remove_mask_ = remove_mask;
  origin_type_mask_ = origin_type_mask;

  if (origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsUnprotectedWeb"));
  }
  if (origin_type_mask_ & BrowsingDataHelper::PROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsProtectedWeb"));
  }
  if (origin_type_mask_ & BrowsingDataHelper::EXTENSION) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsExtension"));
  }
  // If this fires, we added a new BrowsingDataHelper::OriginTypeMask without
  // updating the user metrics above.
  static_assert(
      BrowsingDataHelper::ALL == (BrowsingDataHelper::UNPROTECTED_WEB |
                                  BrowsingDataHelper::PROTECTED_WEB |
                                  BrowsingDataHelper::EXTENSION),
      "OriginTypeMask has been updated without updating user metrics");

  // Record the combined deletion of cookies and cache.
  CookieOrCacheDeletionChoice choice = NEITHER_COOKIES_NOR_CACHE;
  if (remove_mask & REMOVE_COOKIES &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    choice = remove_mask & REMOVE_CACHE ? BOTH_COOKIES_AND_CACHE
                                        : ONLY_COOKIES;
  } else if (remove_mask & REMOVE_CACHE) {
    choice = ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCache",
      choice, MAX_CHOICE_VALUE);

  // Managed devices and supervised users can have restrictions on history
  // deletion.
  // TODO(crbug.com/668114): This should be provided via ContentBrowserClient
  // once BrowsingDataRemoverImpl moves to content.
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();
  bool may_delete_history =
      prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);

  //////////////////////////////////////////////////////////////////////////////
  // INITIALIZATION
  base::Callback<bool(const GURL& url)> filter =
      filter_builder.BuildGeneralFilter();

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_DOWNLOADS
  if ((remove_mask & REMOVE_DOWNLOADS) && may_delete_history) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Downloads"));
    content::DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser_context_);
    download_manager->RemoveDownloadsByURLAndTime(filter,
                                                  delete_begin_, delete_end_);
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_CHANNEL_IDS
  // Channel IDs are not separated for protected and unprotected web
  // origins. We check the origin_type_mask_ to prevent unintended deletion.
  if (remove_mask & REMOVE_CHANNEL_IDS &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_ChannelIDs"));
    // Since we are running on the UI thread don't call GetURLRequestContext().
    scoped_refptr<net::URLRequestContextGetter> rq_context =
        content::BrowserContext::GetDefaultStoragePartition(browser_context_)->
          GetURLRequestContext();
    clear_channel_ids_.Start();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ClearChannelIDsOnIOThread,
                   filter_builder.BuildChannelIDFilter(),
                   delete_begin_, delete_end_, std::move(rq_context),
                   clear_channel_ids_.GetCompletionCallback()));
  }

  //////////////////////////////////////////////////////////////////////////////
  // STORAGE PARTITION DATA
  uint32_t storage_partition_remove_mask = 0;

  // We ignore the REMOVE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request REMOVE_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if (remove_mask & REMOVE_COOKIES &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
  }
  if (remove_mask & REMOVE_LOCAL_STORAGE) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  }
  if (remove_mask & REMOVE_INDEXEDDB) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  }
  if (remove_mask & REMOVE_WEBSQL) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  }
  if (remove_mask & REMOVE_APPCACHE) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  }
  if (remove_mask & REMOVE_SERVICE_WORKERS) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  }
  if (remove_mask & REMOVE_CACHE_STORAGE) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  }
  if (remove_mask & REMOVE_FILE_SYSTEMS) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  }

  // Content Decryption Modules used by Encrypted Media store licenses in a
  // private filesystem. These are different than content licenses used by
  // Flash (which are deleted father down in this method).
  if (remove_mask & REMOVE_MEDIA_LICENSES) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA;
  }

  if (storage_partition_remove_mask) {
    clear_storage_partition_data_.Start();

    content::StoragePartition* storage_partition;
    if (storage_partition_for_testing_) {
      storage_partition = storage_partition_for_testing_;
    } else {
      storage_partition =
          BrowserContext::GetDefaultStoragePartition(browser_context_);
    }

    uint32_t quota_storage_remove_mask =
        ~content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;

    if (delete_begin_ == base::Time() ||
        origin_type_mask_ &
          (BrowsingDataHelper::PROTECTED_WEB | BrowsingDataHelper::EXTENSION)) {
      // If we're deleting since the beginning of time, or we're removing
      // protected origins, then remove persistent quota data.
      quota_storage_remove_mask |=
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
    }

    // If cookies are supposed to be conditionally deleted from the storage
    // partition, create a cookie matcher function.
    content::StoragePartition::CookieMatcherFunction cookie_matcher;
    if (!filter_builder.IsEmptyBlacklist() &&
        (storage_partition_remove_mask &
            content::StoragePartition::REMOVE_DATA_MASK_COOKIES)) {
      cookie_matcher = filter_builder.BuildCookieFilter();
    }

    storage_partition->ClearData(
        storage_partition_remove_mask, quota_storage_remove_mask,
        base::Bind(&DoesOriginMatchMaskAndUrls, origin_type_mask_, filter),
        cookie_matcher, delete_begin_, delete_end_,
        clear_storage_partition_data_.GetCompletionCallback());
  }

  //////////////////////////////////////////////////////////////////////////////
  // CACHE
  if (remove_mask & REMOVE_CACHE) {
    // Tell the renderers to clear their cache.
    web_cache::WebCacheManager::GetInstance()->ClearCache();

    content::RecordAction(UserMetricsAction("ClearBrowsingData_Cache"));

    clear_cache_.Start();
    // StoragePartitionHttpCacheDataRemover deletes itself when it is done.
    if (filter_builder.IsEmptyBlacklist()) {
      browsing_data::StoragePartitionHttpCacheDataRemover::CreateForRange(
          BrowserContext::GetDefaultStoragePartition(browser_context_),
          delete_begin_, delete_end_)
          ->Remove(clear_cache_.GetCompletionCallback());
    } else {
      browsing_data::StoragePartitionHttpCacheDataRemover::
          CreateForURLsAndRange(
              BrowserContext::GetDefaultStoragePartition(browser_context_),
              filter, delete_begin_, delete_end_)
              ->Remove(clear_cache_.GetCompletionCallback());
    }

    // Tell the shader disk cache to clear.
    content::RecordAction(UserMetricsAction("ClearBrowsingData_ShaderCache"));
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Auth cache.
  if (remove_mask & REMOVE_COOKIES) {
    scoped_refptr<net::URLRequestContextGetter> request_context =
        BrowserContext::GetDefaultStoragePartition(browser_context_)
            ->GetURLRequestContext();
    clear_http_auth_cache_.Start();
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ClearHttpAuthCacheOnIOThread, std::move(request_context),
                   delete_begin_),
        clear_http_auth_cache_.GetCompletionCallback());
  }

  //////////////////////////////////////////////////////////////////////////////
  // Embedder data.
  if (embedder_delegate_) {
    clear_embedder_data_.Start();
    embedder_delegate_->RemoveEmbedderData(
        delete_begin_,
        delete_end_,
        remove_mask,
        filter_builder,
        origin_type_mask,
        clear_embedder_data_.GetCompletionCallback());
  }

  // Notify in case all actions taken were synchronous.
  synchronous_clear_operations_.GetCompletionCallback().Run();
}

void BrowsingDataRemoverImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BrowsingDataRemoverImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowsingDataRemoverImpl::OverrideStoragePartitionForTesting(
    content::StoragePartition* storage_partition) {
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

BrowsingDataRemoverImpl::RemovalTask::~RemovalTask() {}

bool BrowsingDataRemoverImpl::AllDone() {
  return !synchronous_clear_operations_.is_pending() &&
         !clear_embedder_data_.is_pending() &&
         !clear_cache_.is_pending() &&
         !clear_channel_ids_.is_pending() &&
         !clear_http_auth_cache_.is_pending() &&
         !clear_storage_partition_data_.is_pending();
}

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
      base::Bind(&BrowsingDataRemoverImpl::RunNextTask,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BrowsingDataRemoverImpl::NotifyIfDone() {
  // TODO(brettw) http://crbug.com/305259: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.

  if (!AllDone())
    return;

  if (completion_inhibitor_) {
    completion_inhibitor_->OnBrowsingDataRemoverWouldComplete(
        this, base::Bind(&BrowsingDataRemoverImpl::Notify,
                         weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  Notify();
}
