// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/web_drop_target.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"
#include "webkit/glue/webdropdata.h"

class WebDropTargetTest : public RenderViewHostTestHarness {
 public:
  WebDropTargetTest() {
   RenderViewHostTestHarness::SetUp();
   drop_target_.reset([[WebDropTarget alloc] initWithTabContents:contents()]);
  }

  virtual ~WebDropTargetTest() {
   RenderViewHostTestHarness::TearDown();

  }

  void PutURLOnPasteboard(NSString* urlString, NSPasteboard* pboard) {
    [pboard declareTypes:[NSArray arrayWithObject:NSURLPboardType]
                   owner:nil];
    NSURL* url = [NSURL URLWithString:urlString];
    EXPECT_TRUE(url);
    [url writeToPasteboard:pboard];
  }

  base::ScopedNSAutoreleasePool pool_;
  CocoaTestHelper cocoa_helper_;
  scoped_nsobject<WebDropTarget> drop_target_;
};

// Make sure nothing leaks.
TEST_F(WebDropTargetTest, DISABLED_Init) {
  EXPECT_TRUE(drop_target_);
}

// Test flipping of coordinates given a point in window coordinates.
TEST_F(WebDropTargetTest, DISABLED_Flip) {
  NSPoint windowPoint = NSZeroPoint;
  NSPoint viewPoint =
      [drop_target_ flipWindowPointToView:windowPoint
                                     view:cocoa_helper_.contentView()];
  NSPoint screenPoint =
      [drop_target_ flipWindowPointToScreen:windowPoint
                               view:cocoa_helper_.contentView()];
  EXPECT_EQ(viewPoint.x, 0);
  EXPECT_EQ(viewPoint.y, 600);
  EXPECT_EQ(screenPoint.x, 0);
  // We can't put a value on the screen size since everyone will have a
  // different one.
  EXPECT_NE(screenPoint.y, 0);
}

TEST_F(WebDropTargetTest, DISABLED_URL) {
  WebDropData data;

  // Put a URL on the pasteboard and check it.
  NSPasteboard* pboard = [NSPasteboard pasteboardWithUniqueName];
  PutURLOnPasteboard(@"http://www.google.com", pboard);
  [drop_target_ populateURLAndTitle:&data fromPasteboard:pboard];
  EXPECT_EQ(data.url.spec(), "http://www.google.com/");
  [pboard releaseGlobally];

  // Put a 'url ' and 'urln' on the pasteboard and check it.
  NSString* title = @"Title of Awesomeness!";
  pboard = [NSPasteboard pasteboardWithUniqueName];
  [pboard declareTypes:[NSArray arrayWithObjects:kCorePasteboardFlavorType_url,
                                        kCorePasteboardFlavorType_urln, nil]
                 owner:nil];
  [pboard setString:@"http://www.something.com/"
            forType:kCorePasteboardFlavorType_url];
  [pboard setString:title
            forType:kCorePasteboardFlavorType_urln];
  [drop_target_ populateURLAndTitle:&data fromPasteboard:pboard];
  EXPECT_EQ(data.url.spec(), "http://www.something.com/");
  EXPECT_EQ(data.url_title, base::SysNSStringToUTF16(title));

  [pboard releaseGlobally];
}

TEST_F(WebDropTargetTest, DISABLED_Data) {
  WebDropData data;
  NSPasteboard* pboard = [NSPasteboard pasteboardWithUniqueName];

  PutURLOnPasteboard(@"http://www.google.com", pboard);
  [pboard addTypes:[NSArray arrayWithObjects:NSHTMLPboardType,
                              NSStringPboardType, nil]
             owner:nil];
  NSString* htmlString = @"<html><body><b>hi there</b></body></html>";
  NSString* textString = @"hi there";
  [pboard setString:htmlString forType:NSHTMLPboardType];
  [pboard setString:textString forType:NSStringPboardType];
  [drop_target_ populateWebDropData:&data fromPasteboard:pboard];
  EXPECT_EQ(data.url.spec(), "http://www.google.com/");
  EXPECT_EQ(data.plain_text, base::SysNSStringToUTF16(textString));
  EXPECT_EQ(data.text_html, base::SysNSStringToUTF16(htmlString));

  [pboard releaseGlobally];
}

TEST_F(WebDropTargetTest, DISABLED_EnterExitDrop) {
  // TODO(pinkerton): Call enter/exit/drop and see what happens. This is a bit
  // harder to test.
}
