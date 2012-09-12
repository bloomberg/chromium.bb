// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_
#define CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_

namespace net {
class SSLConfigService;
}  // namespace net

class PrefService;

// An interface for creating SSLConfigService objects.
class SSLConfigServiceManager {
 public:
  // Create an instance of the SSLConfigServiceManager. The lifetime of the
  // PrefService objects must be longer than that of the manager. Get SSL
  // preferences from local_state object.  The user_prefs may be NULL if this
  // SSLConfigServiceManager is not associated with a profile.
  static SSLConfigServiceManager* CreateDefaultManager(
      PrefService* local_state,
      PrefService* user_prefs);

  static void RegisterPrefs(PrefService* local_state);

  virtual ~SSLConfigServiceManager() {}

  // Get an SSLConfigService instance.  It may be a new instance or the manager
  // may return the same instance multiple times.
  // The caller should hold a reference as long as it needs the instance (eg,
  // using scoped_refptr.)
  virtual net::SSLConfigService* Get() = 0;
};

#endif  // CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_
