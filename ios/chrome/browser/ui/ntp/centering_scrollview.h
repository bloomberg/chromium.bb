// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_CENTERING_SCROLLVIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_CENTERING_SCROLLVIEW_H_

#import <UIKit/UIKit.h>

// A CenteringScrollView is a UIScrollView that re-centers its one and only
// content subview.
// If the scroll view is larger than the content subview, the content subview
// will be placed in the center of the scroll view and the scroll view will not
// be scrollable. If the scroll view frame is smaller than the content subview,
// the content view  will be placed flushed to the top-left edge and the
// scroll view will be scrollable.
@interface CenteringScrollView : UIScrollView

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_CENTERING_SCROLLVIEW_H_
