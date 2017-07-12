// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_main_delegate.h"

#import "base/mac/bundle_locations.h"
#import "ios/web_view/public/cwv_html_element.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebMainDelegate::WebViewWebMainDelegate() {}

WebViewWebMainDelegate::~WebViewWebMainDelegate() = default;

void WebViewWebMainDelegate::BasicStartupComplete() {
  // Use CWVHTMLElement instead of CWVWebView and CWVWebViewConfiguration
  // because the latter two classes' +intialize calls in to this method and may
  // cause a deadlock.
  base::mac::SetOverrideFrameworkBundle(
      [NSBundle bundleForClass:[CWVHTMLElement class]]);
}

}  // namespace ios_web_view
