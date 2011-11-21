// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
#pragma once

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_index.h"
#include "chrome/common/important_file_writer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class BookmarkModel;
class BookmarkNode;
class Profile;

// BookmarkLoadDetails is used by BookmarkStorage when loading bookmarks.
// BookmarkModel creates a BookmarkLoadDetails and passes it (including
// ownership) to BookmarkStorage. BoomarkStorage loads the bookmarks (and index)
// in the background thread, then calls back to the BookmarkModel (on the main
// thread) when loading is done, passing ownership back to the BookmarkModel.
// While loading BookmarkModel does not maintain references to the contents
// of the BookmarkLoadDetails, this ensures we don't have any threading
// problems.
class BookmarkLoadDetails {
 public:
  BookmarkLoadDetails(BookmarkNode* bb_node,
                      BookmarkNode* other_folder_node,
                      BookmarkNode* synced_folder_node,
                      BookmarkIndex* index,
                      int64 max_id);
  ~BookmarkLoadDetails();

  BookmarkNode* bb_node() { return bb_node_.get(); }
  BookmarkNode* release_bb_node() { return bb_node_.release(); }
  BookmarkNode* synced_folder_node() { return synced_folder_node_.get(); }
  BookmarkNode* release_synced_folder_node() {
    return synced_folder_node_.release();
  }
  BookmarkNode* other_folder_node() { return other_folder_node_.get(); }
  BookmarkNode* release_other_folder_node() {
    return other_folder_node_.release();
  }
  BookmarkIndex* index() { return index_.get(); }
  BookmarkIndex* release_index() { return index_.release(); }

  // Max id of the nodes.
  void set_max_id(int64 max_id) { max_id_ = max_id; }
  int64 max_id() const { return max_id_; }

  // Computed checksum.
  void set_computed_checksum(const std::string& value) {
    computed_checksum_ = value;
  }
  const std::string& computed_checksum() const { return computed_checksum_; }

  // Stored checksum.
  void set_stored_checksum(const std::string& value) {
    stored_checksum_ = value;
  }
  const std::string& stored_checksum() const { return stored_checksum_; }

  // Whether ids were reassigned. IDs are reassigned during decoding if the
  // checksum of the file doesn't match, some IDs are missing or not
  // unique. Basically, if the user modified the bookmarks directly we'll
  // reassign the ids to ensure they are unique.
  void set_ids_reassigned(bool value) { ids_reassigned_ = value; }
  bool ids_reassigned() const { return ids_reassigned_; }

 private:
  scoped_ptr<BookmarkNode> bb_node_;
  scoped_ptr<BookmarkNode> other_folder_node_;
  scoped_ptr<BookmarkNode> synced_folder_node_;
  scoped_ptr<BookmarkIndex> index_;
  int64 max_id_;
  std::string computed_checksum_;
  std::string stored_checksum_;
  bool ids_reassigned_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkLoadDetails);
};

// BookmarkStorage handles reading/write the bookmark bar model. The
// BookmarkModel uses the BookmarkStorage to load bookmarks from disk, as well
// as notifying the BookmarkStorage every time the model changes.
//
// Internally BookmarkStorage uses BookmarkCodec to do the actual read/write.
class BookmarkStorage : public content::NotificationObserver,
                        public ImportantFileWriter::DataSerializer,
                        public base::RefCountedThreadSafe<BookmarkStorage> {
 public:
  // Creates a BookmarkStorage for the specified model
  BookmarkStorage(Profile* profile, BookmarkModel* model);

  // Loads the bookmarks into the model, notifying the model when done. This
  // takes ownership of |details|. See BookmarkLoadDetails for details.
  void LoadBookmarks(BookmarkLoadDetails* details);

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // Notification the bookmark bar model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void BookmarkModelDeleted();

  // ImportantFileWriter::DataSerializer
  virtual bool SerializeData(std::string* output) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<BookmarkStorage>;

  virtual ~BookmarkStorage();

  class LoadTask;

  // Callback from backend with the results of the bookmark file.
  // This may be called multiple times, with different paths. This happens when
  // we migrate bookmark data from database.
  void OnLoadFinished(bool file_exists,
                      const FilePath& path);

  // Loads bookmark data from |file| and notifies the model when finished.
  void DoLoadBookmarks(const FilePath& file);

  // Load bookmarks data from the file written by history (StarredURLDatabase).
  void MigrateFromHistory();

  // Called when history has written the file with bookmarks data. Loads data
  // from that file.
  void OnHistoryFinishedWriting();

  // Called after we loaded file generated by history. Saves the data, deletes
  // the temporary file, and notifies the model.
  void FinishHistoryMigration();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  // Keep the pointer to profile, we may need it for migration from history.
  Profile* profile_;

  // The model. The model is NULL once BookmarkModelDeleted has been invoked.
  BookmarkModel* model_;

  // Helper to write bookmark data safely.
  ImportantFileWriter writer_;

  // Helper to ensure that we unregister from notifications on destruction.
  content::NotificationRegistrar notification_registrar_;

  // Path to temporary file created during migrating bookmarks from history.
  const FilePath tmp_history_path_;

  // See class description of BookmarkLoadDetails for details on this.
  scoped_ptr<BookmarkLoadDetails> details_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkStorage);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
