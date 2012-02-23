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
#include "base/memory/singleton.h"
#include "base/values.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

class Extension;
class ExtensionPermissionSet;

namespace extensions {

// C++ Wrapper for the JSON API definitions in chrome/common/extensions/api/.
class ExtensionAPI {
 public:
  // Filtering option for the GetSchemas functions.
  enum GetSchemasFilter {
    // Returns all schemas that an extension has permission for.
    ALL,

    // Returns schemas for only APIs with unprivileged components (i.e. those
    // where !IsWholeAPIPrivileged).
    ONLY_UNPRIVILEGED
  };

  typedef std::map<std::string, linked_ptr<const DictionaryValue> > SchemaMap;

  // Returns the single instance of this class.
  static ExtensionAPI* GetInstance();

  // Returns true if |name| is a privileged API path. Privileged paths can only
  // be called from extension code which is running in its own designated
  // extension process. They cannot be called from extension code running in
  // content scripts, or other low-privileged contexts.
  bool IsPrivileged(const std::string& name) const;

  // Returns whether *every* path in the API is privileged. This will be false
  // for APIs such as "storage" which is entirely unprivileged, and "test"
  // which has unprivileged components.
  bool IsWholeAPIPrivileged(const std::string& api_name) const;

  // Gets a map of API name (aka namespace) to API schema.
  const SchemaMap& schemas() { return schemas_; }

  // Gets the schema for the extension API with namespace |api_name|.
  // Ownership remains with this object.
  const base::DictionaryValue* GetSchema(const std::string& api_name) const;

  // Gets the API schemas that are available to an Extension.
  void GetSchemasForExtension(const Extension& extension,
                              GetSchemasFilter filter,
                              SchemaMap* out) const;

  // Gets the schemas for the default set of APIs that are available to every
  // extension.
  void GetDefaultSchemas(GetSchemasFilter filter, SchemaMap* out) const;

 private:
  friend struct DefaultSingletonTraits<ExtensionAPI>;

  ExtensionAPI();
  ~ExtensionAPI();

  // Loads a schema from a resource.
  void LoadSchemaFromResource(int resource_id);

  // Find an item in |list| with the specified property name and value, or NULL
  // if no such item exists.
  base::DictionaryValue* FindListItem(const base::ListValue* list,
                                      const std::string& property_name,
                                      const std::string& property_value) const;

  // Returns true if the function or event under |namespace_node| with
  // the specified |child_name| is privileged, or false otherwise. If the name
  // is not found, defaults to privileged.
  bool IsChildNamePrivileged(const base::DictionaryValue* namespace_node,
                             const std::string& child_kind,
                             const std::string& child_name) const;

  // Gets the schemas for the APIs that are allowed by a permission set.
  void GetSchemasForPermissions(const ExtensionPermissionSet& permissions,
                                GetSchemasFilter filter,
                                SchemaMap* out) const;

  // Adds dependent schemas to |out| as determined by the "dependencies"
  // property.
  void ResolveDependencies(SchemaMap* out) const;

  static ExtensionAPI* instance_;

  // Schemas for each namespace.
  SchemaMap schemas_;

  // APIs that are entirely unprivileged.
  std::set<std::string> completely_unprivileged_apis_;

  // APIs that are not entirely unprivileged, but have unprivileged components.
  std::set<std::string> partially_unprivileged_apis_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAPI);
};

}  // extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
