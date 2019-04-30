// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/leveldb_proto/web_view_proto_database_provider_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace leveldb_proto {

// static
WebViewProtoDatabaseProviderFactory*
WebViewProtoDatabaseProviderFactory::GetInstance() {
  static base::NoDestructor<WebViewProtoDatabaseProviderFactory> instance;
  return instance.get();
}

// static
ProtoDatabaseProvider* WebViewProtoDatabaseProviderFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<ProtoDatabaseProvider*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewProtoDatabaseProviderFactory::WebViewProtoDatabaseProviderFactory()
    : BrowserStateKeyedServiceFactory(
          "leveldb_proto::ProtoDatabaseProvider",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewProtoDatabaseProviderFactory::~WebViewProtoDatabaseProviderFactory() =
    default;

std::unique_ptr<KeyedService>
WebViewProtoDatabaseProviderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<ProtoDatabaseProvider>(context->GetStatePath());
}

}  // namespace leveldb_proto