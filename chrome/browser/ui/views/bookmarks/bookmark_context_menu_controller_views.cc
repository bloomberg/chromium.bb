// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu_controller_views.h"

#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/instant/search.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/views/widget/widget.h"

using content::PageNavigator;
using content::UserMetricsAction;

#if !defined(OS_WIN)
// static
BookmarkContextMenuControllerViews* BookmarkContextMenuControllerViews::Create(
      views::Widget* parent_widget,
      BookmarkContextMenuControllerViewsDelegate* delegate,
      Browser* browser,
      Profile* profile,
      content::PageNavigator* navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection) {
  return new BookmarkContextMenuControllerViews(parent_widget, delegate,
                                                browser, profile, navigator,
                                                parent, selection);
}
#endif  // !defined(OS_WIN)

BookmarkContextMenuControllerViews::~BookmarkContextMenuControllerViews() {
  if (model_)
    model_->RemoveObserver(this);
}

void BookmarkContextMenuControllerViews::ExecuteCommand(int id) {
  BookmarkModel* model = RemoveModelObserver();

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
      chrome::OpenAll(parent_widget_->GetNativeWindow(), navigator_,
                      selection_, initial_disposition, profile_);
      bookmark_utils::RecordBookmarkLaunch(bookmark_utils::LAUNCH_CONTEXT_MENU);
      break;
    }

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      content::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Edit"));

      if (selection_.size() != 1) {
        NOTREACHED();
        return;
      }

      BookmarkEditor::Show(
          parent_widget_->GetNativeWindow(),
          profile_,
          BookmarkEditor::EditDetails::EditNode(selection_[0]),
          selection_[0]->is_url() ? BookmarkEditor::SHOW_TREE :
                                    BookmarkEditor::NO_TREE);
      break;

    case IDC_BOOKMARK_BAR_REMOVE: {
      content::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Remove"));

      delegate_->WillRemoveBookmarks(selection_);
      for (size_t i = 0; i < selection_.size(); ++i) {
        model->Remove(selection_[i]->parent(),
                      selection_[i]->parent()->GetIndexOf(selection_[i]));
      }
      delegate_->DidRemoveBookmarks();
      selection_.clear();
      break;
    }

    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK: {
      content::RecordAction(
          UserMetricsAction("BookmarkBar_ContextMenu_Add"));

      int index;
      const BookmarkNode* parent =
          bookmark_utils::GetParentForNewNodes(parent_, selection_, &index);
      GURL url;
      string16 title;
      chrome::GetURLAndTitleToBookmark(
          browser_->tab_strip_model()->GetActiveWebContents(),
          &url, &title);
      BookmarkEditor::Show(parent_widget_->GetNativeWindow(),
                           profile_,
                           BookmarkEditor::EditDetails::AddNodeInFolder(
                               parent, index, url, title),
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
          parent_widget_->GetNativeWindow(),
          profile_,
          BookmarkEditor::EditDetails::AddFolder(parent, index),
          BookmarkEditor::SHOW_TREE);
      break;
    }

    case IDC_BOOKMARK_BAR_ALWAYS_SHOW:
      chrome::ToggleBookmarkBarWhenVisible(profile_);
      break;

    case IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT: {
      PrefService* prefs = profile_->GetPrefs();
      prefs->SetBoolean(
          prefs::kShowAppsShortcutInBookmarkBar,
          !prefs->GetBoolean(prefs::kShowAppsShortcutInBookmarkBar));
      break;
    }

    case IDC_BOOKMARK_MANAGER: {
      content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
      if (selection_.size() != 1)
        chrome::ShowBookmarkManager(browser_);
      else if (selection_[0]->is_folder())
        chrome::ShowBookmarkManagerForNode(browser_, selection_[0]->id());
      else if (parent_)
        chrome::ShowBookmarkManagerForNode(browser_, parent_->id());
      else
        chrome::ShowBookmarkManager(browser_);
      break;
    }

    case IDC_CUT:
      delegate_->WillRemoveBookmarks(selection_);
      bookmark_utils::CopyToClipboard(model, selection_, true);
      delegate_->DidRemoveBookmarks();
      break;

    case IDC_COPY:
      bookmark_utils::CopyToClipboard(model, selection_, false);
      break;

    case IDC_PASTE: {
      int index;
      const BookmarkNode* paste_target =
          bookmark_utils::GetParentForNewNodes(parent_, selection_, &index);
      if (!paste_target)
        return;

      bookmark_utils::PasteFromClipboard(model, paste_target, index);
      break;
    }

    default:
      NOTREACHED();
  }
}

bool BookmarkContextMenuControllerViews::IsItemChecked(int id) const {
  if (id == IDC_BOOKMARK_BAR_ALWAYS_SHOW)
    return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);

  // This should only be available when instant extended is enabled.
  DCHECK(chrome::search::IsInstantExtendedAPIEnabled());
  DCHECK_EQ(IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT, id);
  return profile_->GetPrefs()->GetBoolean(
      prefs::kShowAppsShortcutInBookmarkBar);
}

