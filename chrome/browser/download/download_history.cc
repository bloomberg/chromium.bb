// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DownloadHistory manages persisting DownloadItems to the history service by
// observing a single DownloadManager and all its DownloadItems using an
// AllDownloadItemNotifier.
//
// DownloadHistory decides whether and when to add items to, remove items from,
// and update items in the database. DownloadHistory uses DownloadHistoryData to
// store per-DownloadItem data such as its db_handle, whether the item is being
// added and waiting for its db_handle, and the last history::DownloadRow
// that was passed to the database.  When the DownloadManager and its delegate
// (ChromeDownloadManagerDelegate) are initialized, DownloadHistory is created
// and queries the HistoryService. When the HistoryService calls back from
// QueryDownloads() to QueryCallback(), DownloadHistory uses
// DownloadManager::CreateDownloadItem() to inform DownloadManager of these
// persisted DownloadItems. CreateDownloadItem() internally calls
// OnDownloadCreated(), which normally adds items to the database, so
// QueryCallback() uses |loading_db_handle_| to disable adding these items to
// the database as it matches them up with their db_handles.  If a download is
// removed via OnDownloadRemoved() while the item is still being added to the
// database, DownloadHistory uses |removed_while_adding_| to remember to remove
// the item when its ItemAdded() callback is called.  All callbacks are bound
// with a weak pointer to DownloadHistory to prevent use-after-free bugs.
// ChromeDownloadManagerDelegate owns DownloadHistory, and deletes it in
// Shutdown(), which is called by DownloadManagerImpl::Shutdown() after all
// DownloadItems are destroyed.

#include "chrome/browser/download/download_history.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/history/download_database.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

namespace {

// Per-DownloadItem data. This information does not belong inside DownloadItem,
// and keeping maps in DownloadHistory from DownloadItem to this information is
// error-prone and complicated. Unfortunately, DownloadHistory::removing_*_ and
// removed_while_adding_ cannot be moved into this class partly because
// DownloadHistoryData is destroyed when DownloadItems are destroyed, and we
// have no control over when DownloadItems are destroyed.
class DownloadHistoryData : public base::SupportsUserData::Data {
 public:
  static DownloadHistoryData* Get(content::DownloadItem* item) {
    base::SupportsUserData::Data* data = item->GetUserData(kKey);
    return (data == NULL) ? NULL :
      static_cast<DownloadHistoryData*>(data);
  }

  DownloadHistoryData(content::DownloadItem* item, int64 handle)
    : is_adding_(false),
      db_handle_(handle),
      info_(NULL) {
    item->SetUserData(kKey, this);
  }

  virtual ~DownloadHistoryData() {
  }

  // Whether this item is currently being added to the database.
  bool is_adding() const { return is_adding_; }
  void set_is_adding(bool a) { is_adding_ = a; }

  // Whether this item is already persisted in the database.
  bool is_persisted() const {
    return db_handle_ != history::DownloadDatabase::kUninitializedHandle;
  }

  int64 db_handle() const { return db_handle_; }
  void set_db_handle(int64 h) { db_handle_ = h; }

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

  bool is_adding_;
  int64 db_handle_;
  scoped_ptr<history::DownloadRow> info_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHistoryData);
};

const char DownloadHistoryData::kKey[] =
  "DownloadItem DownloadHistoryData";

history::DownloadRow GetDownloadRow(
    content::DownloadItem* item) {
  // TODO(asanka): Persist GetTargetFilePath() as well.
  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  return history::DownloadRow(
      item->GetFullPath(),
      item->GetTargetFilePath(),
      item->GetUrlChain(),
      item->GetReferrerUrl(),
      item->GetStartTime(),
      item->GetEndTime(),
      item->GetReceivedBytes(),
      item->GetTotalBytes(),
      item->GetState(),
      item->GetDangerType(),
      item->GetLastReason(),
      ((data != NULL) ? data->db_handle()
       : history::DownloadDatabase::kUninitializedHandle),
      item->GetOpened());
}

