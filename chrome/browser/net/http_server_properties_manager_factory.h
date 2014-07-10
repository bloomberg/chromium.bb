// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_FACTORY_H_
#define CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_FACTORY_H_

#include "base/macros.h"

class PrefService;

namespace net {
class HttpServerPropertiesManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chrome_browser_net {

////////////////////////////////////////////////////////////////////////////////
// Class for registration and creation of HttpServerPropertiesManager
class HttpServerPropertiesManagerFactory {
 public:
  // Create an instance of HttpServerPropertiesManager.
  static net::HttpServerPropertiesManager* CreateManager(
      PrefService* pref_service);

  // Register prefs for properties managed by HttpServerPropertiesManager.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HttpServerPropertiesManagerFactory);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_FACTORY_H_
