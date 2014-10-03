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
  virtual void Initialize() override;
  virtual const PermissionMessageProvider& GetPermissionMessageProvider()
      const override;
  virtual const std::string GetProductName() override;
  virtual scoped_ptr<FeatureProvider> CreateFeatureProvider(
      const std::string& name) const override;
  virtual scoped_ptr<JSONFeatureProviderSource> CreateFeatureProviderSource(
      const std::string& name) const override;
  virtual void FilterHostPermissions(
      const URLPatternSet& hosts,
      URLPatternSet* new_hosts,
      std::set<PermissionMessage>* messages) const override;
  virtual void SetScriptingWhitelist(
      const ScriptingWhitelist& whitelist) override;
  virtual const ScriptingWhitelist& GetScriptingWhitelist() const override;
  virtual URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const override;
  virtual bool IsScriptableURL(const GURL& url,
                               std::string* error) const override;
  virtual bool IsAPISchemaGenerated(const std::string& name) const override;
  virtual base::StringPiece GetAPISchema(
      const std::string& name) const override;
  virtual void RegisterAPISchemaResources(ExtensionAPI* api) const override;
  virtual bool ShouldSuppressFatalErrors() const override;
  virtual std::string GetWebstoreBaseURL() const override;
  virtual std::string GetWebstoreUpdateURL() const override;
  virtual bool IsBlacklistUpdateURL(const GURL& url) const override;

 private:
  const ExtensionsAPIPermissions extensions_api_permissions_;

  ScriptingWhitelist scripting_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_COMMON_SHELL_EXTENSIONS_CLIENT_H_
