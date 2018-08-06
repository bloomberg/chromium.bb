// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_PERSONALIZATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_PERSONALIZATION_COORDINATOR_H_

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_option_type.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Coordinator for handling the ConsentBump Personalization screen.
@interface ConsentBumpPersonalizationCoordinator : ChromeCoordinator

// Type of the selected option.
@property(nonatomic, assign, readonly) ConsentBumpOptionType selectedOption;

// The view Controller for this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_PERSONALIZATION_COORDINATOR_H_
