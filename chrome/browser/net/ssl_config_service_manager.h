// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_
#define CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_
#pragma once

namespace net {
class SSLConfigService;
}  // namespace net

class Profile;

// An interface for creating SSLConfigService objects for the current platform.
class SSLConfigServiceManager {
 public:
  // Create an instance of the default SSLConfigServiceManager for the current
  // platform. The lifetime of the profile must be longer than that of the
  // manager.
  static SSLConfigServiceManager* CreateDefaultManager(Profile* profile);

  virtual ~SSLConfigServiceManager() {}

  // Get an SSLConfigService instance.  It may be a new instance or the manager
  // may return the same instance multiple times.
  // The caller should hold a reference as long as it needs the instance (eg,
  // using scoped_refptr.)
  virtual net::SSLConfigService* Get() = 0;
};

#endif  // CHROME_BROWSER_NET_SSL_CONFIG_SERVICE_MANAGER_H_
