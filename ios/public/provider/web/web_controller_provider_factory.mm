// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/web/web_controller_provider_factory.h"

#include "base/memory/ptr_util.h"
#import "ios/public/provider/web/web_controller_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace {
WebControllerProviderFactory* g_web_controller_provider_factory;
}

WebControllerProviderFactory::WebControllerProviderFactory() {}

WebControllerProviderFactory::~WebControllerProviderFactory() {}

std::unique_ptr<WebControllerProvider>
WebControllerProviderFactory::CreateWebControllerProvider(
    web::BrowserState* browser_state) {
  return base::MakeUnique<WebControllerProvider>(browser_state);
}

void SetWebControllerProviderFactory(
    WebControllerProviderFactory* provider_factory) {
  g_web_controller_provider_factory = provider_factory;
}

WebControllerProviderFactory* GetWebControllerProviderFactory() {
  return g_web_controller_provider_factory;
}

}  // namespace ios
