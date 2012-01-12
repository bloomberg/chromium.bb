// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_api.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "grit/common_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

// static
ExtensionAPI* ExtensionAPI::GetInstance() {
  return Singleton<ExtensionAPI>::get();
}

static base::ListValue* LoadSchemaList(int resource_id) {
  const bool kAllowTrailingCommas = false;
  std::string error_message;
  Value* result =
      base::JSONReader::ReadAndReturnError(
          ResourceBundle::GetSharedInstance().GetRawDataResource(
              resource_id).as_string(),
          kAllowTrailingCommas,
          NULL,  // error code
          &error_message);
  CHECK(result) << error_message;
  CHECK(result->IsType(base::Value::TYPE_LIST));
  return static_cast<base::ListValue*>(result);
}

void ExtensionAPI::LoadSchemaFromResource(int resource_id) {
  scoped_ptr<base::ListValue> loaded(LoadSchemaList(resource_id));
  Value* value = NULL;
  std::string schema_namespace;
  while (!loaded->empty()) {
    loaded->Remove(loaded->GetSize() - 1, &value);
    CHECK(value->IsType(Value::TYPE_DICTIONARY));
    const DictionaryValue* schema = static_cast<const DictionaryValue*>(value);
    CHECK(schema->GetString("namespace", &schema_namespace));
    schemas_[schema_namespace] = linked_ptr<const DictionaryValue>(schema);
  }
}

ExtensionAPI::ExtensionAPI() {
  static int kJsonApiResourceIds[] = {
    IDR_EXTENSION_API_JSON_BOOKMARKS,
    IDR_EXTENSION_API_JSON_BROWSERACTION,
    IDR_EXTENSION_API_JSON_CHROMEAUTHPRIVATE,
    IDR_EXTENSION_API_JSON_CHROMEOSINFOPRIVATE,
    IDR_EXTENSION_API_JSON_CHROMEPRIVATE,
    IDR_EXTENSION_API_JSON_CONTENTSETTINGS,
    IDR_EXTENSION_API_JSON_CONTEXTMENUS,
    IDR_EXTENSION_API_JSON_COOKIES,
    IDR_EXTENSION_API_JSON_DEBUGGER,
    IDR_EXTENSION_API_JSON_DEVTOOLS,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_ACCESSIBILITY,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_APP,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_BOOKMARKMANAGER,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_CLEAR,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_DNS,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_DOWNLOADS,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_INFOBARS,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_INPUT_IME,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_INPUT_UI,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_INPUT_VIRTUALKEYBOARD,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_PRIVACY,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_PROCESSES,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_RLZ,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_SOCKET,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_SPEECHINPUT,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_STORAGE,
    IDR_EXTENSION_API_JSON_EXPERIMENTAL_TOPSITES,
    IDR_EXTENSION_API_JSON_EXTENSION,
    IDR_EXTENSION_API_JSON_FILEBROWSERHANDLER,
    IDR_EXTENSION_API_JSON_FILEBROWSERPRIVATE,
    IDR_EXTENSION_API_JSON_HISTORY,
    IDR_EXTENSION_API_JSON_I18N,
    IDR_EXTENSION_API_JSON_IDLE,
    IDR_EXTENSION_API_JSON_INPUTMETHODPRIVATE,
    IDR_EXTENSION_API_JSON_MANAGEMENT,
    IDR_EXTENSION_API_JSON_MEDIAPLAYERPRIVATE,
    IDR_EXTENSION_API_JSON_METRICSPRIVATE,
    IDR_EXTENSION_API_JSON_OMNIBOX,
    IDR_EXTENSION_API_JSON_PAGEACTION,
    IDR_EXTENSION_API_JSON_PAGEACTIONS,
    IDR_EXTENSION_API_JSON_PAGECAPTURE,
    IDR_EXTENSION_API_JSON_PERMISSIONS,
    IDR_EXTENSION_API_JSON_PROXY,
    IDR_EXTENSION_API_JSON_SYSTEMPRIVATE,
    IDR_EXTENSION_API_JSON_TABS,
    IDR_EXTENSION_API_JSON_TERMINALPRIVATE,
    IDR_EXTENSION_API_JSON_TEST,
    IDR_EXTENSION_API_JSON_TTS,
    IDR_EXTENSION_API_JSON_TTSENGINE,
    IDR_EXTENSION_API_JSON_TYPES,
    IDR_EXTENSION_API_JSON_WEBNAVIGATION,
    IDR_EXTENSION_API_JSON_WEBREQUEST,
    IDR_EXTENSION_API_JSON_WEBSOCKETPROXYPRIVATE,
    IDR_EXTENSION_API_JSON_WEBSTOREPRIVATE,
    IDR_EXTENSION_API_JSON_WINDOWS,
  };

  for (size_t i = 0; i < arraysize(kJsonApiResourceIds); i++) {
    LoadSchemaFromResource(kJsonApiResourceIds[i]);
  }
}

