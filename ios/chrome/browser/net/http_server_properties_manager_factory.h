// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_FACTORY_H_

#include "base/macros.h"

class PrefService;

namespace net {
class HttpServerPropertiesManager;
class NetLog;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Class for registration and creation of HttpServerPropertiesManager
class HttpServerPropertiesManagerFactory {
 public:
  // Create an instance of HttpServerPropertiesManager.
  static net::HttpServerPropertiesManager* CreateManager(
      PrefService* pref_service,
      net::NetLog* net_log);

  // Register prefs for properties managed by HttpServerPropertiesManager.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManagerFactory);
};

#endif  // IOS_CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_FACTORY_H_
