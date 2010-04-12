// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_context_menu_controller_views.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_folder_editor_controller.h"
#include "chrome/browser/bookmarks/bookmark_manager.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace {

// Returns true if the specified node is of type URL, or has a descendant
// of type URL.
bool NodeHasURLs(const BookmarkNode* node) {
  if (node->is_url())
    return true;

  for (int i = 0; i < node->GetChildCount(); ++i) {
    if (NodeHasURLs(node->GetChild(i)))
      return true;
  }
  return false;
}

// SelectOnCreationHandler ----------------------------------------------------

// Used when adding a new bookmark. If a new bookmark is created it is selected
// in the bookmark manager.
class SelectOnCreationHandler : public BookmarkEditor::Handler {
 public:
  explicit SelectOnCreationHandler(Profile* profile) : profile_(profile) {
  }

  virtual void NodeCreated(const BookmarkNode* new_node) {
    BookmarkManager::SelectInTree(profile_, new_node);
  }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SelectOnCreationHandler);
};

}  // namespace

BookmarkContextMenuControllerViews::BookmarkContextMenuControllerViews(
    gfx::NativeWindow parent_window,
    BookmarkContextMenuControllerViewsDelegate* delegate,
    Profile* profile,
    PageNavigator* navigator,
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection,
    ConfigurationType configuration)
    : parent_window_(parent_window),
      delegate_(delegate),
      profile_(profile),
      navigator_(navigator),
      parent_(parent),
      selection_(selection),
      configuration_(configuration),
      model_(profile->GetBookmarkModel()) {
  DCHECK(profile_);
  DCHECK(model_->IsLoaded());
  model_->AddObserver(this);
}

BookmarkContextMenuControllerViews::~BookmarkContextMenuControllerViews() {
  if (model_)
    model_->RemoveObserver(this);
}

void BookmarkContextMenuControllerViews::BuildMenu() {
  if (configuration_ != BOOKMARK_MANAGER_ORGANIZE_MENU) {
    if (selection_.size() == 1 && selection_[0]->is_url()) {
      delegate_->AddItemWithStringId(IDS_BOOMARK_BAR_OPEN_ALL,
                                     IDS_BOOMARK_BAR_OPEN_IN_NEW_TAB);
      delegate_->AddItemWithStringId(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW,
                                     IDS_BOOMARK_BAR_OPEN_IN_NEW_WINDOW);
      delegate_->AddItemWithStringId(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO,
                                     IDS_BOOMARK_BAR_OPEN_INCOGNITO);
    } else {
      delegate_->AddItem(IDS_BOOMARK_BAR_OPEN_ALL);
      delegate_->AddItem(IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW);
      delegate_->AddItem(IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO);
    }
    delegate_->AddSeparator();
  }

  if (selection_.size() == 1 && selection_[0]->is_folder()) {
    delegate_->AddItem(IDS_BOOKMARK_BAR_RENAME_FOLDER);
  } else {
    delegate_->AddItem(IDS_BOOKMARK_BAR_EDIT);
  }

  if (configuration_ == BOOKMARK_MANAGER_TABLE ||
      configuration_ == BOOKMARK_MANAGER_TABLE_OTHER ||
      configuration_ == BOOKMARK_MANAGER_ORGANIZE_MENU ||
      configuration_ == BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER) {
    delegate_->AddItem(IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER);
  }

  delegate_->AddSeparator();
  delegate_->AddItem(IDS_CUT);
  delegate_->AddItem(IDS_COPY);
  delegate_->AddItem(IDS_PASTE);

  delegate_->AddSeparator();
  delegate_->AddItem(IDS_BOOKMARK_BAR_REMOVE);

  if (configuration_ == BOOKMARK_MANAGER_ORGANIZE_MENU) {
    delegate_->AddSeparator();
    delegate_->AddItem(IDS_BOOKMARK_MANAGER_SORT);
  }

  delegate_->AddSeparator();

  delegate_->AddItem(IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK);
  delegate_->AddItem(IDS_BOOMARK_BAR_NEW_FOLDER);

  if (configuration_ == BOOKMARK_BAR) {
    delegate_->AddSeparator();
    delegate_->AddItem(IDS_BOOKMARK_MANAGER);
    delegate_->AddCheckboxItem(IDS_BOOMARK_BAR_ALWAYS_SHOW);
  }
}

