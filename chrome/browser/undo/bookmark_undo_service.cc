// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/bookmark_undo_service.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/undo/bookmark_renumber_observer.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/undo/undo_operation.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "grit/generated_resources.h"

using bookmarks::BookmarkNodeData;

namespace {

// BookmarkUndoOperation ------------------------------------------------------

// Base class for all bookmark related UndoOperations that facilitates access to
// the BookmarkUndoService.
class BookmarkUndoOperation : public UndoOperation,
                              public BookmarkRenumberObserver {
 public:
  explicit BookmarkUndoOperation(Profile* profile);
  virtual ~BookmarkUndoOperation() {}

  BookmarkModel* GetBookmarkModel() const;
  BookmarkRenumberObserver* GetUndoRenumberObserver() const;

 private:
  Profile* profile_;
};

BookmarkUndoOperation::BookmarkUndoOperation(Profile* profile)
    : profile_(profile) {
}

BookmarkModel* BookmarkUndoOperation::GetBookmarkModel() const {
  return BookmarkModelFactory::GetForProfile(profile_);
}

BookmarkRenumberObserver* BookmarkUndoOperation::GetUndoRenumberObserver()
    const {
  return BookmarkUndoServiceFactory::GetForProfile(profile_);
}

// BookmarkAddOperation -------------------------------------------------------

// Handles the undo of the insertion of a bookmark or folder.
class BookmarkAddOperation : public BookmarkUndoOperation {
 public:
  BookmarkAddOperation(Profile* profile, const BookmarkNode* parent, int index);
  virtual ~BookmarkAddOperation() {}

  // UndoOperation:
  virtual void Undo() OVERRIDE;
  virtual int GetUndoLabelId() const OVERRIDE;
  virtual int GetRedoLabelId() const OVERRIDE;

  // BookmarkRenumberObserver:
  virtual void OnBookmarkRenumbered(int64 old_id, int64 new_id) OVERRIDE;

 private:
  int64 parent_id_;
  const int index_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAddOperation);
};

BookmarkAddOperation::BookmarkAddOperation(Profile* profile,
                                           const BookmarkNode* parent,
                                           int index)
  : BookmarkUndoOperation(profile),
    parent_id_(parent->id()),
    index_(index) {
}

void BookmarkAddOperation::Undo() {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, parent_id_);
  DCHECK(parent);

  model->Remove(parent, index_);
}

int BookmarkAddOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_ADD;
}

int BookmarkAddOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_DELETE;
}

void BookmarkAddOperation::OnBookmarkRenumbered(int64 old_id, int64 new_id) {
  if (parent_id_ == old_id)
    parent_id_ = new_id;
}

// BookmarkRemoveOperation ----------------------------------------------------

// Handles the undo of the deletion of a bookmark node. For a bookmark folder,
// the information for all descendant bookmark nodes is maintained.
//
// The BookmarkModel allows only single bookmark node to be removed.
class BookmarkRemoveOperation : public BookmarkUndoOperation {
 public:
  BookmarkRemoveOperation(Profile* profile,
                          const BookmarkNode* parent,
                          int old_index,
                          const BookmarkNode* node);
  virtual ~BookmarkRemoveOperation() {}

  // UndoOperation:
  virtual void Undo() OVERRIDE;
  virtual int GetUndoLabelId() const OVERRIDE;
  virtual int GetRedoLabelId() const OVERRIDE;

  // BookmarkRenumberObserver:
  virtual void OnBookmarkRenumbered(int64 old_id, int64 new_id) OVERRIDE;

 private:
  void UpdateBookmarkIds(const BookmarkNodeData::Element& element,
                         const BookmarkNode* parent,
                         int index_added_at) const;

  int64 parent_id_;
  const int old_index_;
  BookmarkNodeData removed_node_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkRemoveOperation);
};

BookmarkRemoveOperation::BookmarkRemoveOperation(Profile* profile,
                                                 const BookmarkNode* parent,
                                                 int old_index,
                                                 const BookmarkNode* node)
  : BookmarkUndoOperation(profile),
    parent_id_(parent->id()),
    old_index_(old_index),
    removed_node_(node) {
}

void BookmarkRemoveOperation::Undo() {
  DCHECK(removed_node_.is_valid());
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, parent_id_);
  DCHECK(parent);

  bookmarks::CloneBookmarkNode(
      model, removed_node_.elements, parent, old_index_, false);
  UpdateBookmarkIds(removed_node_.elements[0], parent, old_index_);
}

int BookmarkRemoveOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_DELETE;
}

int BookmarkRemoveOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_ADD;
}

void BookmarkRemoveOperation::UpdateBookmarkIds(
    const BookmarkNodeData::Element& element,
    const BookmarkNode* parent,
    int index_added_at) const {
  const BookmarkNode* node = parent->GetChild(index_added_at);
  if (element.id() != node->id())
    GetUndoRenumberObserver()->OnBookmarkRenumbered(element.id(), node->id());
  if (!element.is_url) {
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      UpdateBookmarkIds(element.children[i], node, 0);
  }
}

