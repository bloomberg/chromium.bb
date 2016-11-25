// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/web/web_controller_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

WebControllerProvider::WebControllerProvider(web::BrowserState* browser_state) {
}

WebControllerProvider::~WebControllerProvider() {}

bool WebControllerProvider::SuppressesDialogs() const {
  return false;
}

web::WebState* WebControllerProvider::GetWebState() const {
  return nullptr;
}

void WebControllerProvider::InjectScript(
    const std::string& script,
    web::JavaScriptResultBlock completion) {
  if (completion)
    completion(nil, nil);
}

}  // namespace ios
