// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_menu_model.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/toolbar/action_box_button_controller.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "sync/notifier/invalidation_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

using extensions::FeatureSwitch;

class ActionBoxMenuModelTest : public BrowserWithTestWindowTest,
                               public ActionBoxButtonController::Delegate {
 public:
  ActionBoxMenuModelTest() {}

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    controller_.reset(new ActionBoxButtonController(browser(), this));
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  scoped_ptr<ActionBoxMenuModel> CreateModel() {
    return controller_->CreateMenuModel();
  }

  void InitProfile(){
    profile()->set_incognito(true);
    profile()->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
  }

  void SetProfileSignedIn() {
    profile()->set_incognito(false);
    // Set username pref (i.e. sign in),
    profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "foo");
  }

  void NavigateToBookmarkablePage() {
    AddTab(browser(), GURL("http://www.google.com"));
  }

  void NavigateToLocalPage() {
    AddTab(browser(), GURL("chrome://blank"));
  }

 private:
  scoped_ptr<ActionBoxButtonController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxMenuModelTest);
};

// Tests that Bookmark Star is lit up only on bookmarked pages.
TEST_F(ActionBoxMenuModelTest, BookmarkedPage) {
  FeatureSwitch::ScopedOverride enable_action_box(FeatureSwitch::action_box(),
                                                  true);
  // Set up bookmark model
  profile()->CreateBookmarkModel(true);
  ui_test_utils::WaitForBookmarkModelToLoad(profile());

  // Navigate to a url.
  GURL url1("http://www.google.com");
  AddTab(browser(), url1);

  scoped_ptr<ActionBoxMenuModel> model = CreateModel();

  // Bokomark item should be in menu.
  int bookmark_item_index = model->GetIndexOfCommandId(
      IDC_BOOKMARK_PAGE_FROM_STAR);
  ASSERT_NE(-1, bookmark_item_index);

  gfx::Image bookmark_icon;
  gfx::Image unlit_icon;
  gfx::Image lit_icon;

  model->GetIconAt(bookmark_item_index, &bookmark_icon);
  unlit_icon =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(IDR_STAR);

  SkBitmap bookmark_icon_bitmap = *bookmark_icon.ToSkBitmap();
  SkBitmap unlit_icon_bitmap = *unlit_icon.ToSkBitmap();
  SkAutoLockPixels a(bookmark_icon_bitmap);
  SkAutoLockPixels b(unlit_icon_bitmap);

  // Verify that the icon in the menu is the unlit icon.
  EXPECT_EQ(0, memcmp(bookmark_icon_bitmap.getPixels(),
                      unlit_icon_bitmap.getPixels(),
                      unlit_icon_bitmap.getSize()));

  // Now bookmark it.
  chrome::BookmarkCurrentPage(browser());

  scoped_ptr<ActionBoxMenuModel> model2 = CreateModel();

  model2->GetIconAt(bookmark_item_index, &bookmark_icon);
  lit_icon =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(IDR_STAR_LIT);

  SkBitmap bookmark_icon_bitmap2 = *bookmark_icon.ToSkBitmap();
  SkBitmap lit_icon_bitmap = *lit_icon.ToSkBitmap();
  SkAutoLockPixels c(bookmark_icon_bitmap2);
  SkAutoLockPixels d(lit_icon_bitmap);


  // Verify that the icon in the menu is the lit icon.
  EXPECT_EQ(0, memcmp(bookmark_icon_bitmap2.getPixels(),
                      lit_icon_bitmap.getPixels(),
                      lit_icon_bitmap.getSize()));
}
