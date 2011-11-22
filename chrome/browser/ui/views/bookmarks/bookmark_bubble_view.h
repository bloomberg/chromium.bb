// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/bookmarks/recently_used_folders_combo_model.h"
#include "googleurl/src/gurl.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "views/controls/button/button.h"
#include "views/controls/combobox/combobox_listener.h"
#include "views/controls/link_listener.h"

class Profile;

namespace views {
class TextButton;
class Textfield;
}

// BookmarkBubbleView is a view intended to be used as the content of an
// Bubble. BookmarkBubbleView provides views for unstarring and editing the
// bookmark it is created with. Don't create a BookmarkBubbleView directly,
// instead use the static Show method.
class BookmarkBubbleView : public views::BubbleDelegateView,
                           public views::LinkListener,
                           public views::ButtonListener,
                           public views::ComboboxListener {
 public:
  static void ShowBubble(views::View* anchor_view,
                         Profile* profile,
                         const GURL& url,
                         bool newly_bookmarked);

  static bool IsShowing();

  static void Hide();

  virtual ~BookmarkBubbleView();

  // views::BubbleDelegateView methods.
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual gfx::Point GetAnchorPoint() OVERRIDE;

  // views::WidgetDelegate method.
  virtual void WindowClosing() OVERRIDE;

  // views::View method.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

 protected:
  // views::BubbleDelegateView method.
  virtual void Init() OVERRIDE;

 private:
  // Creates a BookmarkBubbleView.
  BookmarkBubbleView(views::View* anchor_view,
                     Profile* profile,
                     const GURL& url,
                     bool newly_bookmarked);

  // Returns the title to display.
  string16 GetTitle();

  // Overridden from views::LinkListener:
  // Either unstars the item or shows the bookmark editor (depending upon which
  // link was clicked).
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Overridden from views::ButtonListener:
  // Closes the bubble or opens the edit dialog.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::ComboboxListener:
  // Changes the parent of the bookmark.
  virtual void ItemChanged(views::Combobox* combo_box,
                           int prev_index,
                           int new_index) OVERRIDE;

  // Handle the message when the user presses a button.
  void HandleButtonPressed(views::Button* sender);

  // Shows the BookmarkEditor.
  void ShowEditor();

  // Sets the title and parent of the node.
  void ApplyEdits();

  // The bookmark bubble, if we're showing one.
  static BookmarkBubbleView* bookmark_bubble_;

  // The profile.
  Profile* profile_;

  // The bookmark URL.
  const GURL url_;

  // Title of the bookmark. This is initially the title supplied to the
  // constructor, which is typically the title of the page.
  std::wstring title_;

  // If true, the page was just bookmarked.
  const bool newly_bookmarked_;

  RecentlyUsedFoldersComboModel parent_model_;

  // Link for removing/unstarring the bookmark.
  views::Link* remove_link_;

  // Button to bring up the editor.
  views::TextButton* edit_button_;

  // Button to close the window.
  views::TextButton* close_button_;

  // Textfield showing the title of the bookmark.
  views::Textfield* title_tf_;

  // Combobox showing a handful of folders the user can choose from, including
  // the current parent.
  views::Combobox* parent_combobox_;

  // When the destructor is invoked should the bookmark be removed?
  bool remove_bookmark_;

  // When the destructor is invoked should edits be applied?
  bool apply_edits_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
