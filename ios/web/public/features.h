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

// Used to enable API to send messages directly to frames of a webpage.
extern const base::Feature kWebFrameMessaging;

// Used to enable the WKBackForwardList based navigation manager.
extern const base::Feature kSlimNavigationManager;

// Used to enable displaying error pages in WebState by loading HTML string.
extern const base::Feature kWebErrorPages;

// If enabled the CRWCertVerificationController will use WebThread::PostTask
// instead of GCD. GCD API was used to make sure that completion callbacks are
// called during the app shutdown, which may be unnecessary
// (https://crbug.com/853774).
extern const base::Feature kUseWebThreadInCertVerificationController;

// Used to enable using WKHTTPSystemCookieStore in main context URL requests.
extern const base::Feature kWKHTTPSystemCookieStore;

// Used to crash the browser if unexpected URL change is detected.
// https://crbug.com/841105.
extern const base::Feature kCrashOnUnexpectedURLChange;

// Used to make BrowserContainerViewController fullscreen.
extern const base::Feature kBrowserContainerFullscreen;

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_PUBLIC_FEATURES_H_
