// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_controller_factory_registry.h"

#include "base/lazy_instance.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"

namespace content {

base::LazyInstance<std::vector<WebUIControllerFactory*> > g_factories =
    LAZY_INSTANCE_INITIALIZER;

void WebUIControllerFactory::RegisterFactory(WebUIControllerFactory* factory) {
  g_factories.Pointer()->push_back(factory);
}

void WebUIControllerFactory::UnregisterFactoryForTesting(
    WebUIControllerFactory* factory) {
  std::vector<WebUIControllerFactory*>* factories = g_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    if ((*factories)[i] == factory) {
      factories->erase(factories->begin() + i);
      return;
    }
  }
  NOTREACHED() << "Tried to unregister a factory but it wasn't found";
}

WebUIControllerFactoryRegistry* WebUIControllerFactoryRegistry::GetInstance() {
  return Singleton<WebUIControllerFactoryRegistry>::get();
}

WebUIController* WebUIControllerFactoryRegistry::CreateWebUIControllerForURL(
    WebUI* web_ui, const GURL& url) const {
  std::vector<WebUIControllerFactory*>* factories = g_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    WebUIController* controller = (*factories)[i]->CreateWebUIControllerForURL(
        web_ui, url);
    if (controller)
      return controller;
  }
  return NULL;
}

WebUI::TypeID WebUIControllerFactoryRegistry::GetWebUIType(
    BrowserContext* browser_context, const GURL& url) const {
  std::vector<WebUIControllerFactory*>* factories = g_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    WebUI::TypeID type = (*factories)[i]->GetWebUIType(browser_context, url);
    if (type != WebUI::kNoWebUI)
      return type;
  }
  return WebUI::kNoWebUI;
}

bool WebUIControllerFactoryRegistry::UseWebUIForURL(
    BrowserContext* browser_context, const GURL& url) const {
  std::vector<WebUIControllerFactory*>* factories = g_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    if ((*factories)[i]->UseWebUIForURL(browser_context, url))
      return true;
  }
  return false;
}

bool WebUIControllerFactoryRegistry::UseWebUIBindingsForURL(
    BrowserContext* browser_context, const GURL& url) const {
  std::vector<WebUIControllerFactory*>* factories = g_factories.Pointer();
  for (size_t i = 0; i < factories->size(); ++i) {
    if ((*factories)[i]->UseWebUIBindingsForURL(browser_context, url))
      return true;
  }
  return false;
}

bool WebUIControllerFactoryRegistry::IsURLAcceptableForWebUI(
    BrowserContext* browser_context,
    const GURL& url,
    bool data_urls_allowed) const {
  return UseWebUIForURL(browser_context, url) ||
      // javascript: URLs are allowed to run in Web UI pages.
      url.SchemeIs(chrome::kJavaScriptScheme) ||
      // It's possible to load about:blank in a Web UI renderer.
      // See http://crbug.com/42547
      url.spec() == chrome::kAboutBlankURL ||
      // Chrome URLs crash, kill, hang, and shorthang are allowed.
      url == GURL(kChromeUICrashURL) ||
      url == GURL(kChromeUIKillURL) ||
      url == GURL(kChromeUIHangURL) ||
      url == GURL(kChromeUIShorthangURL) ||
      // Data URLs are usually not allowed in WebUI for security reasons.
      // BalloonHosts are one exception needed by ChromeOS, and are safe because
      // they cannot be scripted by other pages.
      (data_urls_allowed && url.SchemeIs(chrome::kDataScheme));
}

WebUIControllerFactoryRegistry::WebUIControllerFactoryRegistry() {
}

WebUIControllerFactoryRegistry::~WebUIControllerFactoryRegistry() {
}

}  // namespace content
