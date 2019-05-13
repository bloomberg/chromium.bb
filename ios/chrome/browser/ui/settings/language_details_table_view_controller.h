// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_DETAILS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_DETAILS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@class LanguageDetailsTableViewController;

@protocol LanguageDetailsTableViewControllerDelegate

// Informs the delegate that user selected whether or not to offer Translate for
// |languageCode|.
- (void)languageDetailsTableViewController:
            (LanguageDetailsTableViewController*)tableViewController
                   didSelectOfferTranslate:(BOOL)offerTranslate
                              languageCode:(NSString*)languageCode;

@end

// Controller for the UI that allows the user to change the settings for a given
// language, i.e., to choose whether or not Translate should be offered for it.
@interface LanguageDetailsTableViewController : SettingsRootTableViewController

// The designated initializer.
- (instancetype)initWithLanguageCode:(NSString*)languageCode
                        languageName:(NSString*)languageName
                             blocked:(BOOL)blocked
                   canOfferTranslate:(BOOL)canOfferTranslate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

@property(nonatomic, weak) id<LanguageDetailsTableViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_DETAILS_TABLE_VIEW_CONTROLLER_H_
