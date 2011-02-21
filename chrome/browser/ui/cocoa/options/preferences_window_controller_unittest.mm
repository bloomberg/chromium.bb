// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/options/preferences_window_controller.h"

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/options/custom_home_pages_model.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

// Helper Objective-C object that sets a BOOL when we get a particular
// callback from the prefs window.
@interface PrefsClosedObserver : NSObject {
 @public
  BOOL gotNotification_;
}
- (void)prefsWindowClosed:(NSNotification*)notify;
@end

@implementation PrefsClosedObserver
- (void)prefsWindowClosed:(NSNotification*)notify {
  gotNotification_ = YES;
}
@end

namespace {

class PrefsControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    browser::RegisterLocalState(&local_state_);
    local_state_.RegisterBooleanPref(prefs::kMetricsReportingEnabled, false);
    browser_process_.get()->SetPrefService(&local_state_);

    pref_controller_ = [[PreferencesWindowController alloc]
                         initWithProfile:browser_helper_.profile()
                             initialPage:OPTIONS_PAGE_DEFAULT];
    EXPECT_TRUE(pref_controller_);
  }

  virtual void TearDown() {
    [pref_controller_ close];
    CocoaTest::TearDown();
  }

  ScopedTestingBrowserProcess browser_process_;
  TestingPrefService local_state_;
  BrowserTestHelper browser_helper_;
  PreferencesWindowController* pref_controller_;
};

// Test showing the preferences window and making sure it's visible, then
// making sure we get the notification when it's closed.
TEST_F(PrefsControllerTest, ShowAndClose) {
  [pref_controller_ showPreferences:nil];
  EXPECT_TRUE([[pref_controller_ window] isVisible]);

  scoped_nsobject<PrefsClosedObserver> observer(
      [[PrefsClosedObserver alloc] init]);
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:observer.get()
                    selector:@selector(prefsWindowClosed:)
                        name:NSWindowWillCloseNotification
                      object:[pref_controller_ window]];
  [[pref_controller_ window] performClose:observer];
  EXPECT_TRUE(observer.get()->gotNotification_);
  [defaultCenter removeObserver:observer.get()];

  // Prevent pref_controller_ from being closed again in TearDown()
  pref_controller_ = nil;
}

TEST_F(PrefsControllerTest, ValidateCustomHomePagesTable) {
  // First, insert two valid URLs into the CustomHomePagesModel.
  GURL url1("http://www.google.com/");
  GURL url2("http://maps.google.com/");
  std::vector<GURL> urls;
  urls.push_back(url1);
  urls.push_back(url2);
  [[pref_controller_ customPagesSource] setURLs:urls];
  EXPECT_EQ(2U, [[pref_controller_ customPagesSource] countOfCustomHomePages]);

  // Now insert a bad (empty) URL into the model.
  [[pref_controller_ customPagesSource] setURLStringEmptyAt:1];

  // Send a notification to simulate the end of editing on a cell in the table
  // which should trigger validation.
  [pref_controller_ controlTextDidEndEditing:[NSNotification
      notificationWithName:NSControlTextDidEndEditingNotification
                    object:nil]];
  EXPECT_EQ(1U, [[pref_controller_ customPagesSource] countOfCustomHomePages]);
}

TEST_F(PrefsControllerTest, NormalizePage) {
  EXPECT_EQ(OPTIONS_PAGE_GENERAL,
            [pref_controller_ normalizePage:OPTIONS_PAGE_GENERAL]);
  EXPECT_EQ(OPTIONS_PAGE_CONTENT,
            [pref_controller_ normalizePage:OPTIONS_PAGE_CONTENT]);
  EXPECT_EQ(OPTIONS_PAGE_ADVANCED,
            [pref_controller_ normalizePage:OPTIONS_PAGE_ADVANCED]);

  [pref_controller_ lastSelectedPage]->SetValue(OPTIONS_PAGE_ADVANCED);
  EXPECT_EQ(OPTIONS_PAGE_ADVANCED,
            [pref_controller_ normalizePage:OPTIONS_PAGE_DEFAULT]);

  [pref_controller_ lastSelectedPage]->SetValue(OPTIONS_PAGE_DEFAULT);
  EXPECT_EQ(OPTIONS_PAGE_GENERAL,
            [pref_controller_ normalizePage:OPTIONS_PAGE_DEFAULT]);
}

