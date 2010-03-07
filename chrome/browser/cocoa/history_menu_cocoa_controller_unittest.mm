// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_vector.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/history_menu_bridge.h"
#include "chrome/browser/cocoa/history_menu_cocoa_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface FakeHistoryMenuController : HistoryMenuCocoaController {
 @public
  BOOL opened_[2];
  ScopedVector<HistoryMenuBridge::HistoryItem> items_;
}
@end

@implementation FakeHistoryMenuController

- (id)init {
  if ((self = [super init])) {
    opened_[0] = NO;
    opened_[1] = NO;
  }
  return self;
}

- (HistoryMenuBridge::HistoryItem*)itemForTag:(NSInteger)tag {
  HistoryMenuBridge::HistoryItem* item = new HistoryMenuBridge::HistoryItem();
  if (tag == 0) {
    item->title = ASCIIToUTF16("uno");
    item->url = GURL("http://google.com");
  } else if (tag == 1) {
    item->title = ASCIIToUTF16("duo");
    item->url = GURL("http://apple.com");
  } else {
    NOTREACHED();
  }
  // We push the item into a scoped vector that will delete it on destruction.
  items_.push_back(item);
  return item;
}

- (void)openURLForItem:(HistoryMenuBridge::HistoryItem*)item {
  std::string url = item->url.possibly_invalid_spec();
  if (url.find("http://google.com") != std::string::npos)
    opened_[0] = YES;
  if (url.find("http://apple.com") != std::string::npos)
    opened_[1] = YES;
}

@end  // FakeHistoryMenuController

TEST(HistoryMenuCocoaControllerTest, OpenURLForItem) {
  FakeHistoryMenuController* c = [[FakeHistoryMenuController alloc] init];
  NSMenuItem* item = [[[NSMenuItem alloc] init] autorelease];
  for (int i = 0; i < 2; ++i) {
    [item setTag:i];
    ASSERT_EQ(c->opened_[i], NO);
    [c openHistoryMenuItem:item];
    ASSERT_EQ(c->opened_[i], YES);
  }
  [c release];
}
