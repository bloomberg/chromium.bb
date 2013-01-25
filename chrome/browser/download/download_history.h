// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/download/all_download_item_notifier.h"
#include "chrome/browser/history/history_service.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

namespace history {
struct DownloadRow;
}  // namespace history

// Observes a single DownloadManager and all its DownloadItems, keeping the
// DownloadDatabase up to date.
class DownloadHistory : public AllDownloadItemNotifier::Observer {
 public:
  typedef std::set<int32> IdSet;

  // Caller must guarantee that HistoryService outlives HistoryAdapter.
  class HistoryAdapter {
   public:
    explicit HistoryAdapter(HistoryService* history);
    virtual ~HistoryAdapter();

    virtual void QueryDownloads(
        const HistoryService::DownloadQueryCallback& callback);

    virtual void CreateDownload(
        const history::DownloadRow& info,
        const HistoryService::DownloadCreateCallback& callback);

    virtual void UpdateDownload(const history::DownloadRow& data);

    virtual void RemoveDownloads(const std::set<int64>& db_handles);

   private:
    HistoryService* history_;
    DISALLOW_COPY_AND_ASSIGN(HistoryAdapter);
  };

  class Observer {
   public:
    Observer();
    virtual ~Observer();

    // Fires when a download is added to or updated in the database. When
    // downloads are first added, this fires after the callback from the
    // database so that |info| includes the |db_handle|. When downloads are
    // updated, this fires right after the message is sent to the database.
    // |info| always includes the |db_handle|.
    virtual void OnDownloadStored(content::DownloadItem* item,
                                  const history::DownloadRow& info) {
    }

    // Fires when RemoveDownloads messages are sent to the DB thread.
    virtual void OnDownloadsRemoved(const IdSet& ids) {}

    // Fires when the DownloadHistory is being destroyed so that implementors
    // can RemoveObserver() and nullify their DownloadHistory*s.
    virtual void OnDownloadHistoryDestroyed() {}
  };

  // Returns true if the item is persisted.
  static bool IsPersisted(content::DownloadItem* item);

  // Neither |manager| nor |history| may be NULL.
  // DownloadService creates DownloadHistory some time after DownloadManager is
  // created and destroys DownloadHistory as DownloadManager is shutting down.
  DownloadHistory(
      content::DownloadManager* manager,
      scoped_ptr<HistoryAdapter> history);

  virtual ~DownloadHistory();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  typedef std::set<int64> HandleSet;
  typedef std::set<content::DownloadItem*> ItemSet;

  // Callback from |history_| containing all entries in the downloads database
  // table.
  void QueryCallback(
      scoped_ptr<std::vector<history::DownloadRow> > infos);

  // May add |item| to |history_|.
  void MaybeAddToHistory(content::DownloadItem* item);

  // Callback from |history_| when an item was successfully inserted into the
  // database.
  void ItemAdded(int32 id, int64 db_handle);

  // AllDownloadItemNotifier::Observer
  virtual void OnDownloadCreated(
      content::DownloadManager* manager, content::DownloadItem* item) OVERRIDE;
  virtual void OnDownloadUpdated(
      content::DownloadManager* manager, content::DownloadItem* item) OVERRIDE;
  virtual void OnDownloadOpened(
      content::DownloadManager* manager, content::DownloadItem* item) OVERRIDE;
  virtual void OnDownloadRemoved(
      content::DownloadManager* manager, content::DownloadItem* item) OVERRIDE;

  // Schedule a record to be removed from |history_| the next time
  // RemoveDownloadsBatch() runs. Schedule RemoveDownloadsBatch() to be run soon
  // if it isn't already scheduled.
  void ScheduleRemoveDownload(int32 download_id, int64 db_handle);

  // Removes all |removing_handles_| from |history_|.
  void RemoveDownloadsBatch();


  AllDownloadItemNotifier notifier_;

  scoped_ptr<HistoryAdapter> history_;

  // |db_handle| of the item being created in response to QueryCallback(),
  // matched up with created items in OnDownloadCreated() so that the item is
  // not re-added to the database. For items not created by QueryCallback(),
  // this is DownloadDatabase::kUninitializedHandle.
  int64 loading_db_handle_;

  // |db_handles| and |ids| of items that are scheduled for removal from
  // history, to facilitate batching removals together for database efficiency.
  HandleSet removing_handles_;
  IdSet removing_ids_;

  // |GetId()|s of items that were removed while they were being added, so that
  // they can be removed when their db_handles are received from the database.
  IdSet removed_while_adding_;

  // Count the number of items in the history for UMA.
  int64 history_size_;

  ObserverList<Observer> observers_;

  base::WeakPtrFactory<DownloadHistory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHistory);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_
