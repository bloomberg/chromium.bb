// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// The accessibility identifier of the privacy settings collection view.
extern NSString* const kClearBrowsingDataCollectionViewId;

// The accessibility identifiers of the cells in the collection view.
extern NSString* const kClearBrowsingHistoryCellId;
extern NSString* const kClearCookiesCellId;
extern NSString* const kClearCacheCellId;
extern NSString* const kClearSavedPasswordsCellId;
extern NSString* const kClearAutofillCellId;

// CollectionView for clearing browsing data (including history,
// cookies, caches, passwords, and autofill).
@interface ClearBrowsingDataCollectionViewController
    : SettingsRootCollectionViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_COLLECTION_VIEW_CONTROLLER_H_
