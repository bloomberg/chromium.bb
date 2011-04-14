// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
#pragma once

#include "base/string16.h"
#include "chrome/browser/bookmarks/recently_used_folders_combo_model.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"
#include "views/controls/button/button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/link.h"
#include "views/view.h"

class Profile;

class BookmarkModel;
class BookmarkNode;

namespace views {
class NativeButton;
class Textfield;
}

// BookmarkBubbleView is a view intended to be used as the content of an
// Bubble. BookmarkBubbleView provides views for unstarring and editing the
// bookmark it is created with. Don't create a BookmarkBubbleView directly,
// instead use the static Show method.
class BookmarkBubbleView : public views::View,
                           public views::LinkController,
                           public views::ButtonListener,
                           public views::Combobox::Listener,
                           public BubbleDelegate {
 public:
  static void Show(views::Window* window,
                   const gfx::Rect& bounds,
                   BubbleDelegate* delegate,
                   Profile* profile,
                   const GURL& url,
                   bool newly_bookmarked);

  static bool IsShowing();

  static void Hide();

  virtual ~BookmarkBubbleView();

  void set_bubble(Bubble* bubble) { bubble_ = bubble; }

  // Invoked after the bubble has been shown.
  virtual void BubbleShown();

  // Override to close on return.
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

 private:
  // Creates a BookmarkBubbleView.
  // |title| is the title of the page. If newly_bookmarked is false, title is
  // ignored and the title of the bookmark is fetched from the database.
  BookmarkBubbleView(BubbleDelegate* delegate,
                     Profile* profile,
                     const GURL& url,
                     bool newly_bookmarked);
  // Creates the child views.
  void Init();

  // Returns the title to display.
  string16 GetTitle();

  // LinkController method, either unstars the item or shows the bookmark
  // editor (depending upon which link was clicked).
  virtual void LinkActivated(views::Link* source, int event_flags);

  // ButtonListener method, closes the bubble or opens the edit dialog.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Combobox::Listener method. Changes the parent of the bookmark.
  virtual void ItemChanged(views::Combobox* combobox,
                           int prev_index,
                           int new_index);

  // BubbleDelegate methods. These forward to the BubbleDelegate supplied in the
  // constructor as well as sending out the necessary notification.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape);
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow();
  virtual std::wstring accessible_name();

  // Closes the bubble.
  void Close();

  // Handle the message when the user presses a button.
  void HandleButtonPressed(views::Button* sender);

  // Shows the BookmarkEditor.
  void ShowEditor();

  // Sets the title and parent of the node.
  void ApplyEdits();

  // The bookmark bubble, if we're showing one.
  static BookmarkBubbleView* bookmark_bubble_;

  // The Bubble showing us.
  Bubble* bubble_;

  // Delegate for the bubble, may be null.
  BubbleDelegate* delegate_;

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
  views::NativeButton* edit_button_;

  // Button to close the window.
  views::NativeButton* close_button_;

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