bool BookmarkContextMenuControllerViews::IsCommandEnabled(int id) const {
  bool is_root_node = selection_.size() == 1 &&
                      selection_[0]->parent() == model_->root_node();
  bool can_edit =
      profile_->GetPrefs()->GetBoolean(prefs::kEditBookmarksEnabled);
  IncognitoModePrefs::Availability incognito_avail =
      IncognitoModePrefs::GetAvailability(profile_->GetPrefs());
  switch (id) {
    case IDC_BOOKMARK_BAR_OPEN_INCOGNITO:
      return !profile_->IsOffTheRecord() &&
             incognito_avail != IncognitoModePrefs::DISABLED;

    case IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO:
      return chrome::HasBookmarkURLsAllowedInIncognitoMode(selection_, profile_)
             &&
             !profile_->IsOffTheRecord() &&
             incognito_avail != IncognitoModePrefs::DISABLED;

    case IDC_BOOKMARK_BAR_OPEN_ALL:
      return chrome::HasBookmarkURLs(selection_);
    case IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW:
      return chrome::HasBookmarkURLs(selection_) &&
             incognito_avail != IncognitoModePrefs::FORCED;

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

    case IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT:
      return !profile_->GetPrefs()->IsManagedPreference(
          prefs::kShowAppsShortcutInBookmarkBar);

    case IDC_COPY:
    case IDC_CUT:
      return !selection_.empty() && !is_root_node &&
             (id == IDC_COPY || can_edit);

    case IDC_PASTE:
      // Paste to selection from the Bookmark Bar, to parent_ everywhere else
      return can_edit &&
             ((!selection_.empty() &&
               bookmark_utils::CanPasteFromClipboard(selection_[0])) ||
              bookmark_utils::CanPasteFromClipboard(parent_));
  }
  return true;
}

void BookmarkContextMenuControllerViews::BuildMenu() {
  if (selection_.size() == 1 && selection_[0]->is_url()) {
    delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_OPEN_ALL,
                                   IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB);
    delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
                                   IDS_BOOKMARK_BAR_OPEN_IN_NEW_WINDOW);
    delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
                                   IDS_BOOKMARK_BAR_OPEN_INCOGNITO);
  } else {
    delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_OPEN_ALL,
                                   IDS_BOOKMARK_BAR_OPEN_ALL);
    delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW,
                                   IDS_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW);
    delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO,
                                   IDS_BOOKMARK_BAR_OPEN_ALL_INCOGNITO);
  }

  delegate_->AddSeparator();
  if (selection_.size() == 1 && selection_[0]->is_folder()) {
    delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_RENAME_FOLDER,
                                   IDS_BOOKMARK_BAR_RENAME_FOLDER);
  } else {
    delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_EDIT,
                                   IDS_BOOKMARK_BAR_EDIT);
  }

  delegate_->AddSeparator();
  delegate_->AddItemWithStringId(IDC_CUT, IDS_CUT);
  delegate_->AddItemWithStringId(IDC_COPY, IDS_COPY);
  delegate_->AddItemWithStringId(IDC_PASTE, IDS_PASTE);

  delegate_->AddSeparator();
  delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_REMOVE,
                                 IDS_BOOKMARK_BAR_REMOVE);

  delegate_->AddSeparator();
  delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK,
                                 IDS_BOOKMARK_BAR_ADD_NEW_BOOKMARK);
  delegate_->AddItemWithStringId(IDC_BOOKMARK_BAR_NEW_FOLDER,
                                 IDS_BOOKMARK_BAR_NEW_FOLDER);

  delegate_->AddSeparator();
  delegate_->AddItemWithStringId(IDC_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
  if (chrome::search::IsInstantExtendedAPIEnabled()) {
    delegate_->AddCheckboxItem(IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT,
                               IDS_BOOKMARK_BAR_SHOW_APPS_SHORTCUT);
  }
  delegate_->AddCheckboxItem(IDC_BOOKMARK_BAR_ALWAYS_SHOW,
                             IDS_SHOW_BOOKMARK_BAR);
}

BookmarkContextMenuControllerViews::BookmarkContextMenuControllerViews(
    views::Widget* parent_widget,
    BookmarkContextMenuControllerViewsDelegate* delegate,
    Browser* browser,
    Profile* profile,
    PageNavigator* navigator,
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection)
    : parent_widget_(parent_widget),
      delegate_(delegate),
      browser_(browser),
      profile_(profile),
      navigator_(navigator),
      parent_(parent),
      selection_(selection),
      model_(BookmarkModelFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(model_->IsLoaded());
  model_->AddObserver(this);
}

void BookmarkContextMenuControllerViews::BookmarkModelChanged() {
  delegate_->CloseMenu();
}

BookmarkModel* BookmarkContextMenuControllerViews::RemoveModelObserver() {
  BookmarkModel* model = model_;
  model_->RemoveObserver(this);
  model_ = NULL;
  return model;
}
