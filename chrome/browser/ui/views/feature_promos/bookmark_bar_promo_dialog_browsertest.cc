// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/feature_promos/bookmark_bar_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_member.h"
#include "components/strings/grit/components_strings.h"

class BookmarkBarPromoDialogTest : public DialogBrowserTest {
 public:
  BookmarkBarPromoDialogTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Create a bookmark for the promo to be shown against.
    Profile* profile = browser()->profile();
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(profile);
    bookmarks::test::WaitForBookmarkModelToLoad(model);
    bookmarks::test::AddNodesFromModelString(model, model->bookmark_bar_node(),
                                             "a ");

    // Now actually show the promo.
    BookmarkBarPromoBubbleView bubble_view(
        IDS_NUX_GOOGLE_APPS_DESCRIPTION_PROMO_BUBBLE);
    bubble_view.ShowForNode(model->bookmark_bar_node()->GetChild(0));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkBarPromoDialogTest);
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(BookmarkBarPromoDialogTest,
                       InvokeUi_BookmarkBarPromoBubble) {
  BookmarkBarView::DisableAnimationsForTesting(true);
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowBookmarkBar, true);
  ShowAndVerifyUi();
  BookmarkBarView::DisableAnimationsForTesting(false);
}
