// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DownloadHistory manages persisting DownloadItems to the history service by
// observing a single DownloadManager and all its DownloadItems using an
// AllDownloadItemNotifier.
//
// DownloadHistory decides whether and when to add items to, remove items from,
// and update items in the database. DownloadHistory uses DownloadHistoryData to
// store per-DownloadItem data such as whether the item is persisted or being
// persisted, and the last history::DownloadRow that was passed to the database.
// When the DownloadManager and its delegate (ChromeDownloadManagerDelegate) are
// initialized, DownloadHistory is created and queries the HistoryService. When
// the HistoryService calls back from QueryDownloads() to QueryCallback(),
// DownloadHistory uses DownloadManager::CreateDownloadItem() to inform
// DownloadManager of these persisted DownloadItems. CreateDownloadItem()
// internally calls OnDownloadCreated(), which normally adds items to the
// database, so QueryCallback() uses |loading_id_| to disable adding these items
// to the database.  If a download is removed via OnDownloadRemoved() while the
// item is still being added to the database, DownloadHistory uses
// |removed_while_adding_| to remember to remove the item when its ItemAdded()
// callback is called.  All callbacks are bound with a weak pointer to
// DownloadHistory to prevent use-after-free bugs.
// ChromeDownloadManagerDelegate owns DownloadHistory, and deletes it in
// Shutdown(), which is called by DownloadManagerImpl::Shutdown() after all
// DownloadItems are destroyed.

#include "chrome/browser/download/download_history.h"

#include <utility>

#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/download/download_crx_util.h"
#include "components/history/content/browser/download_constants_utils.h"
#include "components/history/core/browser/download_database.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#endif

namespace {

// Per-DownloadItem data. This information does not belong inside DownloadItem,
// and keeping maps in DownloadHistory from DownloadItem to this information is
// error-prone and complicated. Unfortunately, DownloadHistory::removing_*_ and
// removed_while_adding_ cannot be moved into this class partly because
// DownloadHistoryData is destroyed when DownloadItems are destroyed, and we
// have no control over when DownloadItems are destroyed.
class DownloadHistoryData : public base::SupportsUserData::Data {
 public:
  enum PersistenceState {
    NOT_PERSISTED,
    PERSISTING,
    PERSISTED,
  };

  static DownloadHistoryData* Get(content::DownloadItem* item) {
    base::SupportsUserData::Data* data = item->GetUserData(kKey);
    return (data == NULL) ? NULL :
      static_cast<DownloadHistoryData*>(data);
  }

  static const DownloadHistoryData* Get(const content::DownloadItem* item) {
    const base::SupportsUserData::Data* data = item->GetUserData(kKey);
    return (data == NULL) ? NULL
                          : static_cast<const DownloadHistoryData*>(data);
  }

  explicit DownloadHistoryData(content::DownloadItem* item)
      : state_(NOT_PERSISTED),
        was_restored_from_history_(false) {
    item->SetUserData(kKey, this);
  }

  ~DownloadHistoryData() override {}

  PersistenceState state() const { return state_; }
  void SetState(PersistenceState s) { state_ = s; }

  bool was_restored_from_history() const { return was_restored_from_history_; }
  void set_was_restored_from_history(bool value) {
    was_restored_from_history_ = value;
  }

  // This allows DownloadHistory::OnDownloadUpdated() to see what changed in a
  // DownloadItem if anything, in order to prevent writing to the database
  // unnecessarily.  It is nullified when the item is no longer in progress in
  // order to save memory.
  history::DownloadRow* info() { return info_.get(); }
  void set_info(const history::DownloadRow& i) {
    info_.reset(new history::DownloadRow(i));
  }
  void clear_info() {
    info_.reset();
  }

 private:
  static const char kKey[];

  PersistenceState state_;
  std::unique_ptr<history::DownloadRow> info_;
  bool was_restored_from_history_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHistoryData);
};

const char DownloadHistoryData::kKey[] =
  "DownloadItem DownloadHistoryData";

