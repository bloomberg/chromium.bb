// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/view_resizer_pong.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"
#include "chrome/browser/ui/toolbar/recent_tabs_builder_test_helper.h"
#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class MockWrenchMenuModel : public WrenchMenuModel {
 public:
  MockWrenchMenuModel() : WrenchMenuModel() {}
  ~MockWrenchMenuModel() {
    // This dirty, ugly hack gets around a bug in the test. In
    // ~WrenchMenuModel(), there's a call to TabstripModel::RemoveObserver(this)
    // which mysteriously leads to this crash: http://crbug.com/49206 .  It
    // seems that the vector of observers is getting hosed somewhere between
    // |-[ToolbarController dealloc]| and ~MockWrenchMenuModel(). This line
    // short-circuits the parent destructor to avoid this crash.
    tab_strip_model_ = NULL;
  }
  MOCK_METHOD2(ExecuteCommand, void(int command_id, int event_flags));
};

class WrenchMenuControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_.reset([[WrenchMenuController alloc] initWithBrowser:browser()]);
    fake_model_.reset(new MockWrenchMenuModel);
  }

  virtual void TearDown() OVERRIDE {
    fake_model_.reset();
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  WrenchMenuController* controller() {
    return controller_.get();
  }

  scoped_nsobject<WrenchMenuController> controller_;

  scoped_ptr<MockWrenchMenuModel> fake_model_;
};

TEST_F(WrenchMenuControllerTest, Initialized) {
  EXPECT_TRUE([controller() menu]);
  EXPECT_GE([[controller() menu] numberOfItems], 5);
}

TEST_F(WrenchMenuControllerTest, DispatchSimple) {
  scoped_nsobject<NSButton> button([[NSButton alloc] init]);
  [button setTag:IDC_ZOOM_PLUS];

  // Set fake model to test dispatching.
  EXPECT_CALL(*fake_model_, ExecuteCommand(IDC_ZOOM_PLUS, 0));
  [controller() setModel:fake_model_.get()];

  [controller() dispatchWrenchMenuCommand:button.get()];
  chrome::testing::NSRunLoopRunAllPending();
}

TEST_F(WrenchMenuControllerTest, RecentTabsFavIcon) {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile());
  browser_sync::SessionModelAssociator associator_(sync_service, true);
  associator_.SetCurrentMachineTagForTesting("WrenchMenuControllerTest");

  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  recent_tabs_builder.AddTab(0, 0);
  recent_tabs_builder.RegisterRecentTabs(&associator_);

  RecentTabsSubMenuModel recent_tabs_sub_menu_model(
      NULL, browser(), &associator_);
  fake_model_->AddSubMenuWithStringId(
      IDC_RECENT_TABS_MENU, IDS_RECENT_TABS_MENU,
      &recent_tabs_sub_menu_model);

  [controller() setModel:fake_model_.get()];
  NSMenu* menu = [controller() menu];
  [controller() updateRecentTabsSubmenu];

  NSString* title = l10n_util::GetNSStringWithFixup(IDS_RECENT_TABS_MENU);
  NSMenu* recent_tabs_menu = [[menu itemWithTitle:title] submenu];
  EXPECT_TRUE(recent_tabs_menu);
  EXPECT_EQ(6, [recent_tabs_menu numberOfItems]);

  // Send a icon changed event and verify that the icon is updated.
  gfx::Image icon(ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_BOOKMARKS_FAVICON));
  recent_tabs_sub_menu_model.SetIcon(3, icon);
  EXPECT_NSNE(icon.ToNSImage(), [[recent_tabs_menu itemAtIndex:3] image]);
  recent_tabs_sub_menu_model.GetMenuModelDelegate()->OnIconChanged(3);
  EXPECT_TRUE([[recent_tabs_menu itemAtIndex:3] image]);
  EXPECT_NSEQ(icon.ToNSImage(), [[recent_tabs_menu itemAtIndex:3] image]);

  controller_.reset();
  fake_model_.reset();
}

TEST_F(WrenchMenuControllerTest, RecentTabsElideTitle) {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile());
  browser_sync::SessionModelAssociator associator_(sync_service, true);
  associator_.SetCurrentMachineTagForTesting("WrenchMenuControllerTest");

  // Add 1 session with 1 window and 2 tabs.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  string16 tab1_short_title = ASCIIToUTF16("Short");
  recent_tabs_builder.AddTabWithInfo(0, 0, base::Time::Now(), tab1_short_title);
  string16 tab2_long_title = ASCIIToUTF16("Very very very very very very "
                                          "very very very very very very long");
  recent_tabs_builder.AddTabWithInfo(0, 0,
      base::Time::Now() - base::TimeDelta::FromMinutes(10), tab2_long_title);
  recent_tabs_builder.RegisterRecentTabs(&associator_);

  RecentTabsSubMenuModel recent_tabs_sub_menu_model(
      NULL, browser(), &associator_);
  fake_model_->AddSubMenuWithStringId(
      IDC_RECENT_TABS_MENU, IDS_RECENT_TABS_MENU,
      &recent_tabs_sub_menu_model);

  [controller() setModel:fake_model_.get()];
  NSMenu* menu = [controller() menu];
  [controller() updateRecentTabsSubmenu];

  NSString* title = l10n_util::GetNSStringWithFixup(IDS_RECENT_TABS_MENU);
  NSMenu* recent_tabs_menu = [[menu itemWithTitle:title] submenu];
  EXPECT_TRUE(recent_tabs_menu);
  EXPECT_EQ(7, [recent_tabs_menu numberOfItems]);

  // Index 0: restore tabs menu item.
  NSString* restore_tab_label = l10n_util::FixUpWindowsStyleLabel(
      recent_tabs_sub_menu_model.GetLabelAt(0));
  EXPECT_NSEQ(restore_tab_label, [[recent_tabs_menu itemAtIndex:0] title]);

  // Item 1: separator.
  EXPECT_TRUE([[recent_tabs_menu itemAtIndex:1] isSeparatorItem]);

  // Item 2: window title.
  EXPECT_NSEQ(
      base::SysUTF16ToNSString(recent_tabs_sub_menu_model.GetLabelAt(2)),
      [[recent_tabs_menu itemAtIndex:2] title]);

  // Item 3: short tab title.
  EXPECT_NSEQ(base::SysUTF16ToNSString(tab1_short_title),
              [[recent_tabs_menu itemAtIndex:3] title]);

  // Item 4: long tab title.
  NSString* tab2_actual_title = [[recent_tabs_menu itemAtIndex:4] title];
  NSUInteger title_length = [tab2_actual_title length];
  EXPECT_GT(tab2_long_title.size(), title_length);
  NSString* actual_substring =
      [tab2_actual_title substringToIndex:title_length - 1];
  NSString* expected_substring = [base::SysUTF16ToNSString(tab2_long_title)
      substringToIndex:title_length - 1];
  EXPECT_NSEQ(expected_substring, actual_substring);

  controller_.reset();
  fake_model_.reset();
}

// Verify that |RecentTabsMenuModelDelegate| is deleted before the model
// it's observing.
TEST_F(WrenchMenuControllerTest, RecentTabDeleteOrder) {
  chrome::EnableInstantExtendedAPIForTesting();
  [controller_ menuNeedsUpdate:[controller_ menu]];
  // If the delete order is wrong then the test will crash on exit.
}

}  // namespace