void BookmarkRemoveOperation::OnBookmarkRenumbered(int64 old_id, int64 new_id) {
  if (parent_id_ == old_id)
    parent_id_ = new_id;
}

// BookmarkEditOperation ------------------------------------------------------

// Handles the undo of the modification of a bookmark node.
class BookmarkEditOperation : public BookmarkUndoOperation {
 public:
  BookmarkEditOperation(Profile* profile,
                        const BookmarkNode* node);
  virtual ~BookmarkEditOperation() {}

  // UndoOperation:
  virtual void Undo() OVERRIDE;
  virtual int GetUndoLabelId() const OVERRIDE;
  virtual int GetRedoLabelId() const OVERRIDE;

  // BookmarkRenumberObserver:
  virtual void OnBookmarkRenumbered(int64 old_id, int64 new_id) OVERRIDE;

 private:
  int64 node_id_;
  BookmarkNodeData original_bookmark_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkEditOperation);
};

BookmarkEditOperation::BookmarkEditOperation(Profile* profile,
                                             const BookmarkNode* node)
    : BookmarkUndoOperation(profile),
      node_id_(node->id()),
      original_bookmark_(node) {
}

void BookmarkEditOperation::Undo() {
  DCHECK(original_bookmark_.is_valid());
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, node_id_);
  DCHECK(node);

  model->SetTitle(node, original_bookmark_.elements[0].title);
  if (original_bookmark_.elements[0].is_url)
    model->SetURL(node, original_bookmark_.elements[0].url);
}

int BookmarkEditOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_EDIT;
}

int BookmarkEditOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_EDIT;
}

void BookmarkEditOperation::OnBookmarkRenumbered(int64 old_id, int64 new_id) {
  if (node_id_ == old_id)
    node_id_ = new_id;
}

// BookmarkMoveOperation ------------------------------------------------------

// Handles the undo of a bookmark being moved to a new location.
class BookmarkMoveOperation : public BookmarkUndoOperation {
 public:
  BookmarkMoveOperation(Profile* profile,
                        const BookmarkNode* old_parent,
                        int old_index,
                        const BookmarkNode* new_parent,
                        int new_index);
  virtual ~BookmarkMoveOperation() {}
  virtual int GetUndoLabelId() const OVERRIDE;
  virtual int GetRedoLabelId() const OVERRIDE;

  // UndoOperation:
  virtual void Undo() OVERRIDE;

  // BookmarkRenumberObserver:
  virtual void OnBookmarkRenumbered(int64 old_id, int64 new_id) OVERRIDE;

 private:
  int64 old_parent_id_;
  int64 new_parent_id_;
  int old_index_;
  int new_index_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMoveOperation);
};

BookmarkMoveOperation::BookmarkMoveOperation(Profile* profile,
                                             const BookmarkNode* old_parent,
                                             int old_index,
                                             const BookmarkNode* new_parent,
                                             int new_index)
    : BookmarkUndoOperation(profile),
      old_parent_id_(old_parent->id()),
      new_parent_id_(new_parent->id()),
      old_index_(old_index),
      new_index_(new_index) {
}

void BookmarkMoveOperation::Undo() {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* old_parent =
      bookmarks::GetBookmarkNodeByID(model, old_parent_id_);
  const BookmarkNode* new_parent =
      bookmarks::GetBookmarkNodeByID(model, new_parent_id_);
  DCHECK(old_parent);
  DCHECK(new_parent);

  const BookmarkNode* node = new_parent->GetChild(new_index_);
  int destination_index = old_index_;

  // If the bookmark was moved up within the same parent then the destination
  // index needs to be incremented since the old index did not account for the
  // moved bookmark.
  if (old_parent == new_parent && new_index_ < old_index_)
    ++destination_index;

  model->Move(node, old_parent, destination_index);
}

int BookmarkMoveOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_MOVE;
}

int BookmarkMoveOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_MOVE;
}

void BookmarkMoveOperation::OnBookmarkRenumbered(int64 old_id, int64 new_id) {
  if (old_parent_id_ == old_id)
    old_parent_id_ = new_id;
  if (new_parent_id_ == old_id)
    new_parent_id_ = new_id;
}

// BookmarkReorderOperation ---------------------------------------------------

// Handle the undo of reordering of bookmarks that can happen as a result of
// sorting a bookmark folder by name or the undo of that operation.  The change
// of order is not recursive so only the order of the immediate children of the
// folder need to be restored.
class BookmarkReorderOperation : public BookmarkUndoOperation {
 public:
  BookmarkReorderOperation(Profile* profile,
                           const BookmarkNode* parent);
  virtual ~BookmarkReorderOperation();

  // UndoOperation:
  virtual void Undo() OVERRIDE;
  virtual int GetUndoLabelId() const OVERRIDE;
  virtual int GetRedoLabelId() const OVERRIDE;

  // BookmarkRenumberObserver:
  virtual void OnBookmarkRenumbered(int64 old_id, int64 new_id) OVERRIDE;

 private:
  int64 parent_id_;
  std::vector<int64> ordered_bookmarks_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkReorderOperation);
};