TEST_F(PrefsControllerTest, GetToolbarItemForPage) {
  // Trigger awakeFromNib.
  [pref_controller_ window];

  NSArray* toolbarItems = [[pref_controller_ toolbar] items];
  EXPECT_EQ([toolbarItems objectAtIndex:0],
            [pref_controller_ getToolbarItemForPage:OPTIONS_PAGE_GENERAL]);
  EXPECT_EQ([toolbarItems objectAtIndex:1],
            [pref_controller_ getToolbarItemForPage:OPTIONS_PAGE_CONTENT]);
  EXPECT_EQ([toolbarItems objectAtIndex:2],
            [pref_controller_ getToolbarItemForPage:OPTIONS_PAGE_ADVANCED]);

  [pref_controller_ lastSelectedPage]->SetValue(OPTIONS_PAGE_ADVANCED);
  EXPECT_EQ([toolbarItems objectAtIndex:2],
            [pref_controller_ getToolbarItemForPage:OPTIONS_PAGE_DEFAULT]);

  // Out-of-range argument.
  EXPECT_EQ([toolbarItems objectAtIndex:0],
            [pref_controller_ getToolbarItemForPage:OPTIONS_PAGE_COUNT]);
}

TEST_F(PrefsControllerTest, GetPageForToolbarItem) {
  scoped_nsobject<NSToolbarItem> toolbarItem(
      [[NSToolbarItem alloc] initWithItemIdentifier:@""]);
  [toolbarItem setTag:0];
  EXPECT_EQ(OPTIONS_PAGE_GENERAL,
            [pref_controller_ getPageForToolbarItem:toolbarItem]);
  [toolbarItem setTag:1];
  EXPECT_EQ(OPTIONS_PAGE_CONTENT,
            [pref_controller_ getPageForToolbarItem:toolbarItem]);
  [toolbarItem setTag:2];
  EXPECT_EQ(OPTIONS_PAGE_ADVANCED,
            [pref_controller_ getPageForToolbarItem:toolbarItem]);

  // Out-of-range argument.
  [toolbarItem setTag:3];
  EXPECT_EQ(OPTIONS_PAGE_GENERAL,
            [pref_controller_ getPageForToolbarItem:toolbarItem]);
}

TEST_F(PrefsControllerTest, GetPrefsViewForPage) {
  // Trigger awakeFromNib.
  [pref_controller_ window];

  EXPECT_EQ([pref_controller_ basicsView],
            [pref_controller_ getPrefsViewForPage:OPTIONS_PAGE_GENERAL]);
  EXPECT_EQ([pref_controller_ personalStuffView],
            [pref_controller_ getPrefsViewForPage:OPTIONS_PAGE_CONTENT]);
  EXPECT_EQ([pref_controller_ underTheHoodView],
            [pref_controller_ getPrefsViewForPage:OPTIONS_PAGE_ADVANCED]);

  [pref_controller_ lastSelectedPage]->SetValue(OPTIONS_PAGE_ADVANCED);
  EXPECT_EQ([pref_controller_ underTheHoodView],
            [pref_controller_ getPrefsViewForPage:OPTIONS_PAGE_DEFAULT]);
}

