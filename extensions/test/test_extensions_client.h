// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_TEST_TEST_EXTENSIONS_CLIENT_H_

#include "base/macros.h"
#include "extensions/common/extensions_client.h"

namespace extensions {

class TestExtensionsClient : public ExtensionsClient {
 public:
  TestExtensionsClient();
  virtual ~TestExtensionsClient();

 private:
  virtual void Initialize() OVERRIDE;
  virtual const PermissionMessageProvider& GetPermissionMessageProvider() const
      OVERRIDE;
  virtual const std::string GetProductName() OVERRIDE;
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
  virtual std::string GetWebstoreBaseURL() const OVERRIDE;
  virtual std::string GetWebstoreUpdateURL() const OVERRIDE;
  virtual bool IsBlacklistUpdateURL(const GURL& url) const OVERRIDE;

  // A whitelist of extensions that can script anywhere. Do not add to this
  // list (except in tests) without consulting the Extensions team first.
  // Note: Component extensions have this right implicitly and do not need to be
  // added to this list.
  ScriptingWhitelist scripting_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionsClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_EXTENSIONS_CLIENT_H_