history::DownloadRow GetDownloadRow(
    content::DownloadItem* item) {
  std::string by_ext_id, by_ext_name;
#if defined(ENABLE_EXTENSIONS)
  extensions::DownloadedByExtension* by_ext =
      extensions::DownloadedByExtension::Get(item);
  if (by_ext) {
    by_ext_id = by_ext->id();
    by_ext_name = by_ext->name();
  }
#endif

  return history::DownloadRow(
      item->GetFullPath(), item->GetTargetFilePath(), item->GetUrlChain(),
      item->GetReferrerUrl(), item->GetSiteUrl(), item->GetTabUrl(),
      item->GetTabReferrerUrl(),
      std::string(),  // HTTP method (not available yet)
      item->GetMimeType(), item->GetOriginalMimeType(), item->GetStartTime(),
      item->GetEndTime(), item->GetETag(), item->GetLastModifiedTime(),
      item->GetReceivedBytes(), item->GetTotalBytes(),
      history::ToHistoryDownloadState(item->GetState()),
      history::ToHistoryDownloadDangerType(item->GetDangerType()),
      history::ToHistoryDownloadInterruptReason(item->GetLastReason()),
      std::string(),  // Hash value (not available yet)
      history::ToHistoryDownloadId(item->GetId()), item->GetGuid(),
      item->GetOpened(), by_ext_id, by_ext_name);
}

bool ShouldUpdateHistory(const history::DownloadRow* previous,
                         const history::DownloadRow& current) {
  // Ignore url_chain, referrer, site_url, http_method, mime_type,
  // original_mime_type, start_time, id, and guid. These fields don't change.
  return ((previous == NULL) ||
          (previous->current_path != current.current_path) ||
          (previous->target_path != current.target_path) ||
          (previous->end_time != current.end_time) ||
          (previous->received_bytes != current.received_bytes) ||
          (previous->total_bytes != current.total_bytes) ||
          (previous->etag != current.etag) ||
          (previous->last_modified != current.last_modified) ||
          (previous->state != current.state) ||
          (previous->danger_type != current.danger_type) ||
          (previous->interrupt_reason != current.interrupt_reason) ||
          (previous->hash != current.hash) ||
          (previous->opened != current.opened) ||
          (previous->by_ext_id != current.by_ext_id) ||
          (previous->by_ext_name != current.by_ext_name));
}

typedef std::vector<history::DownloadRow> InfoVector;

}  // anonymous namespace

DownloadHistory::HistoryAdapter::HistoryAdapter(
    history::HistoryService* history)
    : history_(history) {
}
DownloadHistory::HistoryAdapter::~HistoryAdapter() {}

void DownloadHistory::HistoryAdapter::QueryDownloads(
    const history::HistoryService::DownloadQueryCallback& callback) {
  history_->QueryDownloads(callback);
}

void DownloadHistory::HistoryAdapter::CreateDownload(
    const history::DownloadRow& info,
    const history::HistoryService::DownloadCreateCallback& callback) {
  history_->CreateDownload(info, callback);
}

void DownloadHistory::HistoryAdapter::UpdateDownload(
    const history::DownloadRow& data) {
  history_->UpdateDownload(data);
}

void DownloadHistory::HistoryAdapter::RemoveDownloads(
    const std::set<uint32_t>& ids) {
  history_->RemoveDownloads(ids);
}

DownloadHistory::Observer::Observer() {}
DownloadHistory::Observer::~Observer() {}

// static
bool DownloadHistory::IsPersisted(const content::DownloadItem* item) {
  const DownloadHistoryData* data = DownloadHistoryData::Get(item);
  return data && (data->state() == DownloadHistoryData::PERSISTED);
}

DownloadHistory::DownloadHistory(content::DownloadManager* manager,
                                 std::unique_ptr<HistoryAdapter> history)
    : notifier_(manager, this),
      history_(std::move(history)),
      loading_id_(content::DownloadItem::kInvalidId),
      history_size_(0),
      initial_history_query_complete_(false),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::DownloadManager::DownloadVector items;
  notifier_.GetManager()->GetAllDownloads(&items);
  for (content::DownloadManager::DownloadVector::const_iterator
       it = items.begin(); it != items.end(); ++it) {
    OnDownloadCreated(notifier_.GetManager(), *it);
  }
  history_->QueryDownloads(base::Bind(
      &DownloadHistory::QueryCallback, weak_ptr_factory_.GetWeakPtr()));
}

