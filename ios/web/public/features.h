// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FEATURES_H_
#define IOS_WEB_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace web {
namespace features {

// Used to enable asynchronous DOM element fetching for context menu.
extern const base::Feature kContextMenuElementPostMessage;

// Used to enable the WKBackForwardList based navigation manager.
extern const base::Feature kSlimNavigationManager;

// Used to enable PassKit download on top of ios/web Download API.
extern const base::Feature kNewPassKitDownload;

// Used to enable new Download Manager UI and backend.
extern const base::Feature kNewFileDownload;

// Used to enable using WKHTTPSystemCookieStore in main context URL requests.
extern const base::Feature kWKHTTPSystemCookieStore;

// Used to enable web view preloads when navigating from NativeContent to
// web content on iOS 11.3. Preload will make NativeContent -> web content UI
// transitioning smoother by inserting web view only after the navigation is
// started. On iOS 11.3 web view may load the page slower if it is not a part of
// the view hierarchy.
extern const base::Feature
    kPreloadWebViewWhenNavigatingFromNativeContentOnIOS11_3;

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_PUBLIC_FEATURES_H_
