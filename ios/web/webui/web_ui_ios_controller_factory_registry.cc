// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/webui/web_ui_ios_controller_factory_registry.h"

#include "base/lazy_instance.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web {

base::LazyInstance<std::vector<WebUIIOSControllerFactory*> > g_factories =
    LAZY_INSTANCE_INITIALIZER;

void WebUIIOSControllerFactory::RegisterFactory(
    WebUIIOSControllerFactory* factory) {
  g_factories.Pointer()->push_back(factory);
}

WebUIIOSControllerFactoryRegistry*
WebUIIOSControllerFactoryRegistry::GetInstance() {
  return base::Singleton<WebUIIOSControllerFactoryRegistry>::get();
}

WebUIIOSController*
WebUIIOSControllerFactoryRegistry::CreateWebUIIOSControllerForURL(
    WebUIIOS* web_ui,
    const GURL& url) const {
  std::vector<WebUIIOSControllerFactory*>* factories = g_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    WebUIIOSController* controller =
        (*factories)[i]->CreateWebUIIOSControllerForURL(web_ui, url);
    if (controller)
      return controller;
  }
  return NULL;
}

WebUIIOSControllerFactoryRegistry::WebUIIOSControllerFactoryRegistry() {
}

WebUIIOSControllerFactoryRegistry::~WebUIIOSControllerFactoryRegistry() {
}

}  // namespace web
