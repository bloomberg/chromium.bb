// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"

class GURL;

namespace extensions {

class APIPermissionSet;
class Extension;
class ExtensionAPI;
class FeatureProvider;
class JSONFeatureProviderSource;
class ManifestPermissionSet;
class PermissionMessage;
class PermissionMessageProvider;
class SimpleFeature;
class URLPatternSet;

// Sets up global state for the extensions system. Should be Set() once in each
// process. This should be implemented by the client of the extensions system.
class ExtensionsClient {
 public:
  typedef std::vector<std::string> ScriptingWhitelist;

  virtual ~ExtensionsClient() {}

  // Initializes global state. Not done in the constructor because unit tests
  // can create additional ExtensionsClients because the utility thread runs
  // in-process.
  virtual void Initialize() = 0;

  // Returns the global PermissionMessageProvider to use to provide permission
  // warning strings.
  virtual const PermissionMessageProvider& GetPermissionMessageProvider()
      const = 0;

  // Returns the application name. For example, "Chromium" or "app_shell".
  virtual const std::string GetProductName() = 0;

  // Create a FeatureProvider for a specific feature type, e.g. "permission".
  virtual scoped_ptr<FeatureProvider> CreateFeatureProvider(
      const std::string& name) const = 0;

  // Create a JSONFeatureProviderSource for a specific feature type,
  // e.g. "permission". Currently, all features are loaded from
  // JSONFeatureProviderSources.
  // This is used primarily in CreateFeatureProvider, above.
  virtual scoped_ptr<JSONFeatureProviderSource> CreateFeatureProviderSource(
      const std::string& name) const = 0;

  // Takes the list of all hosts and filters out those with special
  // permission strings. Adds the regular hosts to |new_hosts|,
  // and adds the special permission messages to |messages|.
  virtual void FilterHostPermissions(
      const URLPatternSet& hosts,
      URLPatternSet* new_hosts,
      std::set<PermissionMessage>* messages) const = 0;

  // Replaces the scripting whitelist with |whitelist|. Used in the renderer;
  // only used for testing in the browser process.
  virtual void SetScriptingWhitelist(const ScriptingWhitelist& whitelist) = 0;

  // Return the whitelist of extensions that can run content scripts on
  // any origin.
  virtual const ScriptingWhitelist& GetScriptingWhitelist() const = 0;

  // Get the set of chrome:// hosts that |extension| can run content scripts on.
  virtual URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const = 0;

  // Returns false if content scripts are forbidden from running on |url|.
  virtual bool IsScriptableURL(const GURL& url, std::string* error) const = 0;

  // Returns true iff a schema named |name| is generated.
  virtual bool IsAPISchemaGenerated(const std::string& name) const = 0;

  // Gets the generated API schema named |name|.
  virtual base::StringPiece GetAPISchema(const std::string& name) const = 0;

  // Register non-generated API schema resources with the global ExtensionAPI.
  // Called when the ExtensionAPI is lazily initialized.
  virtual void RegisterAPISchemaResources(ExtensionAPI* api) const = 0;

  // Determines if certain fatal extensions errors should be surpressed
  // (i.e., only logged) or allowed (i.e., logged before crashing).
  virtual bool ShouldSuppressFatalErrors() const = 0;

  // Returns the base webstore URL prefix.
  virtual std::string GetWebstoreBaseURL() const = 0;

  // Returns the URL to use for update manifest queries.
  virtual std::string GetWebstoreUpdateURL() const = 0;

  // Returns a flag indicating whether or not a given URL is a valid
  // extension blacklist URL.
  virtual bool IsBlacklistUpdateURL(const GURL& url) const = 0;

  // Return the extensions client.
  static ExtensionsClient* Get();

  // Initialize the extensions system with this extensions client.
  static void Set(ExtensionsClient* client);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSIONS_CLIENT_H_
