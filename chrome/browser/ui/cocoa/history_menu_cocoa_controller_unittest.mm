// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/cocoa/history_menu_cocoa_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface FakeHistoryMenuController : HistoryMenuCocoaController {
 @public
  BOOL opened_[2];
}
@end

@implementation FakeHistoryMenuController

- (id)initTest {
  if ((self = [super init])) {
    opened_[0] = NO;
    opened_[1] = NO;
  }
  return self;
}

- (void)openURLForItem:(HistoryMenuBridge::HistoryItem*)item {
  opened_[item->session_id] = YES;
}

@end  // FakeHistoryMenuController

class HistoryMenuCocoaControllerTest : public CocoaTest {
 public:

  virtual void SetUp() {
    CocoaTest::SetUp();
    bridge_.reset(new HistoryMenuBridge(browser_test_helper_.profile()));
    bridge_->controller_.reset(
        [[FakeHistoryMenuController alloc] initWithBridge:bridge_.get()]);
    [controller() initTest];
  }

  void CreateItems(NSMenu* menu) {
    HistoryMenuBridge::HistoryItem* item = new HistoryMenuBridge::HistoryItem();
    item->url = GURL("http://google.com");
    item->session_id = 0;
    bridge_->AddItemToMenu(item, menu, HistoryMenuBridge::kMostVisited, 0);

    item = new HistoryMenuBridge::HistoryItem();
    item->url = GURL("http://apple.com");
    item->session_id = 1;
    bridge_->AddItemToMenu(item, menu, HistoryMenuBridge::kMostVisited, 1);
  }

  std::map<NSMenuItem*, HistoryMenuBridge::HistoryItem*>& menu_item_map() {
    return bridge_->menu_item_map_;
  }

  FakeHistoryMenuController* controller() {
    return static_cast<FakeHistoryMenuController*>(bridge_->controller_.get());
  }

 private:
  BrowserTestHelper browser_test_helper_;
  scoped_ptr<HistoryMenuBridge> bridge_;
};

TEST_F(HistoryMenuCocoaControllerTest, OpenURLForItem) {

  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"History"]);
  CreateItems(menu.get());

  std::map<NSMenuItem*, HistoryMenuBridge::HistoryItem*>& items =
      menu_item_map();
  std::map<NSMenuItem*, HistoryMenuBridge::HistoryItem*>::iterator it =
      items.begin();

  for ( ; it != items.end(); ++it) {
    HistoryMenuBridge::HistoryItem* item = it->second;
    EXPECT_FALSE(controller()->opened_[item->session_id]);
    [controller() openHistoryMenuItem:it->first];
    EXPECT_TRUE(controller()->opened_[item->session_id]);
  }
}
