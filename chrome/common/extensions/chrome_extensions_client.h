// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_
#define CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/extensions_api_permissions.h"

namespace extensions {

// The implementation of ExtensionsClient for Chrome, which encapsulates the
// global knowledge of features, permissions, and manifest fields.
class ChromeExtensionsClient : public ExtensionsClient {
 public:
  ChromeExtensionsClient();
  virtual ~ChromeExtensionsClient();

  virtual void Initialize() OVERRIDE;

  virtual const PermissionMessageProvider& GetPermissionMessageProvider() const
      OVERRIDE;
  virtual scoped_ptr<FeatureProvider> CreateFeatureProvider(
      const std::string& name) const OVERRIDE;
  virtual scoped_ptr<JSONFeatureProviderSource> CreateFeatureProviderSource(
      const std::string& name) const OVERRIDE;
  virtual void FilterHostPermissions(
      const URLPatternSet& hosts,
      URLPatternSet* new_hosts,
      std::set<PermissionMessage>* messages) const OVERRIDE;
  virtual void SetScriptingWhitelist(const ScriptingWhitelist& whitelist)
      OVERRIDE;
  virtual const ScriptingWhitelist& GetScriptingWhitelist() const OVERRIDE;
  virtual URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const OVERRIDE;
  virtual bool IsScriptableURL(const GURL& url, std::string* error) const
      OVERRIDE;
  virtual bool IsAPISchemaGenerated(const std::string& name) const OVERRIDE;
  virtual base::StringPiece GetAPISchema(const std::string& name) const
      OVERRIDE;
  virtual void RegisterAPISchemaResources(ExtensionAPI* api) const OVERRIDE;
  virtual bool ShouldSuppressFatalErrors() const OVERRIDE;

  // Get the LazyInstance for ChromeExtensionsClient.
  static ChromeExtensionsClient* GetInstance();

 private:
  const ChromeAPIPermissions chrome_api_permissions_;
  const ExtensionsAPIPermissions extensions_api_permissions_;
  const ChromePermissionMessageProvider permission_message_provider_;

  // A whitelist of extensions that can script anywhere. Do not add to this
  // list (except in tests) without consulting the Extensions team first.
  // Note: Component extensions have this right implicitly and do not need to be
  // added to this list.
  ScriptingWhitelist scripting_whitelist_;

  friend struct base::DefaultLazyInstanceTraits<ChromeExtensionsClient>;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsClient);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_
