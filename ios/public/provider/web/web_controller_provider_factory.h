// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_WEB_CONTROLLER_PROVIDER_FACTORY_H_
#define IOS_PUBLIC_PROVIDER_WEB_WEB_CONTROLLER_PROVIDER_FACTORY_H_

#include <memory>

namespace web {
class BrowserState;
}

namespace ios {

class WebControllerProvider;

// Factory for creation of WebControllerProvider instances.
class WebControllerProviderFactory {
 public:
  WebControllerProviderFactory();
  virtual ~WebControllerProviderFactory();

  // Vends WebControllerProviders created using |browser_state|, passing
  // ownership to callers.
  virtual std::unique_ptr<WebControllerProvider> CreateWebControllerProvider(
      web::BrowserState* browser_state);
};

// Setter and getter for the provider factory. The provider factory should be
// set early, before any component using WebControllerProviders is called.
void SetWebControllerProviderFactory(
    WebControllerProviderFactory* provider_factory);
WebControllerProviderFactory* GetWebControllerProviderFactory();

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_WEB_WEB_CONTROLLER_PROVIDER_FACTORY_H_