bool ShouldUpdateHistory(const history::DownloadRow* previous,
                         const history::DownloadRow& current) {
  // Ignore url, referrer, start_time, db_handle, which don't change.
  return ((previous == NULL) ||
          (previous->current_path != current.current_path) ||
          (previous->target_path != current.target_path) ||
          (previous->end_time != current.end_time) ||
          (previous->received_bytes != current.received_bytes) ||
          (previous->total_bytes != current.total_bytes) ||
          (previous->state != current.state) ||
          (previous->danger_type != current.danger_type) ||
          (previous->interrupt_reason != current.interrupt_reason) ||
          (previous->opened != current.opened));
}

typedef std::vector<history::DownloadRow> InfoVector;

}  // anonymous namespace

DownloadHistory::HistoryAdapter::HistoryAdapter(HistoryService* history)
  : history_(history) {
}
DownloadHistory::HistoryAdapter::~HistoryAdapter() {}

void DownloadHistory::HistoryAdapter::QueryDownloads(
    const HistoryService::DownloadQueryCallback& callback) {
  history_->QueryDownloads(callback);
}

void DownloadHistory::HistoryAdapter::CreateDownload(
    const history::DownloadRow& info,
    const HistoryService::DownloadCreateCallback& callback) {
  history_->CreateDownload(info, callback);
}

void DownloadHistory::HistoryAdapter::UpdateDownload(
    const history::DownloadRow& data) {
  history_->UpdateDownload(data);
}

void DownloadHistory::HistoryAdapter::RemoveDownloads(
    const std::set<int64>& db_handles) {
  history_->RemoveDownloads(db_handles);
}


DownloadHistory::Observer::Observer() {}
DownloadHistory::Observer::~Observer() {}

bool DownloadHistory::IsPersisted(content::DownloadItem* item) {
  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  return data && data->is_persisted();
}

DownloadHistory::DownloadHistory(content::DownloadManager* manager,
                                 scoped_ptr<HistoryAdapter> history)
  : notifier_(manager, this),
    history_(history.Pass()),
    loading_db_handle_(history::DownloadDatabase::kUninitializedHandle),
    history_size_(0),
    weak_ptr_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadHistoryDestroyed());
  observers_.Clear();
}

void DownloadHistory::AddObserver(DownloadHistory::Observer* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  observers_.AddObserver(observer);
}

void DownloadHistory::RemoveObserver(DownloadHistory::Observer* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void DownloadHistory::QueryCallback(scoped_ptr<InfoVector> infos) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // ManagerGoingDown() may have happened before the history loaded.
  if (!notifier_.GetManager())
    return;
  for (InfoVector::const_iterator it = infos->begin();
       it != infos->end(); ++it) {
    // OnDownloadCreated() is called inside DM::CreateDownloadItem(), so set
    // loading_db_handle_ to match up the created item with its db_handle.  All
    // methods run on the UI thread and CreateDownloadItem() is synchronous.
    loading_db_handle_ = it->db_handle;
    content::DownloadItem* download_item =
      notifier_.GetManager()->CreateDownloadItem(
        it->current_path,
        it->target_path,
        it->url_chain,
        it->referrer_url,
        it->start_time,
        it->end_time,
        it->received_bytes,
        it->total_bytes,
        it->state,
        it->danger_type,
        it->interrupt_reason,
        it->opened);
    DownloadHistoryData* data = DownloadHistoryData::Get(download_item);

    // If this DCHECK fails, then you probably added an Observer that
    // synchronously creates a DownloadItem in response to
    // DownloadManager::OnDownloadCreated(), and your observer runs before
    // DownloadHistory, and DownloadManager creates items synchronously. Just
    // bounce your DownloadItem creation off the message loop to flush
    // DownloadHistory::OnDownloadCreated.
    DCHECK_EQ(it->db_handle, data->db_handle());
    ++history_size_;
  }
  notifier_.GetManager()->CheckForHistoryFilesRemoval();
}

