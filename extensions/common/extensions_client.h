// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_

#include <string>

namespace extensions {

class FeatureProvider;
class PermissionsProvider;

// Sets up global state for the extensions system. Should be Set() once in each
// process. This should be implemented by the client of the extensions system.
class ExtensionsClient {
 public:
  // Returns a PermissionsProvider to initialize the permissions system.
  virtual const PermissionsProvider& GetPermissionsProvider() const = 0;

  // Gets a feature provider for a specific feature type.
  virtual FeatureProvider* GetFeatureProviderByName(const std::string& name)
      const = 0;

  // Called at startup. Registers the handlers for parsing manifests.
  virtual void RegisterManifestHandlers() const = 0;

  // Return the extensions client.
  static ExtensionsClient* Get();

  // Initialize the extensions system with this extensions client.
  static void Set(ExtensionsClient* client);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_