DownloadHistory::~DownloadHistory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadHistoryDestroyed());
  observers_.Clear();
}

void DownloadHistory::AddObserver(DownloadHistory::Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.AddObserver(observer);

  if (initial_history_query_complete_)
    observer->OnHistoryQueryComplete();
}

void DownloadHistory::RemoveObserver(DownloadHistory::Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

bool DownloadHistory::WasRestoredFromHistory(
    const content::DownloadItem* download) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const DownloadHistoryData* data = DownloadHistoryData::Get(download);

  // The OnDownloadCreated handler sets the was_restored_from_history flag when
  // resetting the loading_id_. So one of the two conditions below will hold for
  // a download restored from history even if the caller of this method is
  // racing with our OnDownloadCreated handler.
  return (data && data->was_restored_from_history()) ||
         download->GetId() == loading_id_;
}

void DownloadHistory::QueryCallback(std::unique_ptr<InfoVector> infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // ManagerGoingDown() may have happened before the history loaded.
  if (!notifier_.GetManager())
    return;
  for (InfoVector::const_iterator it = infos->begin();
       it != infos->end(); ++it) {
    loading_id_ = history::ToContentDownloadId(it->id);
    content::DownloadItem* item = notifier_.GetManager()->CreateDownloadItem(
        it->guid, loading_id_, it->current_path, it->target_path, it->url_chain,
        it->referrer_url, it->site_url, it->tab_url, it->tab_referrer_url,
        it->mime_type, it->original_mime_type, it->start_time, it->end_time,
        it->etag, it->last_modified, it->received_bytes, it->total_bytes,
        std::string(),  // TODO(asanka): Need to persist and restore hash of
                        // partial file for an interrupted download. No need to
                        // store hash for a completed file.
        history::ToContentDownloadState(it->state),
        history::ToContentDownloadDangerType(it->danger_type),
        history::ToContentDownloadInterruptReason(it->interrupt_reason),
        it->opened);
#if defined(ENABLE_EXTENSIONS)
    if (!it->by_ext_id.empty() && !it->by_ext_name.empty()) {
      new extensions::DownloadedByExtension(
          item, it->by_ext_id, it->by_ext_name);
      item->UpdateObservers();
    }
#endif
    DCHECK_EQ(DownloadHistoryData::PERSISTED,
              DownloadHistoryData::Get(item)->state());
    ++history_size_;
  }
  notifier_.GetManager()->CheckForHistoryFilesRemoval();

  initial_history_query_complete_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnHistoryQueryComplete());
}

void DownloadHistory::MaybeAddToHistory(content::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uint32_t download_id = item->GetId();
  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  bool removing = removing_ids_.find(download_id) != removing_ids_.end();

  // TODO(benjhayden): Remove IsTemporary().
  if (download_crx_util::IsExtensionDownload(*item) ||
      item->IsTemporary() ||
      (data->state() != DownloadHistoryData::NOT_PERSISTED) ||
      removing)
    return;

  data->SetState(DownloadHistoryData::PERSISTING);
  if (data->info() == NULL) {
    // Keep the info here regardless of whether the item is in progress so that,
    // when ItemAdded() calls OnDownloadUpdated(), it can decide whether to
    // Update the db and/or clear the info.
    data->set_info(GetDownloadRow(item));
  }

  history_->CreateDownload(*data->info(), base::Bind(
      &DownloadHistory::ItemAdded, weak_ptr_factory_.GetWeakPtr(),
      download_id));
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadStored(
      item, *data->info()));
}

