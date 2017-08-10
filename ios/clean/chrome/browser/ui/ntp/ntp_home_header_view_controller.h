// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_provider.h"

@protocol LogoVendor;

// Coordinator handling the header of the NTP home panel.
@interface NTPHomeHeaderViewController
    : UIViewController<ContentSuggestionsHeaderProvider>

// Exposes view and methods to drive the doodle.
@property(nonatomic, weak) id<LogoVendor> logoVendor;

- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled;
// If Google is not the default search engine, hide the logo, doodle and
// fakebox. Make them appear if Google is set as default.
- (void)setLogoIsShowing:(BOOL)logoIsShowing;
// Updates the iPhone omnibox's frame based on the current scroll view |offset|.
- (void)updateFakeOmniboxForOffset:(CGFloat)offset;
// Updates the iPhone omnibox's frame based on the |width|. Does not take the
// scrolling into account.
- (void)updateFakeOmniboxForWidth:(CGFloat)width;
// Notifies that the collection will shift down.
- (void)collectionWillShiftDown;
// Notifies that the collection will shift up.
- (void)collectionDidShiftUp;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_HEADER_VIEW_CONTROLLER_H_
