// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_view.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

namespace {

class SadTabViewTest : public CocoaTest {
 public:
  SadTabViewTest() {
    NSRect content_frame = [[test_window() contentView] frame];
    scoped_nsobject<SadTabView> view([[SadTabView alloc]
                                      initWithFrame:content_frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  SadTabView* view_;  // Weak. Owned by the view hierarchy.
};

TEST_VIEW(SadTabViewTest, view_);

}  // namespace