void BookmarkContextMenuControllerViews::ExecuteCommand(int id) {
  BookmarkModel* model = RemoveModelObserver();

  switch (id) {
    case IDS_BOOMARK_BAR_OPEN_ALL:
    case IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO:
    case IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW: {
      WindowOpenDisposition initial_disposition;
      if (id == IDS_BOOMARK_BAR_OPEN_ALL) {
        initial_disposition = NEW_FOREGROUND_TAB;
        UserMetrics::RecordAction(
            UserMetricsAction("BookmarkBar_ContextMenu_OpenAll"),
            profile_);
      } else if (id == IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW) {
        initial_disposition = NEW_WINDOW;
        UserMetrics::RecordAction(
            UserMetricsAction("BookmarkBar_ContextMenu_OpenAllInNewWindow"),
            profile_);
      } else {
        initial_disposition = OFF_THE_RECORD;
        UserMetrics::RecordAction(
            UserMetricsAction("BookmarkBar_ContextMenu_OpenAllIncognito"),
            profile_);
      }
      bookmark_utils::OpenAll(parent_window_, profile_, navigator_, selection_,
                              initial_disposition);
      break;
    }

    case IDS_BOOKMARK_BAR_RENAME_FOLDER:
    case IDS_BOOKMARK_BAR_EDIT:
      UserMetrics::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Edit"), profile_);

      if (selection_.size() != 1) {
        NOTREACHED();
        return;
      }

      if (selection_[0]->is_url()) {
        BookmarkEditor::Configuration editor_config;
        if (configuration_ == BOOKMARK_BAR)
          editor_config = BookmarkEditor::SHOW_TREE;
        else
          editor_config = BookmarkEditor::NO_TREE;
        BookmarkEditor::Show(parent_window_, profile_, parent_,
                             BookmarkEditor::EditDetails(selection_[0]),
                             editor_config, NULL);
      } else {
        BookmarkFolderEditorController::Show(profile_, parent_window_,
            selection_[0], -1, BookmarkFolderEditorController::NONE);
      }
      break;

    case IDS_BOOKMARK_BAR_REMOVE: {
      UserMetrics::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Remove"), profile_);

      delegate_->WillRemoveBookmarks(selection_);
      for (size_t i = 0; i < selection_.size(); ++i) {
        model->Remove(selection_[i]->GetParent(),
                      selection_[i]->GetParent()->IndexOfChild(selection_[i]));
      }
      delegate_->DidRemoveBookmarks();
      selection_.clear();
      break;
    }

    case IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK: {
      UserMetrics::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Add"), profile_);

      BookmarkEditor::Configuration editor_config;
      BookmarkEditor::Handler* handler = NULL;
      if (configuration_ == BOOKMARK_BAR) {
        editor_config = BookmarkEditor::SHOW_TREE;
      } else {
        editor_config = BookmarkEditor::NO_TREE;
        // This is owned by the BookmarkEditorView.
        handler = new SelectOnCreationHandler(profile_);
      }
      // TODO: this should honor the index from GetParentForNewNodes.
      BookmarkEditor::Show(
          parent_window_, profile_,
          bookmark_utils::GetParentForNewNodes(parent_, selection_, NULL),
          BookmarkEditor::EditDetails(), editor_config, handler);
      break;
    }

    case IDS_BOOMARK_BAR_NEW_FOLDER: {
      UserMetrics::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_NewFolder"),
          profile_);
      int index;
      const BookmarkNode* parent =
          bookmark_utils::GetParentForNewNodes(parent_, selection_, &index);
      uint32 details = BookmarkFolderEditorController::IS_NEW;
      if (configuration_ != BOOKMARK_BAR)
        details |= BookmarkFolderEditorController::SHOW_IN_MANAGER;
      BookmarkFolderEditorController::Show(profile_, parent_window_, parent,
                                           index, details);
      break;
    }

    case IDS_BOOMARK_BAR_ALWAYS_SHOW:
      bookmark_utils::ToggleWhenVisible(profile_);
      break;

    case IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER:
      UserMetrics::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_ShowInFolder"),
          profile_);

      if (selection_.size() != 1) {
        NOTREACHED();
        return;
      }

      BookmarkManager::SelectInTree(profile_, selection_[0]);
      break;

    case IDS_BOOKMARK_MANAGER:
      UserMetrics::RecordAction(UserMetricsAction("ShowBookmarkManager"),
                                profile_);
      {
        Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
        if (browser)
          browser->OpenBookmarkManager();
        else
          NOTREACHED();
      }
      break;

    case IDS_BOOKMARK_MANAGER_SORT:
      UserMetrics::RecordAction(UserMetricsAction("BookmarkManager_Sort"),
                                profile_);
      model->SortChildren(parent_);
      break;

    case IDS_CUT:
      delegate_->WillRemoveBookmarks(selection_);
      bookmark_utils::CopyToClipboard(model, selection_, true);
      delegate_->DidRemoveBookmarks();
      break;

    case IDS_COPY:
      bookmark_utils::CopyToClipboard(model, selection_, false);
      break;

    case IDS_PASTE: {
      int index;
      const BookmarkNode* paste_target =
          bookmark_utils::GetParentForNewNodes(parent_, selection_, &index);
      if (!paste_target)
        return;

      bookmark_utils::PasteFromClipboard(model, parent_, index);
      break;
    }

    default:
      NOTREACHED();
  }
}

