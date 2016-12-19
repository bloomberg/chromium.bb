// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NATIVE_APPS_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NATIVE_APPS_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/storekit_launcher.h"
#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

// Settings view controller that displays the list of native apps that Chrome
// can launch given a specific URL. From this view controller, native apps can
// be installed if they are not present on the device. If they are present, a
// switch lets the user opt-in to open automatically related links.
// Icons for apps are static resources on a server and are therefore retrieved
// and displayed asynchronously.
@interface NativeAppsCollectionViewController
    : SettingsRootCollectionViewController<StoreKitLauncher>

// Designated initializer. |requestContextGetter| is kept as a weak reference,
// therefore must outlive the initialized instance.
- (id)initWithURLRequestContextGetter:
    (net::URLRequestContextGetter*)requestContextGetter;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NATIVE_APPS_COLLECTION_VIEW_CONTROLLER_H_
