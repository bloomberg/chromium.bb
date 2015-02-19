// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/web/web_controller_provider.h"

namespace ios {

static WebControllerProviderFactory* g_web_controller_provider_factory;

void SetWebControllerProviderFactory(
    WebControllerProviderFactory* provider_factory) {
  g_web_controller_provider_factory = provider_factory;
}

WebControllerProviderFactory* GetWebControllerProviderFactory() {
  return g_web_controller_provider_factory;
}

WebControllerProvider::WebControllerProvider(web::BrowserState* browser_state) {
}

WebControllerProvider::~WebControllerProvider() {
}

bool WebControllerProvider::SuppressesDialogs() const {
  return false;
}

web::WebState* WebControllerProvider::GetWebState() const {
  return nullptr;
}

void WebControllerProvider::InjectScript(const std::string& script,
                                         web::JavaScriptCompletion completion) {
  if (completion)
    completion(nil, nil);
}

WebControllerProviderFactory::WebControllerProviderFactory() {
}

WebControllerProviderFactory::~WebControllerProviderFactory() {
}

scoped_ptr<WebControllerProvider>
WebControllerProviderFactory::CreateWebControllerProvider(
    web::BrowserState* browser_state) {
  return scoped_ptr<WebControllerProvider>(
      new WebControllerProvider(browser_state));
}

}  // namespace ios