bool BookmarkContextMenuControllerViews::IsItemChecked(int id) const {
  DCHECK(id == IDS_BOOMARK_BAR_ALWAYS_SHOW);
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BookmarkContextMenuControllerViews::IsCommandEnabled(int id) const {
  bool is_root_node =
      (selection_.size() == 1 &&
       selection_[0]->GetParent() == model_->root_node());
  switch (id) {
    case IDS_BOOMARK_BAR_OPEN_INCOGNITO:
      return !profile_->IsOffTheRecord();

    case IDS_BOOMARK_BAR_OPEN_ALL_INCOGNITO:
      return HasURLs() && !profile_->IsOffTheRecord();

    case IDS_BOOMARK_BAR_OPEN_ALL:
    case IDS_BOOMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return HasURLs();

    case IDS_BOOKMARK_BAR_RENAME_FOLDER:
    case IDS_BOOKMARK_BAR_EDIT:
      return selection_.size() == 1 && !is_root_node;

    case IDS_BOOKMARK_BAR_REMOVE:
      return !selection_.empty() && !is_root_node;

    case IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER:
      return (configuration_ == BOOKMARK_MANAGER_TABLE_OTHER ||
              configuration_ == BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER) &&
             selection_.size() == 1;

    case IDS_BOOKMARK_MANAGER_SORT:
      return parent_ && parent_ != model_->root_node();

    case IDS_BOOMARK_BAR_NEW_FOLDER:
    case IDS_BOOMARK_BAR_ADD_NEW_BOOKMARK:
      return bookmark_utils::GetParentForNewNodes(
          parent_, selection_, NULL) != NULL;

    case IDS_COPY:
    case IDS_CUT:
      return selection_.size() > 0 && !is_root_node;

    case IDS_PASTE:
      // Paste to selection from the Bookmark Bar, to parent_ everywhere else
      return (configuration_ == BOOKMARK_BAR && !selection_.empty() &&
              bookmark_utils::CanPasteFromClipboard(selection_[0])) ||
              bookmark_utils::CanPasteFromClipboard(parent_);
  }
  return true;
}

void BookmarkContextMenuControllerViews::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  ModelChanged();
}

void BookmarkContextMenuControllerViews::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    int old_index,
    const BookmarkNode* new_parent,
    int new_index) {
  ModelChanged();
}

void BookmarkContextMenuControllerViews::BookmarkNodeAdded(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int index) {
  ModelChanged();
}

void BookmarkContextMenuControllerViews::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int index,
    const BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenuControllerViews::BookmarkNodeChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenuControllerViews::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  ModelChanged();
}

void BookmarkContextMenuControllerViews::ModelChanged() {
  delegate_->CloseMenu();
}

BookmarkModel* BookmarkContextMenuControllerViews::RemoveModelObserver() {
  BookmarkModel* model = model_;
  model_->RemoveObserver(this);
  model_ = NULL;
  return model;
}

bool BookmarkContextMenuControllerViews::HasURLs() const {
  for (size_t i = 0; i < selection_.size(); ++i) {
    if (NodeHasURLs(selection_[i]))
      return true;
  }
  return false;
}
