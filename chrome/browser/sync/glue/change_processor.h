// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_H_

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"

class ProfileSyncService;

namespace browser_sync {

class UnrecoverableErrorHandler;

// This class is responsible for taking changes from the BookmarkModel
// and applying them to the sync_api 'syncable' model, and vice versa.
// All operations and use of this class are from the UI thread.
// This is currently bookmarks specific.
class ChangeProcessor : public BookmarkModelObserver,
                        public ChangeProcessingInterface {
 public:
  explicit ChangeProcessor(UnrecoverableErrorHandler* error_handler)
      : running_(false), error_handler_(error_handler),
        bookmark_model_(NULL), share_handle_(NULL), model_associator_(NULL) {}
  virtual ~ChangeProcessor() { Stop(); }

  // Call when the processor should accept changes from either provided model
  // and apply them to the other.  Both the BookmarkModel and sync_api are
  // expected to be initialized and loaded.  You must have set a valid
  // ModelAssociator and UnrecoverableErrorHandler before using this method,
  // and the two models should be associated w.r.t the ModelAssociator provided.
  // It is safe to call Start again after previously Stop()ing the processor.
  void Start(BookmarkModel* model, sync_api::UserShare* handle);

  // Call when processing changes between models is no longer safe / needed.
  void Stop();

  bool IsRunning() const { return running_; }

  // Injectors for the components required to Start the processor.
  void set_model_associator(ModelAssociator* associator) {
    model_associator_ = associator;
  }

  // BookmarkModelObserver implementation.
  // BookmarkModel -> sync_api model change application.
  virtual void Loaded(BookmarkModel* model) { NOTREACHED(); }
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node);

  // sync_api model -> BookmarkModel change application.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count);

  // The following methods are static and hence may be invoked at any time,
  // and do not depend on having a running ChangeProcessor.
  // Creates a bookmark node under the given parent node from the given sync
  // node. Returns the newly created node.
  static const BookmarkNode* CreateBookmarkNode(
      sync_api::BaseNode* sync_node,
      const BookmarkNode* parent,
      BookmarkModel* model,
      int index);

  // Sets the favicon of the given bookmark node from the given sync node.
  // Returns whether the favicon was set in the bookmark node.
  // |profile| is the profile that contains the HistoryService and BookmarkModel
  // for the bookmark in question.
  static bool SetBookmarkFavicon(sync_api::BaseNode* sync_node,
                                 const BookmarkNode* bookmark_node,
                                 Profile* profile);

  // Sets the favicon of the given sync node from the given bookmark node.
  static void SetSyncNodeFavicon(const BookmarkNode* bookmark_node,
                                 BookmarkModel* model,
                                 sync_api::WriteNode* sync_node);

  // Treat the |index|th child of |parent| as a newly added node, and create a
  // corresponding node in the sync domain using |trans|.  All properties
  // will be transferred to the new node.  A node corresponding to |parent|
  // must already exist and be associated for this call to succeed.  Returns
  // the ID of the just-created node, or if creation fails, kInvalidID.
  static int64 CreateSyncNode(const BookmarkNode* parent,
                              BookmarkModel* model,
                              int index,
                              sync_api::WriteTransaction* trans,
                              ModelAssociator* associator,
                              UnrecoverableErrorHandler* error_handler);

 private:
  enum MoveOrCreate {
    MOVE,
    CREATE,
  };

  // Create a bookmark node corresponding to |src| if one is not already
  // associated with |src|.  Returns the node that was created or updated.
  const BookmarkNode* CreateOrUpdateBookmarkNode(
      sync_api::BaseNode* src,
      BookmarkModel* model);

  // Helper function to determine the appropriate insertion index of sync node
  // |node| under the Bookmark model node |parent|, to make the positions
  // match up between the two models. This presumes that the predecessor of the
  // item (in the bookmark model) has already been moved into its appropriate
  // position.
  int CalculateBookmarkModelInsertionIndex(
      const BookmarkNode* parent,
      const sync_api::BaseNode* node) const;

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
                            sync_api::WriteTransaction* trans,
                            sync_api::WriteNode* dst,
                            ModelAssociator* associator,
                            UnrecoverableErrorHandler* error_handler);

  // Copy properties (but not position) from |src| to |dst|.
  static void UpdateSyncNodeProperties(const BookmarkNode* src,
                                       BookmarkModel* model,
                                       sync_api::WriteNode* dst);

  // Helper function to encode a bookmark's favicon into a PNG byte vector.
  static void EncodeFavicon(const BookmarkNode* src,
                            BookmarkModel* model,
                            std::vector<unsigned char>* dst);

  // Remove the sync node corresponding to |node|.  It shouldn't have
  // any children.
  void RemoveOneSyncNode(sync_api::WriteTransaction* trans,
                         const BookmarkNode* node);

  // Remove all the sync nodes associated with |node| and its children.
  void RemoveSyncNodeHierarchy(const BookmarkNode* node);

  bool running_;  // True if we have been told it is safe to process changes.
  UnrecoverableErrorHandler* error_handler_;  // Guaranteed to outlive us.

  // The two models we are processing changes between.  Non-NULL when
  // |running_| is true.
  BookmarkModel* bookmark_model_;
  sync_api::UserShare* share_handle_;

  // The two models should be associated according to this ModelAssociator.
  scoped_refptr<ModelAssociator> model_associator_;

  DISALLOW_COPY_AND_ASSIGN(ChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_H_
