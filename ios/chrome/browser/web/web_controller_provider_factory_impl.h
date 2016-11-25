// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_WEB_CONTROLLER_PROVIDER_FACTORY_IMPL_H_
#define IOS_CHROME_BROWSER_WEB_WEB_CONTROLLER_PROVIDER_FACTORY_IMPL_H_

#include <memory>

#import "ios/public/provider/web/web_controller_provider_factory.h"

// Concrete implementation of ios::WebControllerProviderFactory.
class WebControllerProviderFactoryImpl
    : public ios::WebControllerProviderFactory {
 public:
  WebControllerProviderFactoryImpl();
  ~WebControllerProviderFactoryImpl() override;

  // ios::WebControllerProviderFactory implementation.
  std::unique_ptr<ios::WebControllerProvider> CreateWebControllerProvider(
      web::BrowserState* browser_state) override;
};

#endif  // IOS_CHROME_BROWSER_WEB_WEB_CONTROLLER_PROVIDER_FACTORY_IMPL_H_
