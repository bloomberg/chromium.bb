// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_menu_model.h"

#include "base/prefs/testing_pref_service.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#include "chrome/browser/chrome_to_mobile_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
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
                               public ui::SimpleMenuModel::Delegate {
 public:
  ActionBoxMenuModelTest() {}

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return false;
  }

  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {}

    // Don't handle accelerators.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE { return false; }

  void InitProfile(){
    profile()->set_incognito(true);
    profile()->GetPrefs()->ClearPref(prefs::kChromeToMobileDeviceList);
    profile()->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
  }

  void SetProfileHasMobiles() {
    ListValue mobiles;
    DictionaryValue* mobile = new DictionaryValue();
    mobile->SetString("type", "Nexus");
    mobile->SetString("name", "MyPhone");
    mobile->SetString("id", "12345");
    mobiles.Append(mobile);
    profile()->GetPrefs()->Set(prefs::kChromeToMobileDeviceList, mobiles);
  }

  void SetProfileSignedIn() {
    profile()->set_incognito(false);
    // Set username pref (i.e. sign in),
    profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "foo");

    // Set up c2m-service.
    ChromeToMobileService* service =
        ChromeToMobileServiceFactory::GetForProfile(profile());
    service->OnInvalidatorStateChange(syncer::INVALIDATIONS_ENABLED);
  }

  void NavigateToBookmarkablePage() {
    AddTab(browser(), GURL("http://www.google.com"));
  }

  void NavigateToLocalPage() {
    AddTab(browser(), GURL("chrome://blank"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ActionBoxMenuModelTest);
};

// Tests that Chrome2Mobile is disabled on incognito profiles without devices.
TEST_F(ActionBoxMenuModelTest, IncongnitoNoMobiles) {
  InitProfile();

  NavigateToLocalPage();
  // Create model.
  ActionBoxMenuModel model(browser(), this);

  // Expect no c2m command in model.
  EXPECT_EQ(-1, model.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));

  NavigateToBookmarkablePage();

  // Create model.
  ActionBoxMenuModel model2(browser(), this);

  // Expect c2m command not in model.
  EXPECT_EQ(-1, model2.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));
}

// Tests that Chrome2Mobile is disabled on incognito profiles with devices.
TEST_F(ActionBoxMenuModelTest, IncongnitoHasMobiles) {
  InitProfile();
  SetProfileHasMobiles();

  NavigateToLocalPage();

  // Create model.
  ActionBoxMenuModel model(browser(), this);

  // Expect no c2m command in model.
  EXPECT_EQ(-1, model.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));

  NavigateToBookmarkablePage();

  // Create model.

  ActionBoxMenuModel model2(browser(), this);
  // Expect c2m command not in model.
  EXPECT_EQ(-1, model2.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));
}

// Tests that Chrome2Mobile is disabled for signed-in profiles with no devices.
TEST_F(ActionBoxMenuModelTest, OnRecordNoMobiles) {
  FeatureSwitch::ScopedOverride enable_action_box(FeatureSwitch::action_box(),
                                                  true);
  InitProfile();
  SetProfileSignedIn();

  NavigateToLocalPage();

  // Create model.
  ActionBoxMenuModel model(browser(), this);

  // Expect no c2m command in model.
  EXPECT_EQ(-1, model.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));

  NavigateToBookmarkablePage();

  // Create model.
  ActionBoxMenuModel model2(browser(), this);

  // Expect c2m command not in model.
  EXPECT_EQ(-1, model2.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));
}

// Tests that Chrome2Mobile is enabled for signed-in profiles with devices, and
// disabled if the profile is set to incognito mode.
TEST_F(ActionBoxMenuModelTest, HasMobilesOnRecordOrIncognito) {
  FeatureSwitch::ScopedOverride enable_action_box(FeatureSwitch::action_box(),
                                                  true);
  InitProfile();
  SetProfileSignedIn();
  SetProfileHasMobiles();

  NavigateToLocalPage();

  // Create model.
  ActionBoxMenuModel model(browser(), this);

  // Expect no c2m command in model.
  EXPECT_EQ(-1, model.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));

  NavigateToBookmarkablePage();

  // Create model.
  ActionBoxMenuModel model2(browser(), this);

  // Expect c2m command in model.
  EXPECT_NE(-1, model2.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));

  // Incognito-ize profile.
  profile()->set_incognito(true);

  // Create another model.
  ActionBoxMenuModel model3(browser(), this);

  // Expect no c2m command in this model.
  EXPECT_EQ(-1, model3.GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_CHROME_TO_MOBILE_PAGE));

  // Un-incognito-ize for shutdown.
  profile()->set_incognito(false);
}

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

  // Create model.
  ActionBoxMenuModel model(browser(), this);

  // Bokomark item should be in menu.
  int bookmark_item_index = model.GetIndexOfCommandId(
      IDC_BOOKMARK_PAGE_FROM_STAR);
  EXPECT_NE(-1, bookmark_item_index);

  gfx::Image bookmark_icon;
  gfx::Image unlit_icon;
  gfx::Image lit_icon;

  model.GetIconAt(bookmark_item_index, &bookmark_icon);
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

  // Create model.
  ActionBoxMenuModel model2(browser(), this);

  model2.GetIconAt(bookmark_item_index, &bookmark_icon);
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
