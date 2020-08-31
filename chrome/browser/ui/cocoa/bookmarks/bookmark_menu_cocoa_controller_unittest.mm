// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"

#import "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface FakeBookmarkMenuController : BookmarkMenuCocoaController {
 @public
  const BookmarkNode* _nodes[2];
  BOOL _opened[2];
}
- (id)initWithProfile:(Profile*)profile;
@end

@implementation FakeBookmarkMenuController

- (id)initWithProfile:(Profile*)profile {
  if ((self = [super init])) {
    base::string16 empty;
    BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
    const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
    _nodes[0] = model->AddURL(bookmark_bar, 0, empty, GURL("http://0.com"));
    _nodes[1] = model->AddURL(bookmark_bar, 1, empty, GURL("http://1.com"));
  }
  return self;
}

- (const BookmarkNode*)nodeForIdentifier:(int)identifier {
  if ((identifier < 0) || (identifier >= 2))
    return NULL;
  return _nodes[identifier];
}

- (void)openURLForNode:(const BookmarkNode*)node {
  std::string url = node->url().possibly_invalid_spec();
  if (url.find("http://0.com") != std::string::npos)
    _opened[0] = YES;
  if (url.find("http://1.com") != std::string::npos)
    _opened[1] = YES;
}

@end  // FakeBookmarkMenuController

class BookmarkMenuCocoaControllerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    profile()->CreateBookmarkModel(true);
    bookmarks::test::WaitForBookmarkModelToLoad(
        BookmarkModelFactory::GetForBrowserContext(profile()));
    controller_.reset(
        [[FakeBookmarkMenuController alloc] initWithProfile:profile()]);
  }

  FakeBookmarkMenuController* controller() { return controller_.get(); }

 private:
  CocoaTestHelper cocoa_test_helper_;
  base::scoped_nsobject<FakeBookmarkMenuController> controller_;
};

TEST_F(BookmarkMenuCocoaControllerTest, TestOpenItem) {
  FakeBookmarkMenuController* c = controller();
  NSMenuItem *item = [[[NSMenuItem alloc] init] autorelease];
  for (int i = 0; i < 2; i++) {
    [item setTag:i];
    ASSERT_EQ(c->_opened[i], NO);
    [c openBookmarkMenuItem:item];
    ASSERT_NE(c->_opened[i], NO);
  }
}
