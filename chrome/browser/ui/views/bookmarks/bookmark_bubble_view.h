// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_delegate.h"
#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "url/gurl.h"

class BookmarkBubbleViewObserver;
class Profile;

namespace views {
class LabelButton;
class Textfield;
}

// BookmarkBubbleView is a view intended to be used as the content of an
// Bubble. BookmarkBubbleView provides views for unstarring and editing the
// bookmark it is created with. Don't create a BookmarkBubbleView directly,
// instead use the static Show method.
class BookmarkBubbleView : public views::BubbleDelegateView,
                           public views::ButtonListener,
                           public views::ComboboxListener {
 public:
  static void ShowBubble(views::View* anchor_view,
                         BookmarkBubbleViewObserver* observer,
                         scoped_ptr<BookmarkBubbleDelegate> delegate,
                         Profile* profile,
                         const GURL& url,
                         bool newly_bookmarked);

  static bool IsShowing();

  static void Hide();

  virtual ~BookmarkBubbleView();

  // views::BubbleDelegateView method.
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;

  // views::WidgetDelegate method.
  virtual void WindowClosing() OVERRIDE;

  // views::View method.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

 protected:
  // views::BubbleDelegateView method.
  virtual void Init() OVERRIDE;

 private:
  friend class BookmarkBubbleViewTest;
  FRIEND_TEST_ALL_PREFIXES(BookmarkBubbleViewTest, SyncPromoSignedIn);
  FRIEND_TEST_ALL_PREFIXES(BookmarkBubbleViewTest, SyncPromoNotSignedIn);

  // Creates a BookmarkBubbleView.
  BookmarkBubbleView(views::View* anchor_view,
                     BookmarkBubbleViewObserver* observer,
                     scoped_ptr<BookmarkBubbleDelegate> delegate,
                     Profile* profile,
                     const GURL& url,
                     bool newly_bookmarked);

  // Returns the title to display.
  base::string16 GetTitle();

  // Overridden from views::View:
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // Overridden from views::ButtonListener:
  // Closes the bubble or opens the edit dialog.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::ComboboxListener:
  virtual void OnPerformAction(views::Combobox* combobox) OVERRIDE;

  // Handle the message when the user presses a button.
  void HandleButtonPressed(views::Button* sender);

  // Shows the BookmarkEditor.
  void ShowEditor();

  // Sets the title and parent of the node.
  void ApplyEdits();

  // The bookmark bubble, if we're showing one.
  static BookmarkBubbleView* bookmark_bubble_;

  // Our observer, to notify when the bubble shows or hides.
  BookmarkBubbleViewObserver* observer_;

  // Delegate, to handle clicks on the sign in link.
  scoped_ptr<BookmarkBubbleDelegate> delegate_;

  // The profile.
  Profile* profile_;

  // The bookmark URL.
  const GURL url_;

  // If true, the page was just bookmarked.
  const bool newly_bookmarked_;

  RecentlyUsedFoldersComboModel parent_model_;

  // Button for removing the bookmark.
  views::LabelButton* remove_button_;

  // Button to bring up the editor.
  views::LabelButton* edit_button_;

  // Button to close the window.
  views::LabelButton* close_button_;

  // Textfield showing the title of the bookmark.
  views::Textfield* title_tf_;

  // Combobox showing a handful of folders the user can choose from, including
  // the current parent.
  views::Combobox* parent_combobox_;

  // Bookmark sync promo view, if displayed.
  views::View* sync_promo_view_;

  // When the destructor is invoked should the bookmark be removed?
  bool remove_bookmark_;

  // When the destructor is invoked should edits be applied?
  bool apply_edits_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
