// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTEXTUAL_SEARCH_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTEXTUAL_SEARCH_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@class TouchToSearchPermissionsMediator;

@interface ContextualSearchCollectionViewController
    : SettingsRootCollectionViewController

// Designated initializer.
- (instancetype)initWithPermissions:
    (TouchToSearchPermissionsMediator*)touchToSearchPermissions
    NS_DESIGNATED_INITIALIZER;

// Initializes an instance of this class, creating a permissions object from
// |browserState|.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTEXTUAL_SEARCH_COLLECTION_VIEW_CONTROLLER_H_
