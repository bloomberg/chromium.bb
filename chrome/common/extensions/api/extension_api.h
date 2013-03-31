// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/features/feature_provider.h"
#include "extensions/common/url_pattern_set.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

class GURL;

namespace extensions {

class Extension;
class Feature;

// C++ Wrapper for the JSON API definitions in chrome/common/extensions/api/.
//
// WARNING: This class is accessed on multiple threads in the browser process
// (see ExtensionFunctionDispatcher). No state should be modified after
// construction.
class ExtensionAPI {
 public:
  // Returns a single shared instance of this class. This is the typical use
  // case in Chrome.
  //
  // TODO(aa): Make this const to enforce thread-safe usage.
  static ExtensionAPI* GetSharedInstance();

  // Creates a new instance configured the way ExtensionAPI typically is in
  // Chrome. Use the default constructor to get a clean instance.
  static ExtensionAPI* CreateWithDefaultConfiguration();

  // Splits a name like "permission:bookmark" into ("permission", "bookmark").
  // The first part refers to a type of feature, for example "manifest",
  // "permission", or "api". The second part is the full name of the feature.
  static void SplitDependencyName(const std::string& full_name,
                                  std::string* feature_type,
                                  std::string* feature_name);

  // Creates a completely clean instance. Configure using RegisterSchema() and
  // RegisterDependencyProvider before use.
  ExtensionAPI();
  virtual ~ExtensionAPI();

  void RegisterSchema(const std::string& api_name,
                      const base::StringPiece& source);

  void RegisterDependencyProvider(const std::string& name,
                                  FeatureProvider* provider);

  // Returns true if the specified API is available. |api_full_name| can be
  // either a namespace name (like "bookmarks") or a member name (like
  // "bookmarks.create"). Returns true if the feature and all of its
  // dependencies are available to the specified context.
  Feature::Availability IsAvailable(const std::string& api_full_name,
                                    const Extension* extension,
                                    Feature::Context context,
                                    const GURL& url);

  // Returns true if |name| is a privileged API path. Privileged paths can only
  // be called from extension code which is running in its own designated
  // extension process. They cannot be called from extension code running in
  // content scripts, or other low-privileged contexts.
  bool IsPrivileged(const std::string& name);

  // Gets the schema for the extension API with namespace |full_name|.
  // Ownership remains with this object.
  const base::DictionaryValue* GetSchema(const std::string& full_name);

  std::set<std::string> GetAllAPINames();

  // Splits a full name from the extension API into its API and child name
  // parts. Some examples:
  //
  // "bookmarks.create" -> ("bookmarks", "create")
  // "experimental.input.ui.cursorUp" -> ("experimental.input.ui", "cursorUp")
  // "storage.sync.set" -> ("storage", "sync.get")
  // "<unknown-api>.monkey" -> ("", "")
  //
  // The |child_name| parameter can be be NULL if you don't need that part.
  std::string GetAPINameFromFullName(const std::string& full_name,
                                     std::string* child_name);

  void InitDefaultConfiguration();

  // Gets a feature from any dependency provider registered with ExtensionAPI.
  // Returns NULL if the feature could not be found.
  Feature* GetFeatureDependency(const std::string& dependency_name);

 private:
  friend struct DefaultSingletonTraits<ExtensionAPI>;

  // Loads a schema.
  void LoadSchema(const std::string& name, const base::StringPiece& schema);

  // Returns true if the function or event under |namespace_node| with
  // the specified |child_name| is privileged, or false otherwise. If the name
  // is not found, defaults to privileged.
  bool IsChildNamePrivileged(const base::DictionaryValue* namespace_node,
                             const std::string& child_name);

  // NOTE: This IsAPIAllowed() and IsNonFeatureAPIAvailable only work for
  // non-feature-controlled APIs.
  // TODO(aa): Remove these when all APIs are converted to the feature system.

  // Checks if API |name| is allowed.
  bool IsAPIAllowed(const std::string& name, const Extension* extension);

  // Check if an API is available to |context| given an |extension| and |url|.
  // The extension or URL may not be relevant to all contexts, and may be left
  // NULL/empty.
  bool IsNonFeatureAPIAvailable(const std::string& name,
                                Feature::Context context,
                                const Extension* extension,
                                const GURL& url);

  // Checks if an API is *entirely* privileged. This won't include APIs such as
  // "storage" which is entirely unprivileged, nor "extension" which has
  // unprivileged components.
  bool IsPrivilegedAPI(const std::string& name);

  // Map from each API that hasn't been loaded yet to the schema which defines
  // it. Note that there may be multiple APIs per schema.
  typedef std::map<std::string, base::StringPiece> UnloadedSchemaMap;
  UnloadedSchemaMap unloaded_schemas_;

  // Schemas for each namespace.
  typedef std::map<std::string, linked_ptr<const DictionaryValue> > SchemaMap;
  SchemaMap schemas_;

  // APIs that are entirely unprivileged.
  std::set<std::string> completely_unprivileged_apis_;

  // APIs that are not entirely unprivileged, but have unprivileged components.
  std::set<std::string> partially_unprivileged_apis_;

  // APIs that have URL matching permissions.
  std::map<std::string, URLPatternSet> url_matching_apis_;

  typedef std::map<std::string, linked_ptr<Feature> > FeatureMap;
  typedef std::map<std::string, linked_ptr<FeatureMap> > APIFeatureMap;
  APIFeatureMap features_;

  // FeatureProviders used for resolving dependencies.
  typedef std::map<std::string, FeatureProvider*> FeatureProviderMap;
  FeatureProviderMap dependency_providers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAPI);
};

}  // extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
