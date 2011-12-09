// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

static void AppendExtraSchemaList(base::ListValue* list, int resource_id) {
  Value* value;
  scoped_ptr<base::ListValue> to_append(LoadSchemaList(resource_id));
  while (to_append->Remove(0, &value)) {
    list->Append(value);
  }
}

ExtensionAPI::ExtensionAPI() {
  value_.reset(LoadSchemaList(IDR_EXTENSION_API_JSON));
  AppendExtraSchemaList(value_.get(), IDR_EXTENSION_API_JSON_EXTENSION);
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

  base::DictionaryValue* name_space_node =
      FindListItem(value_.get(), "namespace", name_space);
  if (!name_space_node)
    return true;

  if (!IsChildNamePrivileged(name_space_node, "functions", name) ||
      !IsChildNamePrivileged(name_space_node, "events", name)) {
    return false;
  }

  return true;
}

DictionaryValue* ExtensionAPI::FindListItem(
    base::ListValue* list,
    const std::string& property_name,
    const std::string& property_value) const {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    DictionaryValue* item = NULL;
    CHECK(list->GetDictionary(i, &item));
    std::string value;
    if (item->GetString(property_name, &value) && value == property_value)
      return item;
  }

  return NULL;
}

bool ExtensionAPI::IsChildNamePrivileged(DictionaryValue* name_space_node,
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

}
