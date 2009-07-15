// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

// Pretend BookmarkURLOpener delegate to keep track of requests
@interface BookmarkURLOpenerPong : NSObject<BookmarkURLOpener> {
 @public
  GURL url_;
  WindowOpenDisposition disposition_;
}
@end

@implementation BookmarkURLOpenerPong
- (void)openBookmarkURL:(const GURL&)url
            disposition:(WindowOpenDisposition)disposition {
  url_ = url;
  disposition_ = disposition;
}
@end


namespace {

static const int kContentAreaHeight = 500;

class BookmarkBarControllerTest : public testing::Test {
 public:
  BookmarkBarControllerTest() {
    NSRect content_frame = NSMakeRect(0, 0, 800, kContentAreaHeight);
    NSRect parent_frame = NSMakeRect(0, 0, 800, 50);
    content_area_.reset([[NSView alloc] initWithFrame:content_frame]);
    parent_view_.reset([[NSView alloc] initWithFrame:parent_frame]);
    [parent_view_ setHidden:YES];
    bar_.reset(
        [[BookmarkBarController alloc] initWithProfile:helper_.profile()
                                            parentView:parent_view_.get()
                                        webContentView:content_area_.get()
                                              delegate:nil]);
    [bar_ view];  // force loading of the nib
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSView> content_area_;
  scoped_nsobject<NSView> parent_view_;
  BrowserTestHelper helper_;
  scoped_nsobject<BookmarkBarController> bar_;
};

TEST_F(BookmarkBarControllerTest, ShowHide) {
  // Assume hidden by default in a new profile.
  EXPECT_FALSE([bar_ isBookmarkBarVisible]);
  EXPECT_TRUE([[bar_ view] isHidden]);

  // Awkwardness to look like we've been installed.
  [parent_view_ addSubview:[bar_ view]];
  NSRect frame = [[[bar_ view] superview] frame];
  frame.origin.y = 100;
  [[[bar_ view] superview] setFrame:frame];

  // Show and hide it by toggling.
  [bar_ toggleBookmarkBar];
  EXPECT_TRUE([bar_ isBookmarkBarVisible]);
  EXPECT_FALSE([[bar_ view] isHidden]);
  NSRect content_frame = [content_area_ frame];
  EXPECT_NE(content_frame.size.height, kContentAreaHeight);
  EXPECT_GT([[bar_ view] frame].size.height, 0);

  [bar_ toggleBookmarkBar];
  EXPECT_FALSE([bar_ isBookmarkBarVisible]);
  EXPECT_TRUE([[bar_ view] isHidden]);
  content_frame = [content_area_ frame];
  EXPECT_EQ(content_frame.size.height, kContentAreaHeight);
  EXPECT_EQ([[bar_ view] frame].size.height, 0);
}

// Confirm openBookmark: forwards the request to the controller's delegate
TEST_F(BookmarkBarControllerTest, OpenBookmark) {
  GURL gurl("http://walla.walla.ding.dong.com");
  scoped_ptr<BookmarkNode> node(new BookmarkNode(gurl));
  scoped_nsobject<BookmarkURLOpenerPong> pong([[BookmarkURLOpenerPong alloc]
                                                init]);
  [bar_ setDelegate:pong.get()];

  scoped_nsobject<NSButtonCell> cell([[NSButtonCell alloc] init]);
  scoped_nsobject<NSButton> button([[NSButton alloc] init]);
  [button setCell:cell.get()];
  [cell setRepresentedObject:[NSValue valueWithPointer:node.get()]];

  [bar_ openBookmark:button];
  EXPECT_EQ(pong.get()->url_, node->GetURL());
  EXPECT_EQ(pong.get()->disposition_, CURRENT_TAB);

  [bar_ setDelegate:nil];
}

// Confirm opening of bookmarks works from the menus (different
// dispositions than clicking on the button).
TEST_F(BookmarkBarControllerTest, OpenBookmarkFromMenus) {
  scoped_nsobject<BookmarkURLOpenerPong> pong([[BookmarkURLOpenerPong alloc]
                                                init]);
  [bar_ setDelegate:pong.get()];

  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"I_dont_care"]);
  scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
                                     initWithTitle:@"still_dont_care"
                                            action:NULL
                                     keyEquivalent:@""]);
  scoped_nsobject<NSButtonCell> cell([[NSButtonCell alloc] init]);
  [item setMenu:menu.get()];
  [menu setDelegate:cell];

  const char* urls[] = { "http://walla.walla.ding.dong.com",
                         "http://i_dont_know.com",
                         "http://cee.enn.enn.dot.com" };
  SEL selectors[] = { @selector(openBookmarkInNewForegroundTab:),
                      @selector(openBookmarkInNewWindow:),
                      @selector(openBookmarkInIncognitoWindow:) };
  WindowOpenDisposition dispositions[] = { NEW_FOREGROUND_TAB,
                                           NEW_WINDOW,
                                           OFF_THE_RECORD };
  for (unsigned int i = 0;
       i < sizeof(dispositions)/sizeof(dispositions[0]);
       i++) {
    scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(urls[i])));
    [cell setRepresentedObject:[NSValue valueWithPointer:node.get()]];
    [bar_ performSelector:selectors[i] withObject:item.get()];
    EXPECT_EQ(pong.get()->url_, node->GetURL());
    EXPECT_EQ(pong.get()->disposition_, dispositions[i]);
    [cell setRepresentedObject:nil];
  }
}


// TODO(jrg): Make sure showing the bookmark bar calls loaded: (to
// process bookmarks)
TEST_F(BookmarkBarControllerTest, ShowAndLoad) {
}

// TODO(jrg): Make sure adding 1 bookmark adds 1 subview, and removing
// 1 removes 1 subview.  (We can't test for a simple Clear since there
// will soon be views in here which aren't bookmarks.)
TEST_F(BookmarkBarControllerTest, ViewChanges) {
}

// TODO(jrg): Make sure loaded: does something useful
TEST_F(BookmarkBarControllerTest, Loaded) {
  // Clear; make sure no views
  // Call loaded:
  // Make sure subviews
}

// TODO(jrg): Test cellForBookmarkNode:
TEST_F(BookmarkBarControllerTest, Cell) {
}

// TODO(jrg): Test frameForBookmarkAtIndex
TEST_F(BookmarkBarControllerTest, FrameAtIndex) {
}

TEST_F(BookmarkBarControllerTest, Contents) {
  // TODO(jrg): addNodesToBar has a LOT of TODOs; when flushed out, write
  // appropriate tests.
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarControllerTest, Display) {
  [[bar_ view] display];
}

}  // namespace
