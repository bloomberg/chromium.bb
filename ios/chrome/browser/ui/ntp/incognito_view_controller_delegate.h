// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_PROTOCOL_H_

@protocol IncognitoViewControllerDelegate
// Sets the alpha for the toolbar's background views.
- (void)setToolbarBackgroundAlpha:(CGFloat)alpha;
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_H_
