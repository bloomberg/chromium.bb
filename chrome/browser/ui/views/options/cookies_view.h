// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_COOKIES_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_COOKIES_VIEW_H_
#pragma once

#include <string>

#include "base/task.h"
#include "chrome/browser/cookies_tree_model.h"
#include "net/base/cookie_monster.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/controls/tree/tree_view.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

class AppCacheInfoView;
class CookieInfoView;
class CookiesTreeView;
class DatabaseInfoView;
class IndexedDBInfoView;
class LocalStorageInfoView;
class Profile;
class Timer;

namespace ui {
class TreeModel;
class TreeModelNode;
}  // namespace ui

namespace views {
class Label;
class NativeButton;
}  // namespace views

class CookiesView : public CookiesTreeModel::Observer,
                    public views::View,
                    public views::DialogDelegate,
                    public views::ButtonListener,
                    public views::TreeViewController,
                    public views::TextfieldController {
 public:
  // Show the Cookies Window, creating one if necessary.
  static void ShowCookiesWindow(Profile* profile);

  virtual ~CookiesView();

  // Updates the display to show only the search results.
  void UpdateSearchResults();

  // Begin TreeModelObserver implementation.
  virtual void TreeNodesAdded(ui::TreeModel* model,
                              ui::TreeModelNode* parent,
                              int start,
                              int count);
  virtual void TreeNodesRemoved(ui::TreeModel* model,
                                ui::TreeModelNode* parent,
                                int start,
                                int count) {}
  virtual void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) {}
  // End TreeModelObserver implementation.

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::TreeViewController:
  virtual void OnTreeViewSelectionChanged(views::TreeView* tree_view);
  virtual void OnTreeViewKeyDown(ui::KeyboardCode keycode);

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key);

  // views::WindowDelegate:
  virtual int GetDialogButtons() const;
  virtual views::View* GetInitiallyFocusedView();
  virtual bool CanResize() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

  // views::View:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  // views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  class InfoPanelView;

  // Use the static factory method to show.
  explicit CookiesView(Profile* profile);

  // Initialize the dialog contents and layout.
  void Init();

  // Resets the display to what it would be if there were no search query.
  void ResetSearchQuery();

  // Update the UI when there are no cookies.
  void UpdateForEmptyState();

  // Enable or disable the remove and remove all buttons.
  void UpdateRemoveButtonsState();

  // Updates view to be visible inside detailed_info_view_;
  void UpdateVisibleDetailedInfo(views::View* view);

  // Assorted dialog controls
  views::Label* search_label_;
  views::Textfield* search_field_;
  views::NativeButton* clear_search_button_;
  views::Label* description_label_;
  CookiesTreeView* cookies_tree_;
  InfoPanelView* info_panel_;
  CookieInfoView* cookie_info_view_;
  DatabaseInfoView* database_info_view_;
  LocalStorageInfoView* local_storage_info_view_;
  AppCacheInfoView* appcache_info_view_;
  IndexedDBInfoView* indexed_db_info_view_;
  views::NativeButton* remove_button_;
  views::NativeButton* remove_all_button_;

  // The Cookies Tree model
  scoped_ptr<CookiesTreeModel> cookies_tree_model_;

  // The Profile for which Cookies are displayed
  Profile* profile_;

  // A factory to construct Runnable Methods so that we can be called back to
  // re-evaluate the model after the search query string changes.
  ScopedRunnableMethodFactory<CookiesView> search_update_factory_;

  // Our containing window. If this is non-NULL there is a visible Cookies
  // window somewhere.
  static views::Window* instance_;

  DISALLOW_COPY_AND_ASSIGN(CookiesView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_COOKIES_VIEW_H_
