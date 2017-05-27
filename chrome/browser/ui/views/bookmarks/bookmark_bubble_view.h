// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_footnote_delegate.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "url/gurl.h"

class Profile;

class DesktopIOSPromotionBubbleView;

namespace bookmarks {
class BookmarkBubbleObserver;
}

#if defined(OS_WIN)
namespace desktop_ios_promotion {
enum class PromotionEntryPoint;
}
#endif

namespace views {
class LabelButton;
class Textfield;
}

// BookmarkBubbleView is a view intended to be used as the content of an
// Bubble. BookmarkBubbleView provides views for unstarring and editing the
// bookmark it is created with. Don't create a BookmarkBubbleView directly,
// instead use the static Show method.
class BookmarkBubbleView : public LocationBarBubbleDelegateView,
                           public views::ButtonListener,
                           public views::ComboboxListener,
                           public DesktopIOSPromotionFootnoteDelegate {
 public:
  // If |anchor_view| is null, |anchor_rect| is used to anchor the bubble and
  // |parent_window| is used to ensure the bubble closes if the parent closes.
  // Returns the newly created bubble's Widget or nullptr in case when the
  // bubble already exists.
  static views::Widget* ShowBubble(
      views::View* anchor_view,
      const gfx::Rect& anchor_rect,
      gfx::NativeView parent_window,
      bookmarks::BookmarkBubbleObserver* observer,
      std::unique_ptr<BubbleSyncPromoDelegate> delegate,
      Profile* profile,
      const GURL& url,
      bool already_bookmarked);

  static void Hide();

  static BookmarkBubbleView* bookmark_bubble() { return bookmark_bubble_; }

  ~BookmarkBubbleView() override;

  // views::LocationBarBubbleDelegateView:
  View* GetInitiallyFocusedView() override;
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  void WindowClosing() override;
  View* CreateFootnoteView() override;
  const char* GetClassName() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::ComboboxListener:
  void OnPerformAction(views::Combobox* combobox) override;

  // DesktopIOSPromotionFootnoteDelegate:
  void OnIOSPromotionFootnoteLinkClicked() override;

 protected:
  // views::LocationBarBubbleDelegateView:
  void Init() override;

 private:
  friend class BookmarkBubbleViewTest;
  friend class BookmarkBubbleViewBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(BookmarkBubbleViewTest, SyncPromoSignedIn);
  FRIEND_TEST_ALL_PREFIXES(BookmarkBubbleViewTest, SyncPromoNotSignedIn);

  // Creates a BookmarkBubbleView.
  BookmarkBubbleView(views::View* anchor_view,
                     bookmarks::BookmarkBubbleObserver* observer,
                     std::unique_ptr<BubbleSyncPromoDelegate> delegate,
                     Profile* profile,
                     const GURL& url,
                     bool newly_bookmarked);

  // Returns the title to display.
  base::string16 GetTitle();

  // Handle the message when the user presses a button.
  void HandleButtonPressed(views::Button* sender);

  // Shows the BookmarkEditor.
  void ShowEditor();

  // Sets the title and parent of the node.
  void ApplyEdits();

#if defined(OS_WIN)
  // Check eligiblity to showthe iOS promotion from a specific entry point.
  bool IsIOSPromotionEligible(
      desktop_ios_promotion::PromotionEntryPoint entry_point);

  // Shows the iOS promotion.
  void ShowIOSPromotion(desktop_ios_promotion::PromotionEntryPoint entry_point);
#endif

  // The bookmark bubble, if we're showing one.
  static BookmarkBubbleView* bookmark_bubble_;

  // Our observer, to notify when the bubble shows or hides.
  bookmarks::BookmarkBubbleObserver* observer_;

  // Delegate, to handle clicks on the sign in link.
  std::unique_ptr<BubbleSyncPromoDelegate> delegate_;

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

  // Bookmark details view, contains the details of the bookmark with controls
  // to edit it.
  std::unique_ptr<View> bookmark_details_view_;

  // iOS promotion view.
  DesktopIOSPromotionBubbleView* ios_promo_view_;

  // Footnote view.
  views::View* footnote_view_;

  // When the destructor is invoked should the bookmark be removed?
  bool remove_bookmark_;

  // When the destructor is invoked should edits be applied?
  bool apply_edits_;

  // Whether the Windows to iOS promotion is shown to the user.
  bool is_showing_ios_promotion_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
