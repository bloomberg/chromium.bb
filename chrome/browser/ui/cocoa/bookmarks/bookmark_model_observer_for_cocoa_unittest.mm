// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_model_observer_for_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

// Keep track of bookmark pings.
@interface ObserverPingTracker : NSObject {
 @public
  int pings;
}
@end

@implementation ObserverPingTracker
- (void)pingMe:(id)sender {
  pings++;
}
@end

namespace {

class BookmarkModelObserverForCocoaTest : public CocoaTest {
 public:
  BrowserTestHelper helper_;

  BookmarkModelObserverForCocoaTest() {}
  virtual ~BookmarkModelObserverForCocoaTest() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkModelObserverForCocoaTest);
};


TEST_F(BookmarkModelObserverForCocoaTest, TestCallback) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* node = model->AddURL(model->GetBookmarkBarNode(),
                                           0, ASCIIToUTF16("super"),
                                           GURL("http://www.google.com"));

  scoped_nsobject<ObserverPingTracker>
      pingCount([[ObserverPingTracker alloc] init]);

  scoped_ptr<BookmarkModelObserverForCocoa>
      observer(new BookmarkModelObserverForCocoa(node, model,
                                                 pingCount,
                                                 @selector(pingMe:)));

  EXPECT_EQ(0, pingCount.get()->pings);

  model->SetTitle(node, ASCIIToUTF16("duper"));
  EXPECT_EQ(1, pingCount.get()->pings);
  model->SetURL(node, GURL("http://www.google.com/reader"));
  EXPECT_EQ(2, pingCount.get()->pings);

  model->Move(node, model->other_node(), 0);
  EXPECT_EQ(3, pingCount.get()->pings);

  model->Remove(node->parent(), 0);
  EXPECT_EQ(4, pingCount.get()->pings);
}

}  // namespace