TEST_F(PrefsControllerTest, SwitchToPage) {
  // Trigger awakeFromNib.
  NSWindow* window = [pref_controller_ window];

  NSView* contentView = [window contentView];
  NSView* basicsView = [pref_controller_ basicsView];
  NSView* personalStuffView = [pref_controller_ personalStuffView];
  NSView* underTheHoodView = [pref_controller_ underTheHoodView];
  NSToolbar* toolbar = [pref_controller_ toolbar];
  NSToolbarItem* basicsToolbarItem =
      [pref_controller_ getToolbarItemForPage:OPTIONS_PAGE_GENERAL];
  NSToolbarItem* personalStuffToolbarItem =
      [pref_controller_ getToolbarItemForPage:OPTIONS_PAGE_CONTENT];
  NSToolbarItem* underTheHoodToolbarItem =
      [pref_controller_ getToolbarItemForPage:OPTIONS_PAGE_ADVANCED];
  NSString* basicsIdentifier = [basicsToolbarItem itemIdentifier];
  NSString* personalStuffIdentifier = [personalStuffToolbarItem itemIdentifier];
  NSString* underTheHoodIdentifier = [underTheHoodToolbarItem itemIdentifier];
  IntegerPrefMember* lastSelectedPage = [pref_controller_ lastSelectedPage];

  // Test without animation.

  [pref_controller_ switchToPage:OPTIONS_PAGE_GENERAL animate:NO];
  EXPECT_TRUE([basicsView isDescendantOf:contentView]);
  EXPECT_FALSE([personalStuffView isDescendantOf:contentView]);
  EXPECT_FALSE([underTheHoodView isDescendantOf:contentView]);
  EXPECT_NSEQ(basicsIdentifier, [toolbar selectedItemIdentifier]);
  EXPECT_EQ(OPTIONS_PAGE_GENERAL, lastSelectedPage->GetValue());
  EXPECT_NSEQ([basicsToolbarItem label], [window title]);

  [pref_controller_ switchToPage:OPTIONS_PAGE_CONTENT animate:NO];
  EXPECT_FALSE([basicsView isDescendantOf:contentView]);
  EXPECT_TRUE([personalStuffView isDescendantOf:contentView]);
  EXPECT_FALSE([underTheHoodView isDescendantOf:contentView]);
  EXPECT_NSEQ([toolbar selectedItemIdentifier], personalStuffIdentifier);
  EXPECT_EQ(OPTIONS_PAGE_CONTENT, lastSelectedPage->GetValue());
  EXPECT_NSEQ([personalStuffToolbarItem label], [window title]);

  [pref_controller_ switchToPage:OPTIONS_PAGE_ADVANCED animate:NO];
  EXPECT_FALSE([basicsView isDescendantOf:contentView]);
  EXPECT_FALSE([personalStuffView isDescendantOf:contentView]);
  EXPECT_TRUE([underTheHoodView isDescendantOf:contentView]);
  EXPECT_NSEQ([toolbar selectedItemIdentifier], underTheHoodIdentifier);
  EXPECT_EQ(OPTIONS_PAGE_ADVANCED, lastSelectedPage->GetValue());
  EXPECT_NSEQ([underTheHoodToolbarItem label], [window title]);

  // Test OPTIONS_PAGE_DEFAULT.

  lastSelectedPage->SetValue(OPTIONS_PAGE_CONTENT);
  [pref_controller_ switchToPage:OPTIONS_PAGE_DEFAULT animate:NO];
  EXPECT_FALSE([basicsView isDescendantOf:contentView]);
  EXPECT_TRUE([personalStuffView isDescendantOf:contentView]);
  EXPECT_FALSE([underTheHoodView isDescendantOf:contentView]);
  EXPECT_NSEQ(personalStuffIdentifier, [toolbar selectedItemIdentifier]);
  EXPECT_EQ(OPTIONS_PAGE_CONTENT, lastSelectedPage->GetValue());
  EXPECT_NSEQ([personalStuffToolbarItem label], [window title]);

  // TODO(akalin): Figure out how to test animation; we'll need everything
  // to stick around until the animation finishes.
}

// TODO(akalin): Figure out how to test sync controls.
// TODO(akalin): Figure out how to test that sync controls are not shown
// when there isn't a sync service.

}  // namespace
