// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_H_
#define COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model_observer.h"

class GURL;
class ReadingListEntry;
class ReadingListModel;
class ScopedReadingListBatchUpdate;

namespace syncer {
class ModelTypeSyncBridge;
}

// The reading list model contains two list of entries: one of unread urls, the
// other of read ones. This object should only be accessed from one thread
// (Usually the main thread). The observers callbacks are also sent on the main
// thread.
class ReadingListModel : public base::NonThreadSafe {
 public:
  class ScopedReadingListBatchUpdate;

  // Returns true if the model finished loading. Until this returns true the
  // reading list is not ready for use.
  virtual bool loaded() const = 0;

  // Returns true if the model is performing batch updates right now.
  bool IsPerformingBatchUpdates() const;

  // Returns the ModelTypeSyncBridge responsible for handling sync message.
  virtual syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() = 0;

  // Tells model to prepare for batch updates.
  // This method is reentrant, i.e. several batch updates may take place at the
  // same time.
  // Returns a scoped batch update object that should be retained while the
  // batch update is performed. Deallocating this object will inform model that
  // the batch update has completed.
  std::unique_ptr<ScopedReadingListBatchUpdate> BeginBatchUpdates();

  // Creates a batch token that will freeze the model while in scope.
  virtual std::unique_ptr<ScopedReadingListBatchUpdate> CreateBatchToken();

  // Returns the size of read and unread entries.
  virtual size_t unread_size() const = 0;
  virtual size_t read_size() const = 0;

  // Returns true if there are entries in the model that were not seen by the
  // user yet. Reset to true when new unread entries are added. Reset to false
  // when ResetUnseenEntries is called.
  virtual bool HasUnseenEntries() const = 0;
  virtual void ResetUnseenEntries() = 0;

  // TODO(659099): Remove methods.
  // Returns a specific entry.
  virtual const ReadingListEntry& GetUnreadEntryAtIndex(size_t index) const = 0;
  virtual const ReadingListEntry& GetReadEntryAtIndex(size_t index) const = 0;

  // Returns a specific entry. Returns null if the entry does not exist.
  // If |read| is not null and the entry is found, |*read| is the read status
  // of the entry.
  virtual const ReadingListEntry* GetEntryFromURL(const GURL& gurl,
                                                  bool* read) const = 0;

  // Synchronously calls the |callback| with entry associated with this |url|.
  // Does nothing if there is no entry associated.
  // Returns whether the callback has been called.
  virtual bool CallbackEntryURL(
      const GURL& url,
      base::Callback<void(const ReadingListEntry&)> callback) const = 0;

  // Adds |url| at the top of the unread entries, and removes entries with the
  // same |url| from everywhere else if they exist. The entry title will be a
  // trimmed copy of |title.
  // The addition may be asynchronous, and the data will be available only once
  // the observers are notified.
  virtual const ReadingListEntry& AddEntry(const GURL& url,
                                           const std::string& title) = 0;

  // Removes an entry. The removal may be asynchronous, and not happen
  // immediately.
  virtual void RemoveEntryByURL(const GURL& url) = 0;

  // If the |url| is in the reading list and unread, mark it read. If it is in
  // the reading list and read, move it to the top of unread if it is not here
  // already. This may trigger deletion of old read entries.
  virtual void MarkReadByURL(const GURL& url) = 0;
  // If the |url| is in the reading list and read, mark it unread. If it is in
  // the reading list and unread, move it to the top of read if it is not here
  // already.
  virtual void MarkUnreadByURL(const GURL& url) = 0;

  // Methods to mutate an entry. Will locate the relevant entry by URL. Does
  // nothing if the entry is not found.
  virtual void SetEntryTitle(const GURL& url, const std::string& title) = 0;
  virtual void SetEntryDistilledPath(const GURL& url,
                                     const base::FilePath& distilled_path) = 0;
  virtual void SetEntryDistilledState(
      const GURL& url,
      ReadingListEntry::DistillationState state) = 0;

  // Observer registration methods. The model will remove all observers upon
  // destruction automatically.
  void AddObserver(ReadingListModelObserver* observer);
  void RemoveObserver(ReadingListModelObserver* observer);

  // Helper class that is used to scope batch updates.
  class ScopedReadingListBatchUpdate {
   public:
    explicit ScopedReadingListBatchUpdate(ReadingListModel* model)
        : model_(model) {}

    virtual ~ScopedReadingListBatchUpdate();

   private:
    ReadingListModel* model_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadingListBatchUpdate);
  };

 protected:
  ReadingListModel();
  virtual ~ReadingListModel();

  // The observers.
  base::ObserverList<ReadingListModelObserver> observers_;

  // Tells model that batch updates have completed. Called from
  // ReadingListBatchUpdateToken dtor.
  virtual void EndBatchUpdates();

  // Called when model is entering batch update mode.
  virtual void EnteringBatchUpdates();

  // Called when model is leaving batch update mode.
  virtual void LeavingBatchUpdates();

 private:
  unsigned int current_batch_updates_count_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListModel);
};

#endif  // COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_H_