void DownloadHistory::MaybeAddToHistory(content::DownloadItem* item) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  int32 download_id = item->GetId();
  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  bool removing = (removing_handles_.find(data->db_handle()) !=
                   removing_handles_.end());

  // TODO(benjhayden): Remove IsTemporary().
  if (download_crx_util::IsExtensionDownload(*item) ||
      item->IsTemporary() ||
      data->is_adding() ||
      data->is_persisted() ||
      removing)
    return;

  data->set_is_adding(true);
  if (data->info() == NULL) {
    // Keep the info here regardless of whether the item is in progress so that,
    // when ItemAdded() calls OnDownloadUpdated(), it can decide whether to
    // Update the db and/or clear the info.
    data->set_info(GetDownloadRow(item));
  }

  history_->CreateDownload(*data->info(), base::Bind(
      &DownloadHistory::ItemAdded, weak_ptr_factory_.GetWeakPtr(),
      download_id));
}

void DownloadHistory::ItemAdded(int32 download_id, int64 db_handle) {
  if (removed_while_adding_.find(download_id) !=
      removed_while_adding_.end()) {
    removed_while_adding_.erase(download_id);
    ScheduleRemoveDownload(download_id, db_handle);
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
  data->set_is_adding(false);

  // The sql INSERT statement failed. Avoid an infinite loop: don't
  // automatically retry. Retry adding the next time the item is updated by
  // unsetting is_adding.
  if (db_handle == history::DownloadDatabase::kUninitializedHandle) {
    DVLOG(20) << __FUNCTION__ << " INSERT failed id=" << download_id;
    return;
  }

  data->set_db_handle(db_handle);

  // Send to observers the actual history::DownloadRow that was sent to
  // the db, plus the db_handle, instead of completely regenerating the
  // history::DownloadRow, in order to accurately reflect the contents of
  // the database.
  data->info()->db_handle = db_handle;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadStored(
      item, *data->info()));

  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.HistorySize2",
                              history_size_,
                              0/*min*/,
                              (1 << 23)/*max*/,
                              (1 << 7)/*num_buckets*/);
  ++history_size_;

  // In case the item changed or became temporary while it was being added.
  // Don't just update all of the item's observers because we're the only
  // observer that can also see db_handle, which is the only thing that
  // ItemAdded changed.
  OnDownloadUpdated(notifier_.GetManager(), item);
}

void DownloadHistory::OnDownloadCreated(
    content::DownloadManager* manager, content::DownloadItem* item) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // All downloads should pass through OnDownloadCreated exactly once.
  CHECK(!DownloadHistoryData::Get(item));
  DownloadHistoryData* data = new DownloadHistoryData(item, loading_db_handle_);
  loading_db_handle_ = history::DownloadDatabase::kUninitializedHandle;
  if (item->GetState() == content::DownloadItem::IN_PROGRESS) {
    data->set_info(GetDownloadRow(item));
  }
  MaybeAddToHistory(item);
}

void DownloadHistory::OnDownloadUpdated(
    content::DownloadManager* manager, content::DownloadItem* item) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  if (!data->is_persisted()) {
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DownloadHistoryData* data = DownloadHistoryData::Get(item);
  if (!data->is_persisted()) {
    if (data->is_adding()) {
      // ScheduleRemoveDownload will be called when history_ calls ItemAdded().
      removed_while_adding_.insert(item->GetId());
    }
    return;
  }
  ScheduleRemoveDownload(item->GetId(), data->db_handle());
  data->set_db_handle(history::DownloadDatabase::kUninitializedHandle);
  // ItemAdded increments history_size_ only if the item wasn't
  // removed_while_adding_, so the next line does not belong in
  // ScheduleRemoveDownload().
  --history_size_;
}

void DownloadHistory::ScheduleRemoveDownload(
    int32 download_id, int64 db_handle) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (db_handle == history::DownloadDatabase::kUninitializedHandle)
    return;

  // For database efficiency, batch removals together if they happen all at
  // once.
  if (removing_handles_.empty()) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadHistory::RemoveDownloadsBatch,
                   weak_ptr_factory_.GetWeakPtr()));
  }
  removing_handles_.insert(db_handle);
  removing_ids_.insert(download_id);
}

void DownloadHistory::RemoveDownloadsBatch() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  HandleSet remove_handles;
  IdSet remove_ids;
  removing_handles_.swap(remove_handles);
  removing_ids_.swap(remove_ids);
  history_->RemoveDownloads(remove_handles);
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadsRemoved(remove_ids));
}
