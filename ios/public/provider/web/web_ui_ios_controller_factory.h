// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_WEB_UI_IOS_CONTROLLER_FACTORY_H_
#define IOS_PUBLIC_PROVIDER_WEB_WEB_UI_IOS_CONTROLLER_FACTORY_H_

#include "ios/public/provider/web/web_ui_ios.h"

class GURL;

namespace web {

class WebUIIOSController;

// Interface for an object that controls which URLs are considered WebUIIOS
// URLs and creates WebUIIOSController instances for given URLs.
class WebUIIOSControllerFactory {
 public:
  virtual ~WebUIIOSControllerFactory() {}

  // Call to register a factory.
  static void RegisterFactory(WebUIIOSControllerFactory* factory);

  // Returns a WebUIIOSController instance for the given URL, or NULL if the URL
  // doesn't correspond to a WebUIIOS.
  virtual WebUIIOSController* CreateWebUIIOSControllerForURL(
      WebUIIOS* web_ui,
      const GURL& url) const = 0;
};

}  // namespace web

#endif  // IOS_PUBLIC_PROVIDER_WEB_WEB_UI_IOS_CONTROLLER_FACTORY_H_
