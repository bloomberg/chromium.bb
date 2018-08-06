// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller_model_delegate.h"

class AuthenticationService;
@class GoogleServicesSettingsViewController;
class PrefService;

// Mediator for the Google services settings.
@interface GoogleServicesSettingsMediator
    : NSObject<GoogleServicesSettingsCommandHandler,
               GoogleServicesSettingsViewControllerModelDelegate>

// Designated initializer. |prefService| should not be null.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// View controller.
@property(nonatomic, weak) id<GoogleServicesSettingsConsumer> consumer;
// Authentication service.
@property(nonatomic, assign) AuthenticationService* authService;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_
