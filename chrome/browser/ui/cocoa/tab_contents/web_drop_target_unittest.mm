// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/tab_contents/web_drop_target.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "webkit/glue/webdropdata.h"

class WebDropTargetTest : public RenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    CocoaTest::BootstrapCocoa();
    drop_target_.reset([[WebDropTarget alloc] initWithTabContents:contents()]);
  }

  void PutURLOnPasteboard(NSString* urlString, NSPasteboard* pboard) {
    [pboard declareTypes:[NSArray arrayWithObject:NSURLPboardType]
                   owner:nil];
    NSURL* url = [NSURL URLWithString:urlString];
    EXPECT_TRUE(url);
    [url writeToPasteboard:pboard];
  }

  void PutCoreURLAndTitleOnPasteboard(NSString* urlString, NSString* title,
                                      NSPasteboard* pboard) {
    [pboard
        declareTypes:[NSArray arrayWithObjects:kCorePasteboardFlavorType_url,
                                               kCorePasteboardFlavorType_urln,
                                               nil]
               owner:nil];
    [pboard setString:urlString
              forType:kCorePasteboardFlavorType_url];
    [pboard setString:title
              forType:kCorePasteboardFlavorType_urln];
  }

  base::mac::ScopedNSAutoreleasePool pool_;
  scoped_nsobject<WebDropTarget> drop_target_;
};

// Make sure nothing leaks.
TEST_F(WebDropTargetTest, Init) {
  EXPECT_TRUE(drop_target_);
}

// Test flipping of coordinates given a point in window coordinates.
TEST_F(WebDropTargetTest, Flip) {
  NSPoint windowPoint = NSZeroPoint;
  scoped_nsobject<NSWindow> window([[CocoaTestHelperWindow alloc] init]);
  NSPoint viewPoint =
      [drop_target_ flipWindowPointToView:windowPoint
                                     view:[window contentView]];
  NSPoint screenPoint =
      [drop_target_ flipWindowPointToScreen:windowPoint
                               view:[window contentView]];
  EXPECT_EQ(0, viewPoint.x);
  EXPECT_EQ(600, viewPoint.y);
  EXPECT_EQ(0, screenPoint.x);
  // We can't put a value on the screen size since everyone will have a
  // different one.
  EXPECT_NE(0, screenPoint.y);
}

TEST_F(WebDropTargetTest, URL) {
  NSPasteboard* pboard = nil;
  NSString* url = nil;
  NSString* title = nil;
  GURL result_url;
  string16 result_title;

  // Put a URL on the pasteboard and check it.
  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"http://www.google.com/";
  PutURLOnPasteboard(url, pboard);
  EXPECT_TRUE([drop_target_ populateURL:&result_url
                               andTitle:&result_title
                         fromPasteboard:pboard
                    convertingFilenames:NO]);
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  [pboard releaseGlobally];

  // Put a 'url ' and 'urln' on the pasteboard and check it.
  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"http://www.google.com/";
  title = @"Title of Awesomeness!",
  PutCoreURLAndTitleOnPasteboard(url, title, pboard);
  EXPECT_TRUE([drop_target_ populateURL:&result_url
                               andTitle:&result_title
                         fromPasteboard:pboard
                    convertingFilenames:NO]);
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);
  [pboard releaseGlobally];

  // Also check that it passes file:// via 'url '/'urln' properly.
  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"file:///tmp/dont_delete_me.txt";
  title = @"very important";
  PutCoreURLAndTitleOnPasteboard(url, title, pboard);
  EXPECT_TRUE([drop_target_ populateURL:&result_url
                               andTitle:&result_title
                         fromPasteboard:pboard
                    convertingFilenames:NO]);
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);
  [pboard releaseGlobally];

  // And javascript:.
  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"javascript:open('http://www.youtube.com/')";
  title = @"kill some time";
  PutCoreURLAndTitleOnPasteboard(url, title, pboard);
  EXPECT_TRUE([drop_target_ populateURL:&result_url
                               andTitle:&result_title
                         fromPasteboard:pboard
                    convertingFilenames:NO]);
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);
  [pboard releaseGlobally];

  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"/bin/sh";
  [pboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType]
                 owner:nil];
  [pboard setPropertyList:[NSArray arrayWithObject:url]
                  forType:NSFilenamesPboardType];
  EXPECT_FALSE([drop_target_ populateURL:&result_url
                                andTitle:&result_title
                          fromPasteboard:pboard
                    convertingFilenames:NO]);
  EXPECT_TRUE([drop_target_ populateURL:&result_url
                               andTitle:&result_title
                         fromPasteboard:pboard
                    convertingFilenames:YES]);
  EXPECT_EQ("file://localhost/bin/sh", result_url.spec());
  EXPECT_EQ("sh", UTF16ToUTF8(result_title));
  [pboard releaseGlobally];
}

TEST_F(WebDropTargetTest, Data) {
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
  EXPECT_EQ(base::SysNSStringToUTF16(textString), data.plain_text);
  EXPECT_EQ(base::SysNSStringToUTF16(htmlString), data.text_html);

  [pboard releaseGlobally];
}
