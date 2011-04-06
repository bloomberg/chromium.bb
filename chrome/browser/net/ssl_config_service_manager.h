// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_
#define CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_
#pragma once

namespace net {
class SSLConfigService;
}  // namespace net

class PrefService;

// An interface for creating SSLConfigService objects for the current platform.
class SSLConfigServiceManager {
 public:
  // Create an instance of the default SSLConfigServiceManager for the current
  // platform. The lifetime of the PrefService objects must be longer than that
  // of the manager. Get SSL preferences from local_state object. If SSL
  // preferences don't exist in local_state object, then get the data from
  // user_prefs object and migrate it to local_state object and then delete the
  // data from user_prefs object.
  static SSLConfigServiceManager* CreateDefaultManager(
      PrefService* user_prefs,
      PrefService* local_state);

  virtual ~SSLConfigServiceManager() {}

  // Get an SSLConfigService instance.  It may be a new instance or the manager
  // may return the same instance multiple times.
  // The caller should hold a reference as long as it needs the instance (eg,
  // using scoped_refptr.)
  virtual net::SSLConfigService* Get() = 0;
};

#endif  // CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_
