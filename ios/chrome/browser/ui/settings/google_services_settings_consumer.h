// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"

// Consumer protocol for Google services settings.
@protocol GoogleServicesSettingsConsumer<NSObject>

// Returns the collection view model.
@property(nonatomic, strong, readonly)
    CollectionViewModel<CollectionViewItem*>* collectionViewModel;

// Reloads |sections|.
- (void)reloadSections:(NSIndexSet*)sections;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_CONSUMER_H_
