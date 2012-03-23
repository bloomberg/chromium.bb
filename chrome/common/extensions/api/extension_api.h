// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/common/extensions/feature.h"
#include "chrome/common/extensions/url_pattern_set.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

class GURL;
class Extension;
class ExtensionPermissionSet;

namespace extensions {

// C++ Wrapper for the JSON API definitions in chrome/common/extensions/api/.
class ExtensionAPI {
 public:
  // Returns the single instance of this class.
  static ExtensionAPI* GetInstance();

  // Public for construction from unit tests. Use GetInstance() normally.
  ExtensionAPI();
  ~ExtensionAPI();

  // Returns true if |name| is a privileged API path. Privileged paths can only
  // be called from extension code which is running in its own designated
  // extension process. They cannot be called from extension code running in
  // content scripts, or other low-privileged contexts.
  bool IsPrivileged(const std::string& name);

  // Gets the schema for the extension API with namespace |api_name|.
  // Ownership remains with this object.
  const base::DictionaryValue* GetSchema(const std::string& api_name);

  // Gets the APIs available to |context| given an |extension| and |url|. The
  // extension or URL may not be relevant to all contexts, and may be left
  // NULL/empty.
  scoped_ptr<std::set<std::string> > GetAPIsForContext(
      Feature::Context context, const Extension* extension, const GURL& url);

 private:
  friend struct DefaultSingletonTraits<ExtensionAPI>;

  // Loads a schema.
  void LoadSchema(const base::StringPiece& schema);

  // Find an item in |list| with the specified property name and value, or NULL
  // if no such item exists.
  base::DictionaryValue* FindListItem(const base::ListValue* list,
                                      const std::string& property_name,
                                      const std::string& property_value);

  // Returns true if the function or event under |namespace_node| with
  // the specified |child_name| is privileged, or false otherwise. If the name
  // is not found, defaults to privileged.
  bool IsChildNamePrivileged(const base::DictionaryValue* namespace_node,
                             const std::string& child_kind,
                             const std::string& child_name);

  // Adds all APIs to |out| that |extension| has any permission (required or
  // optional) to use.
  void GetAllowedAPIs(const Extension* extension, std::set<std::string>* out);

  // Adds dependent schemas to |out| as determined by the "dependencies"
  // property.
  void ResolveDependencies(std::set<std::string>* out);

  // Adds any APIs listed in "dependencies" found in the schema for |api_name|
  // but not in |excluding| to |out|.
  void GetMissingDependencies(
      const std::string& api_name,
      const std::set<std::string>& excluding,
      std::set<std::string>* out);

  // Removes all APIs from |apis| which are *entirely* privileged. This won't
  // include APIs such as "storage" which is entirely unprivileged, nor
  // "extension" which has unprivileged components.
  void RemovePrivilegedAPIs(std::set<std::string>* apis);

  // Adds an APIs that match |url| to |out|.
  void GetAPIsMatchingURL(const GURL& url, std::set<std::string>* out);

  // Loads all remaining resources from |unloaded_schemas_|.
  void LoadAllSchemas();

  static ExtensionAPI* instance_;

  // Map from each API that hasn't been loaded yet to the schema which defines
  // it. Note that there may be multiple APIs per schema.
  std::map<std::string, base::StringPiece> unloaded_schemas_;

  // Schemas for each namespace.
  typedef std::map<std::string, linked_ptr<const DictionaryValue> > SchemaMap;
  SchemaMap schemas_;

  // APIs that are entirely unprivileged.
  std::set<std::string> completely_unprivileged_apis_;

  // APIs that are not entirely unprivileged, but have unprivileged components.
  std::set<std::string> partially_unprivileged_apis_;

  // APIs that have URL matching permissions.
  std::map<std::string, URLPatternSet> url_matching_apis_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAPI);
};

}  // extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
