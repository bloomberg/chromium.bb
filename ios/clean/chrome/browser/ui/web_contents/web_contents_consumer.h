// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_CONSUMER_H_

#import <UIKit/UIKit.h>

// A WebContentsConsumer (typically a view controller) uses data provided by
// this protocol to display a web view.
@protocol WebContentsConsumer
// Called when the content view that the receiver should display changes.
- (void)contentViewDidChange:(UIView*)contentView;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_CONSUMER_H_
