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
  virtual void Initialize() override;
  virtual const PermissionMessageProvider& GetPermissionMessageProvider() const
      override;
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

  // A whitelist of extensions that can script anywhere. Do not add to this
  // list (except in tests) without consulting the Extensions team first.
  // Note: Component extensions have this right implicitly and do not need to be
  // added to this list.
  ScriptingWhitelist scripting_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionsClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_EXTENSIONS_CLIENT_H_
