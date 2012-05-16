// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_context_menu_controller.h"

#include "base/compiler_specific.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::PageNavigator;
using content::UserMetricsAction;

BookmarkContextMenuController::BookmarkContextMenuController(
    gfx::NativeWindow parent_window,
    BookmarkContextMenuControllerDelegate* delegate,
    Profile* profile,
    PageNavigator* navigator,
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection)
    : parent_window_(parent_window),
      delegate_(delegate),
      profile_(profile),
      navigator_(navigator),
      parent_(parent),
      selection_(selection),
      model_(profile->GetBookmarkModel()) {
  DCHECK(profile_);
  DCHECK(model_->IsLoaded());
  menu_model_.reset(new ui::SimpleMenuModel(this));
  model_->AddObserver(this);

  BuildMenu();
}

BookmarkContextMenuController::~BookmarkContextMenuController() {
  if (model_)
    model_->RemoveObserver(this);
}

void BookmarkContextMenuController::BuildMenu() {
  if (selection_.size() == 1 && selection_[0]->is_url()) {
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL,
            IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
            IDS_BOOKMARK_BAR_OPEN_IN_NEW_WINDOW);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
            IDS_BOOKMARK_BAR_OPEN_INCOGNITO);
  } else {
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL, IDS_BOOKMARK_BAR_OPEN_ALL);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
            IDS_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW);
    AddItem(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
            IDS_BOOKMARK_BAR_OPEN_ALL_INCOGNITO);
  }

  AddSeparator();
  if (selection_.size() == 1 && selection_[0]->is_folder()) {
    AddItem(IDC_BOOKMARK_BAR_RENAME_FOLDER, IDS_BOOKMARK_BAR_RENAME_FOLDER);
  } else {
    AddItem(IDC_BOOKMARK_BAR_EDIT, IDS_BOOKMARK_BAR_EDIT);
  }

  AddSeparator();
  AddItem(IDC_CUT, IDS_CUT);
  AddItem(IDC_COPY, IDS_COPY);
  AddItem(IDC_PASTE, IDS_PASTE);

  AddSeparator();
  AddItem(IDC_BOOKMARK_BAR_REMOVE, IDS_BOOKMARK_BAR_REMOVE);

  AddSeparator();
  AddItem(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK, IDS_BOOKMARK_BAR_ADD_NEW_BOOKMARK);
  AddItem(IDC_BOOKMARK_BAR_NEW_FOLDER, IDS_BOOKMARK_BAR_NEW_FOLDER);

  AddSeparator();
  AddItem(IDC_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
  AddCheckboxItem(IDC_BOOKMARK_BAR_ALWAYS_SHOW, IDS_SHOW_BOOKMARK_BAR);
}

void BookmarkContextMenuController::AddItem(int id, int localization_id) {
  menu_model_->AddItemWithStringId(id, localization_id);
}

void BookmarkContextMenuController::AddSeparator() {
  menu_model_->AddSeparator();
}

void BookmarkContextMenuController::AddCheckboxItem(int id,
                                                    int localization_id) {
  menu_model_->AddCheckItemWithStringId(id, localization_id);
}

