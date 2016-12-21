// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/fancy_ui/bidi_container_view.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_protocol.h"

class InfoBarViewDelegate;
@protocol InfoBarViewProtocol;

// UIView representing a single infobar.
@interface InfoBarView : BidiContainerView<InfoBarViewProtocol>
- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(InfoBarViewDelegate*)delegate;
@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_VIEW_H_