ExtensionAPI::~ExtensionAPI() {
}

bool ExtensionAPI::IsPrivileged(const std::string& full_name) const {
  std::vector<std::string> split_full_name;
  base::SplitString(full_name, '.', &split_full_name);
  std::string name = split_full_name.back();
  split_full_name.pop_back();
  std::string name_space = JoinString(split_full_name, '.');

  // HACK(kalman): explicitly mark all Storage API methods as unprivileged.
  // TODO(kalman): solve this in a more general way; the problem is that
  // functions-on-properties are not found with the following algorithm.
  if (name_space == "experimental.storage")
    return false;

  const base::DictionaryValue* name_space_node = GetSchema(name_space);
  if (!name_space_node)
    return true;

  if (!IsChildNamePrivileged(name_space_node, "functions", name) ||
      !IsChildNamePrivileged(name_space_node, "events", name)) {
    return false;
  }

  return true;
}

DictionaryValue* ExtensionAPI::FindListItem(
    const base::ListValue* list,
    const std::string& property_name,
    const std::string& property_value) const {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    DictionaryValue* item = NULL;
    CHECK(list->GetDictionary(i, &item))
        << property_value << "/" << property_name;
    std::string value;
    if (item->GetString(property_name, &value) && value == property_value)
      return item;
  }

  return NULL;
}

bool ExtensionAPI::IsChildNamePrivileged(const DictionaryValue* name_space_node,
                                         const std::string& child_kind,
                                         const std::string& child_name) const {
  ListValue* child_list = NULL;
  name_space_node->GetList(child_kind, &child_list);
  if (!child_list)
    return true;

  bool unprivileged = false;
  DictionaryValue* child = FindListItem(child_list, "name", child_name);
  if (!child || !child->GetBoolean("unprivileged", &unprivileged))
    return true;

  return !unprivileged;
}

const base::DictionaryValue* ExtensionAPI::GetSchema(
    const std::string& api_name) const {
  SchemaMap::const_iterator maybe_schema = schemas_.find(api_name);
  return maybe_schema != schemas_.end() ? maybe_schema->second.get() : NULL;
}

void ExtensionAPI::GetSchemasForExtension(
    const Extension& extension, SchemaMap* out) const {
  // Check both required_permissions and optional_permissions since we need
  // to return all schemas that might be needed.
  GetSchemasForPermissions(*extension.required_permission_set(), out);
  GetSchemasForPermissions(*extension.optional_permission_set(), out);
}

void ExtensionAPI::GetDefaultSchemas(SchemaMap* out) const {
  scoped_refptr<ExtensionPermissionSet> default_permissions(
      new ExtensionPermissionSet());
  GetSchemasForPermissions(*default_permissions, out);
}

void ExtensionAPI::GetSchemasForPermissions(
    const ExtensionPermissionSet& permissions, SchemaMap* out) const {
  for (SchemaMap::const_iterator it = schemas_.begin(); it != schemas_.end();
      ++it) {
    if (permissions.HasAnyAccessToAPI(it->first))
      (*out)[it->first] = it->second;
  }
}

}
