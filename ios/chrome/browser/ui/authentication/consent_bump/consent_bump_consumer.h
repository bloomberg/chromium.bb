// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for the ConsentBump
@protocol ConsentBumpConsumer

// Sets the title of the primary button of the ConsentBump. By default the
// primary button isn't visible.
- (void)setPrimaryButtonTitle:(NSString*)primaryButtonTitle;
// Sets the title of the secondary button of the ConsentBump.
- (void)setSecondaryButtonTitle:(NSString*)secondaryButtonTitle;
// Shows the primary button on the ConsentBump. There are no way to hide it once
// it is shown.
- (void)showPrimaryButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_CONSUMER_H_
