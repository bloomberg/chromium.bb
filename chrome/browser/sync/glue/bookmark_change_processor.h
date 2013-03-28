// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BOOKMARK_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_BOOKMARK_CHANGE_PROCESSOR_H_

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"

namespace base {
class RefCountedMemory;
}

namespace syncer {
class WriteNode;
class WriteTransaction;
}  // namespace syncer

namespace browser_sync {

extern const char kBookmarkTransactionVersionKey[];

// This class is responsible for taking changes from the BookmarkModel
// and applying them to the sync API 'syncable' model, and vice versa.
// All operations and use of this class are from the UI thread.
// This is currently bookmarks specific.
class BookmarkChangeProcessor : public BookmarkModelObserver,
                                public ChangeProcessor {
 public:
  BookmarkChangeProcessor(BookmarkModelAssociator* model_associator,
                          DataTypeErrorHandler* error_handler);
  virtual ~BookmarkChangeProcessor();

  // BookmarkModelObserver implementation.
  // BookmarkModel -> sync API model change application.
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;

  // The change processor implementation, responsible for applying changes from
  // the sync model to the bookmarks model.
  virtual void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) OVERRIDE;

  // Create a bookmark node corresponding to |src| if one is not already
  // associated with |src|.  Returns the node that was created or updated.
  static const BookmarkNode* CreateOrUpdateBookmarkNode(
      syncer::BaseNode* src,
      BookmarkModel* model,
      Profile* profile,
      BookmarkModelAssociator* model_associator);

  // The following methods are static and hence may be invoked at any time,
  // and do not depend on having a running ChangeProcessor.
  // Creates a bookmark node under the given parent node from the given sync
  // node. Returns the newly created node.
  static const BookmarkNode* CreateBookmarkNode(
      syncer::BaseNode* sync_node,
      const BookmarkNode* parent,
      BookmarkModel* model,
      Profile* profile,
      int index);

  // Sets the favicon of the given bookmark node from the given sync node.
  // Returns whether the favicon was set in the bookmark node.
  // |profile| is the profile that contains the HistoryService and BookmarkModel
  // for the bookmark in question.
  static bool SetBookmarkFavicon(syncer::BaseNode* sync_node,
                                 const BookmarkNode* bookmark_node,
                                 BookmarkModel* model,
                                 Profile* profile);

  // Applies the 1x favicon |bitmap_data| and |icon_url| to |bookmark_node|.
  // |profile| is the profile that contains the HistoryService and BookmarkModel
  // for the bookmark in question.
  static void ApplyBookmarkFavicon(
      const BookmarkNode* bookmark_node,
      Profile* profile,
      const GURL& icon_url,
      const scoped_refptr<base::RefCountedMemory>& bitmap_data);

  // Sets the favicon of the given sync node from the given bookmark node.
  static void SetSyncNodeFavicon(const BookmarkNode* bookmark_node,
                                 BookmarkModel* model,
                                 syncer::WriteNode* sync_node);

  // Treat the |index|th child of |parent| as a newly added node, and create a
  // corresponding node in the sync domain using |trans|.  All properties
  // will be transferred to the new node.  A node corresponding to |parent|
  // must already exist and be associated for this call to succeed.  Returns
  // the ID of the just-created node, or if creation fails, kInvalidID.
  static int64 CreateSyncNode(const BookmarkNode* parent,
                              BookmarkModel* model,
                              int index,
                              syncer::WriteTransaction* trans,
                              BookmarkModelAssociator* associator,
                              DataTypeErrorHandler* error_handler);

  // Update transaction version of |model| and |nodes| to |new_version| if
  // it's valid.
  static void UpdateTransactionVersion(
      int64 new_version,
      BookmarkModel* model,
      const std::vector<const BookmarkNode*>& nodes);

 protected:
  virtual void StartImpl(Profile* profile) OVERRIDE;

 private:
  enum MoveOrCreate {
    MOVE,
    CREATE,
  };

  // Helper function to determine the appropriate insertion index of sync node
  // |node| under the Bookmark model node |parent|, to make the positions
  // match up between the two models. This presumes that the predecessor of the
  // item (in the bookmark model) has already been moved into its appropriate
  // position.
  static int CalculateBookmarkModelInsertionIndex(
      const BookmarkNode* parent,
      const syncer::BaseNode* node,
      BookmarkModelAssociator* model_associator);

  // Helper function used to fix the position of a sync node so that it matches
  // the position of a corresponding bookmark model node. |parent| and
  // |index| identify the bookmark model position.  |dst| is the node whose
  // position is to be fixed.  If |operation| is CREATE, treat |dst| as an
  // uncreated node and set its position via InitByCreation(); otherwise,
  // |dst| is treated as an existing node, and its position will be set via
  // SetPosition().  |trans| is the transaction to which |dst| belongs. Returns
  // false on failure.
  static bool PlaceSyncNode(MoveOrCreate operation,
                            const BookmarkNode* parent,
                            int index,
                            syncer::WriteTransaction* trans,
                            syncer::WriteNode* dst,
                            BookmarkModelAssociator* associator);

  // Copy properties (but not position) from |src| to |dst|.
  static void UpdateSyncNodeProperties(const BookmarkNode* src,
                                       BookmarkModel* model,
                                       syncer::WriteNode* dst);

  // Helper function to encode a bookmark's favicon into raw PNG data.
  static void EncodeFavicon(const BookmarkNode* src,
                            BookmarkModel* model,
                            scoped_refptr<base::RefCountedMemory>* dst);

  // Remove the sync node corresponding to |node|.  It shouldn't have
  // any children.
  void RemoveOneSyncNode(syncer::WriteTransaction* trans,
                         const BookmarkNode* node);

  // Remove all the sync nodes associated with |node| and its children.
  void RemoveSyncNodeHierarchy(const BookmarkNode* node);

  // The bookmark model we are processing changes from.  Non-NULL when
  // |running_| is true.
  BookmarkModel* bookmark_model_;

  Profile* profile_;

  // The two models should be associated according to this ModelAssociator.
  BookmarkModelAssociator* model_associator_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BOOKMARK_CHANGE_PROCESSOR_H_