void DownloadHistory::ItemAdded(uint32_t download_id, bool success) {
  if (removed_while_adding_.find(download_id) !=
      removed_while_adding_.end()) {
    removed_while_adding_.erase(download_id);
    if (success)
      ScheduleRemoveDownload(download_id);
    return;
  }

  if (!notifier_.GetManager())
    return;

  content::DownloadItem* item = notifier_.GetManager()->GetDownload(
      download_id);
  if (!item) {
    // This item will have called OnDownloadDestroyed().  If the item should
    // have been removed from history, then it would have also called
    // OnDownloadRemoved(), which would have put |download_id| in
    // removed_while_adding_, handled above.
    return;
  }

  DownloadHistoryData* data = DownloadHistoryData::Get(item);

  // The sql INSERT statement failed. Avoid an infinite loop: don't
  // automatically retry. Retry adding the next time the item is updated by
  // resetting the state to NOT_PERSISTED.
  if (!success) {
    DVLOG(20) << __FUNCTION__ << " INSERT failed id=" << download_id;
    data->SetState(DownloadHistoryData::NOT_PERSISTED);
    return;
  }
  data->SetState(DownloadHistoryData::PERSISTED);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.HistorySize2",
                              history_size_,
                              0/*min*/,
                              (1 << 23)/*max*/,
                              (1 << 7)/*num_buckets*/);
  ++history_size_;

  // In case the item changed or became temporary while it was being added.
  // Don't just update all of the item's observers because we're the only
  // observer that can also see data->state(), which is the only thing that
  // ItemAdded() changed.
  OnDownloadUpdated(notifier_.GetManager(), item);
}

void DownloadHistory::OnDownloadCreated(
    content::DownloadManager* manager, content::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // All downloads should pass through OnDownloadCreated exactly once.
  CHECK(!DownloadHistoryData::Get(item));
  DownloadHistoryData* data = new DownloadHistoryData(item);
  if (item->GetId() == loading_id_) {
    data->SetState(DownloadHistoryData::PERSISTED);
    data->set_was_restored_from_history(true);
    loading_id_ = content::DownloadItem::kInvalidId;
  }
  if (item->GetState() == content::DownloadItem::IN_PROGRESS) {
    data->set_info(GetDownloadRow(item));
  }
  MaybeAddToHistory(item);
}

void DownloadHistory::OnDownloadUpdated(
    content::DownloadManager* manager, content::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  if (data->state() == DownloadHistoryData::NOT_PERSISTED) {
    MaybeAddToHistory(item);
    return;
  }
  if (item->IsTemporary()) {
    OnDownloadRemoved(notifier_.GetManager(), item);
    return;
  }

  history::DownloadRow current_info(GetDownloadRow(item));
  bool should_update = ShouldUpdateHistory(data->info(), current_info);
  UMA_HISTOGRAM_ENUMERATION("Download.HistoryPropagatedUpdate",
                            should_update, 2);
  if (should_update) {
    history_->UpdateDownload(current_info);
    FOR_EACH_OBSERVER(Observer, observers_, OnDownloadStored(
        item, current_info));
  }
  if (item->GetState() == content::DownloadItem::IN_PROGRESS) {
    data->set_info(current_info);
  } else {
    data->clear_info();
  }
}

void DownloadHistory::OnDownloadOpened(
    content::DownloadManager* manager, content::DownloadItem* item) {
  OnDownloadUpdated(manager, item);
}

void DownloadHistory::OnDownloadRemoved(
    content::DownloadManager* manager, content::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  if (data->state() != DownloadHistoryData::PERSISTED) {
    if (data->state() == DownloadHistoryData::PERSISTING) {
      // ScheduleRemoveDownload will be called when history_ calls ItemAdded().
      removed_while_adding_.insert(item->GetId());
    }
    return;
  }
  ScheduleRemoveDownload(item->GetId());
  // This is important: another OnDownloadRemoved() handler could do something
  // that synchronously fires an OnDownloadUpdated().
  data->SetState(DownloadHistoryData::NOT_PERSISTED);
  // ItemAdded increments history_size_ only if the item wasn't
  // removed_while_adding_, so the next line does not belong in
  // ScheduleRemoveDownload().
  --history_size_;
}

void DownloadHistory::ScheduleRemoveDownload(uint32_t download_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // For database efficiency, batch removals together if they happen all at
  // once.
  if (removing_ids_.empty()) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadHistory::RemoveDownloadsBatch,
                   weak_ptr_factory_.GetWeakPtr()));
  }
  removing_ids_.insert(download_id);
}

void DownloadHistory::RemoveDownloadsBatch() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  IdSet remove_ids;
  removing_ids_.swap(remove_ids);
  history_->RemoveDownloads(remove_ids);
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadsRemoved(remove_ids));
}