void BookmarkContextMenuController::ExecuteCommand(int id) {
  if (delegate_)
    delegate_->WillExecuteCommand();

  switch (id) {
    case IDC_BOOKMARK_BAR_OPEN_ALL:
    case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW: {
      WindowOpenDisposition initial_disposition;
      if (id == IDC_BOOKMARK_BAR_OPEN_ALL) {
        initial_disposition = NEW_BACKGROUND_TAB;
        content::RecordAction(
            UserMetricsAction("BookmarkBar_ContextMenu_OpenAll"));
      } else if (id == IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW) {
        initial_disposition = NEW_WINDOW;
        content::RecordAction(
            UserMetricsAction("BookmarkBar_ContextMenu_OpenAllInNewWindow"));
      } else {
        initial_disposition = OFF_THE_RECORD;
        content::RecordAction(
            UserMetricsAction("BookmarkBar_ContextMenu_OpenAllIncognito"));
      }
      bookmark_utils::OpenAll(parent_window_, profile_, navigator_, selection_,
                              initial_disposition);
      break;
    }

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      content::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Edit"));

      if (selection_.size() != 1) {
        NOTREACHED();
        break;
      }

      BookmarkEditor::Show(
          parent_window_,
          profile_,
          BookmarkEditor::EditDetails::EditNode(selection_[0]),
          selection_[0]->is_url() ? BookmarkEditor::SHOW_TREE :
                                    BookmarkEditor::NO_TREE);
      break;

    case IDC_BOOKMARK_BAR_REMOVE: {
      content::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Remove"));

      for (size_t i = 0; i < selection_.size(); ++i) {
        int index = selection_[i]->parent()->GetIndexOf(selection_[i]);
        if (index > -1)
          model_->Remove(selection_[i]->parent(), index);
      }
      selection_.clear();
      break;
    }

    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK: {
      content::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Add"));

      int index;
      const BookmarkNode* parent =
          bookmark_utils::GetParentForNewNodes(parent_, selection_, &index);
      BookmarkEditor::Show(
          parent_window_,
          profile_,
          BookmarkEditor::EditDetails::AddNodeInFolder(parent, index),
          BookmarkEditor::SHOW_TREE);
      break;
    }

    case IDC_BOOKMARK_BAR_NEW_FOLDER: {
      content::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_NewFolder"));

      int index;
      const BookmarkNode* parent =
          bookmark_utils::GetParentForNewNodes(parent_, selection_, &index);
      BookmarkEditor::Show(
          parent_window_,
          profile_,
          BookmarkEditor::EditDetails::AddFolder(parent, index),
          BookmarkEditor::SHOW_TREE);
      break;
    }

    case IDC_BOOKMARK_BAR_ALWAYS_SHOW:
      bookmark_utils::ToggleWhenVisible(profile_);
      break;

    case IDC_BOOKMARK_MANAGER: {
      content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
      Browser* browser = browser::FindLastActiveWithProfile(profile_);
      if (!browser) NOTREACHED();

      if (selection_.size() != 1)
        browser->OpenBookmarkManager();
      else if (selection_[0]->is_folder())
        browser->OpenBookmarkManagerForNode(selection_[0]->id());
      else if (parent_)
        browser->OpenBookmarkManagerForNode(parent_->id());
      else
        browser->OpenBookmarkManager();
      break;
    }


    case IDC_CUT:
      bookmark_utils::CopyToClipboard(model_, selection_, true);
      break;

    case IDC_COPY:
      bookmark_utils::CopyToClipboard(model_, selection_, false);
      break;

    case IDC_PASTE: {
      int index;
      const BookmarkNode* paste_target =
          bookmark_utils::GetParentForNewNodes(parent_, selection_, &index);
      if (!paste_target)
        return;

      bookmark_utils::PasteFromClipboard(model_, paste_target, index);
      break;
    }

    default:
      NOTREACHED();
  }

  if (delegate_)
    delegate_->DidExecuteCommand();
}

bool BookmarkContextMenuController::IsCommandIdChecked(int command_id) const {
  DCHECK(command_id == IDC_BOOKMARK_BAR_ALWAYS_SHOW);
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BookmarkContextMenuController::IsCommandIdEnabled(int command_id) const {
  bool is_root_node = selection_.size() == 1 &&
                      selection_[0]->parent() == model_->root_node();
  bool can_edit =
      profile_->GetPrefs()->GetBoolean(prefs::kEditBookmarksEnabled);
  IncognitoModePrefs::Availability incognito_avail =
      IncognitoModePrefs::GetAvailability(profile_->GetPrefs());
  bool is_bookmark_bar_node = selection_.size() == 1 &&
                              selection_[0] == model_->bookmark_bar_node();
  switch (command_id) {
    case IDC_BOOKMARK_BAR_OPEN_INCOGNITO:
      return !profile_->IsOffTheRecord() &&
             incognito_avail != IncognitoModePrefs::DISABLED;

    case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
      return HasURLs() &&
             !profile_->IsOffTheRecord() &&
             incognito_avail != IncognitoModePrefs::DISABLED &&
             !is_bookmark_bar_node;

    case IDC_BOOKMARK_BAR_OPEN_ALL:
      return HasURLs() && !is_bookmark_bar_node;
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return HasURLs() &&
             incognito_avail != IncognitoModePrefs::FORCED &&
             !is_bookmark_bar_node;

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      return selection_.size() == 1 && !is_root_node && can_edit;

    case IDC_BOOKMARK_BAR_REMOVE:
      return !selection_.empty() && !is_root_node && can_edit;

    case IDC_BOOKMARK_BAR_NEW_FOLDER:
    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
      return can_edit && bookmark_utils::GetParentForNewNodes(
          parent_, selection_, NULL) != NULL;

    case IDC_BOOKMARK_BAR_ALWAYS_SHOW:
      return !profile_->GetPrefs()->IsManagedPreference(
          prefs::kShowBookmarkBar);

    case IDC_COPY:
    case IDC_CUT:
      return !selection_.empty() && !is_root_node &&
             (command_id == IDC_COPY || can_edit);

    case IDC_PASTE:
      // Paste to selection from the Bookmark Bar, to parent_ everywhere else
      return can_edit &&
             ((!selection_.empty() &&
               bookmark_utils::CanPasteFromClipboard(selection_[0])) ||
              bookmark_utils::CanPasteFromClipboard(parent_));
  }
  return true;
}

bool BookmarkContextMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void BookmarkContextMenuController::BookmarkModelChanged() {
  if (delegate_)
    delegate_->CloseMenu();
}

bool BookmarkContextMenuController::HasURLs() const {
  for (size_t i = 0; i < selection_.size(); ++i) {
    if (bookmark_utils::NodeHasURLs(selection_[i]))
      return true;
  }
  return false;
}
