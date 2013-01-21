// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_

#include "base/files/important_file_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class BookmarkIndex;
class BookmarkModel;
class BookmarkPermanentNode;

namespace base {
class SequencedTaskRunner;
}

namespace content {
class BrowserContext;
}

// BookmarkLoadDetails is used by BookmarkStorage when loading bookmarks.
// BookmarkModel creates a BookmarkLoadDetails and passes it (including
// ownership) to BookmarkStorage. BookmarkStorage loads the bookmarks (and
// index) in the background thread, then calls back to the BookmarkModel (on
// the main thread) when loading is done, passing ownership back to the
// BookmarkModel. While loading BookmarkModel does not maintain references to
// the contents of the BookmarkLoadDetails, this ensures we don't have any
// threading problems.
class BookmarkLoadDetails {
 public:
  BookmarkLoadDetails(BookmarkPermanentNode* bb_node,
                      BookmarkPermanentNode* other_folder_node,
                      BookmarkPermanentNode* mobile_folder_node,
                      BookmarkIndex* index,
                      int64 max_id);
  ~BookmarkLoadDetails();

  BookmarkPermanentNode* bb_node() { return bb_node_.get(); }
  BookmarkPermanentNode* release_bb_node() { return bb_node_.release(); }
  BookmarkPermanentNode* mobile_folder_node() {
    return mobile_folder_node_.get();
  }
  BookmarkPermanentNode* release_mobile_folder_node() {
    return mobile_folder_node_.release();
  }
  BookmarkPermanentNode* other_folder_node() {
    return other_folder_node_.get();
  }
  BookmarkPermanentNode* release_other_folder_node() {
    return other_folder_node_.release();
  }
  BookmarkIndex* index() { return index_.get(); }
  BookmarkIndex* release_index() { return index_.release(); }

  const std::string& model_meta_info() { return model_meta_info_; }
  void set_model_meta_info(const std::string& meta_info) {
    model_meta_info_ = meta_info;
  }

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
  scoped_ptr<BookmarkPermanentNode> bb_node_;
  scoped_ptr<BookmarkPermanentNode> other_folder_node_;
  scoped_ptr<BookmarkPermanentNode> mobile_folder_node_;
  scoped_ptr<BookmarkIndex> index_;
  std::string model_meta_info_;
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
class BookmarkStorage : public base::ImportantFileWriter::DataSerializer,
                        public base::RefCountedThreadSafe<BookmarkStorage> {
 public:
  // Creates a BookmarkStorage for the specified model
  BookmarkStorage(content::BrowserContext* context,
                  BookmarkModel* model,
                  base::SequencedTaskRunner* sequenced_task_runner);

  // Loads the bookmarks into the model, notifying the model when done. This
  // takes ownership of |details|. See BookmarkLoadDetails for details.
  void LoadBookmarks(BookmarkLoadDetails* details);

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // Notification the bookmark bar model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void BookmarkModelDeleted();

  // Callback from backend after loading the bookmark file.
  void OnLoadFinished();

  // ImportantFileWriter::DataSerializer implementation.
  virtual bool SerializeData(std::string* output) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<BookmarkStorage>;

  virtual ~BookmarkStorage();

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  // The model. The model is NULL once BookmarkModelDeleted has been invoked.
  BookmarkModel* model_;

  // Helper to write bookmark data safely.
  base::ImportantFileWriter writer_;

  // See class description of BookmarkLoadDetails for details on this.
  scoped_ptr<BookmarkLoadDetails> details_;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkStorage);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_STORAGE_H_
