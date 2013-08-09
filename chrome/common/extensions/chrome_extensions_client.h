// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_
#define CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/permissions_provider.h"

namespace extensions {

// The implementation of ExtensionsClient for Chrome, which encapsulates the
// global knowledge of features, permissions, and manifest fields.
class ChromeExtensionsClient : public ExtensionsClient {
 public:
  ChromeExtensionsClient();
  virtual ~ChromeExtensionsClient();

  virtual const PermissionsProvider& GetPermissionsProvider() const OVERRIDE;
  virtual void RegisterManifestHandlers() const OVERRIDE;
  virtual FeatureProvider* GetFeatureProviderByName(const std::string& name)
      const OVERRIDE;

  // Get the LazyInstance for ChromeExtensionsClient.
  static ChromeExtensionsClient* GetInstance();

 private:
  const ChromeAPIPermissions chrome_api_permissions_;

  friend struct base::DefaultLazyInstanceTraits<ChromeExtensionsClient>;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsClient);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_
