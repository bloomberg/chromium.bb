// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_TEST_INFOBAR_VIEW_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_TEST_INFOBAR_VIEW_H_

#import "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"

#import <UIKit/UIKit.h>

@interface TestInfoBarView : UIView<InfoBarViewProtocol>
@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_TEST_INFOBAR_VIEW_H_