BookmarkReorderOperation::BookmarkReorderOperation(Profile* profile,
                                                   const BookmarkNode* parent)
    : BookmarkUndoOperation(profile),
      parent_id_(parent->id()) {
  ordered_bookmarks_.resize(parent->child_count());
  for (int i = 0; i < parent->child_count(); ++i)
    ordered_bookmarks_[i] = parent->GetChild(i)->id();
}

BookmarkReorderOperation::~BookmarkReorderOperation() {
}

void BookmarkReorderOperation::Undo() {
  BookmarkModel* model = GetBookmarkModel();
  const BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, parent_id_);
  DCHECK(parent);

  std::vector<const BookmarkNode*> ordered_nodes;
  for (size_t i = 0; i < ordered_bookmarks_.size(); ++i) {
    ordered_nodes.push_back(
        bookmarks::GetBookmarkNodeByID(model, ordered_bookmarks_[i]));
  }

  model->ReorderChildren(parent, ordered_nodes);
}

int BookmarkReorderOperation::GetUndoLabelId() const {
  return IDS_BOOKMARK_BAR_UNDO_REORDER;
}

int BookmarkReorderOperation::GetRedoLabelId() const {
  return IDS_BOOKMARK_BAR_REDO_REORDER;
}

void BookmarkReorderOperation::OnBookmarkRenumbered(int64 old_id,
                                                    int64 new_id) {
  if (parent_id_ == old_id)
    parent_id_ = new_id;
  for (size_t i = 0; i < ordered_bookmarks_.size(); ++i) {
    if (ordered_bookmarks_[i] == old_id)
      ordered_bookmarks_[i] = new_id;
  }
}

}  // namespace

// BookmarkUndoService --------------------------------------------------------

BookmarkUndoService::BookmarkUndoService(Profile* profile) : profile_(profile) {
}

BookmarkUndoService::~BookmarkUndoService() {
  BookmarkModelFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void BookmarkUndoService::BookmarkModelLoaded(BookmarkModel* model,
                                              bool ids_reassigned) {
  undo_manager_.RemoveAllOperations();
}

void BookmarkUndoService::BookmarkModelBeingDeleted(BookmarkModel* model) {
  undo_manager_.RemoveAllOperations();
}

void BookmarkUndoService::BookmarkNodeMoved(BookmarkModel* model,
                                            const BookmarkNode* old_parent,
                                            int old_index,
                                            const BookmarkNode* new_parent,
                                            int new_index) {
  scoped_ptr<UndoOperation> op(new BookmarkMoveOperation(profile_,
                                                         old_parent,
                                                         old_index,
                                                         new_parent,
                                                         new_index));
  undo_manager()->AddUndoOperation(op.Pass());
}

void BookmarkUndoService::BookmarkNodeAdded(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int index) {
  scoped_ptr<UndoOperation> op(new BookmarkAddOperation(profile_,
                                                        parent,
                                                        index));
  undo_manager()->AddUndoOperation(op.Pass());
}

void BookmarkUndoService::OnWillRemoveBookmarks(BookmarkModel* model,
                                                const BookmarkNode* parent,
                                                int old_index,
                                                const BookmarkNode* node) {
  scoped_ptr<UndoOperation> op(new BookmarkRemoveOperation(profile_,
                                                           parent,
                                                           old_index,
                                                           node));
  undo_manager()->AddUndoOperation(op.Pass());
}

void BookmarkUndoService::OnWillRemoveAllUserBookmarks(BookmarkModel* model) {
  bookmarks::ScopedGroupBookmarkActions merge_removes(model);
  for (int i = 0; i < model->root_node()->child_count(); ++i) {
    const BookmarkNode* permanent_node = model->root_node()->GetChild(i);
    for (int j = permanent_node->child_count() - 1; j >= 0; --j) {
      scoped_ptr<UndoOperation> op(new BookmarkRemoveOperation(profile_,
          permanent_node, j, permanent_node->GetChild(j)));
      undo_manager()->AddUndoOperation(op.Pass());
    }
  }
}

void BookmarkUndoService::OnWillChangeBookmarkNode(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  scoped_ptr<UndoOperation> op(new BookmarkEditOperation(profile_, node));
  undo_manager()->AddUndoOperation(op.Pass());
}

void BookmarkUndoService::OnWillReorderBookmarkNode(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  scoped_ptr<UndoOperation> op(new BookmarkReorderOperation(profile_, node));
  undo_manager()->AddUndoOperation(op.Pass());
}

void BookmarkUndoService::GroupedBookmarkChangesBeginning(
    BookmarkModel* model) {
  undo_manager()->StartGroupingActions();
}

void BookmarkUndoService::GroupedBookmarkChangesEnded(BookmarkModel* model) {
  undo_manager()->EndGroupingActions();
}

void BookmarkUndoService::OnBookmarkRenumbered(int64 old_id, int64 new_id) {
  std::vector<UndoOperation*> all_operations =
      undo_manager()->GetAllUndoOperations();
  for (std::vector<UndoOperation*>::iterator it = all_operations.begin();
       it != all_operations.end(); ++it) {
    static_cast<BookmarkUndoOperation*>(*it)->OnBookmarkRenumbered(old_id,
                                                                   new_id);
  }
}
