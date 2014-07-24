// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/extensions_api_permissions.h"

namespace extensions {

// The app_shell implementation of ExtensionsClient.
class ShellExtensionsClient : public ExtensionsClient {
 public:
  ShellExtensionsClient();
  virtual ~ShellExtensionsClient();

  // ExtensionsClient overrides:
  virtual void Initialize() OVERRIDE;
  virtual const PermissionMessageProvider& GetPermissionMessageProvider()
      const OVERRIDE;
  virtual scoped_ptr<FeatureProvider> CreateFeatureProvider(
      const std::string& name) const OVERRIDE;
  virtual scoped_ptr<JSONFeatureProviderSource> CreateFeatureProviderSource(
      const std::string& name) const OVERRIDE;
  virtual void FilterHostPermissions(
      const URLPatternSet& hosts,
      URLPatternSet* new_hosts,
      std::set<PermissionMessage>* messages) const OVERRIDE;
  virtual void SetScriptingWhitelist(
      const ScriptingWhitelist& whitelist) OVERRIDE;
  virtual const ScriptingWhitelist& GetScriptingWhitelist() const OVERRIDE;
  virtual URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const OVERRIDE;
  virtual bool IsScriptableURL(const GURL& url,
                               std::string* error) const OVERRIDE;
  virtual bool IsAPISchemaGenerated(const std::string& name) const OVERRIDE;
  virtual base::StringPiece GetAPISchema(
      const std::string& name) const OVERRIDE;
  virtual void RegisterAPISchemaResources(ExtensionAPI* api) const OVERRIDE;
  virtual bool ShouldSuppressFatalErrors() const OVERRIDE;

 private:
  const ExtensionsAPIPermissions extensions_api_permissions_;

  ScriptingWhitelist scripting_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_CLIENT_H_
