// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_permissions_api_helpers.h"

#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/extensions/url_pattern_set.h"

namespace extensions {
namespace permissions_api {

namespace {

const char kInvalidOrigin[] =
    "Invalid value for origin pattern *: *";
const char kUnknownPermissionError[] =
    "'*' is not a recognized permission.";

const char kApisKey[] = "permissions";
const char kOriginsKey[] = "origins";

}  // namespace

DictionaryValue* PackPermissionsToValue(const ExtensionPermissionSet* set) {
  DictionaryValue* value = new DictionaryValue();

  // Generate the list of API permissions.
  ListValue* apis = new ListValue();
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  for (ExtensionAPIPermissionSet::const_iterator i = set->apis().begin();
       i != set->apis().end(); ++i)
    apis->Append(Value::CreateStringValue(info->GetByID(*i)->name()));

  // Generate the list of origin permissions.
  URLPatternSet hosts = set->explicit_hosts();
  ListValue* origins = new ListValue();
  for (URLPatternSet::const_iterator i = hosts.begin(); i != hosts.end(); ++i)
    origins->Append(Value::CreateStringValue(i->GetAsString()));

  value->Set(kApisKey, apis);
  value->Set(kOriginsKey, origins);
  return value;
}

// Creates a new ExtensionPermissionSet from its |value| and passes ownership to
// the caller through |ptr|. Sets |bad_message| to true if the message is badly
// formed. Returns false if the method fails to unpack a permission set.
bool UnpackPermissionsFromValue(DictionaryValue* value,
                                scoped_refptr<ExtensionPermissionSet>* ptr,
                                bool* bad_message,
                                std::string* error) {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet apis;
  if (value->HasKey(kApisKey)) {
    ListValue* api_list = NULL;
    if (!value->GetList(kApisKey, &api_list)) {
      *bad_message = true;
      return false;
    }
    for (size_t i = 0; i < api_list->GetSize(); ++i) {
      std::string api_name;
      if (!api_list->GetString(i, &api_name)) {
        *bad_message = true;
        return false;
      }

      ExtensionAPIPermission* permission = info->GetByName(api_name);
      if (!permission) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kUnknownPermissionError, api_name);
        return false;
      }
      apis.insert(permission->id());
    }
  }

  URLPatternSet origins;
  if (value->HasKey(kOriginsKey)) {
    ListValue* origin_list = NULL;
    if (!value->GetList(kOriginsKey, &origin_list)) {
      *bad_message = true;
      return false;
    }
    for (size_t i = 0; i < origin_list->GetSize(); ++i) {
      std::string pattern;
      if (!origin_list->GetString(i, &pattern)) {
        *bad_message = true;
        return false;
      }

      URLPattern origin(Extension::kValidHostPermissionSchemes);
      URLPattern::ParseResult parse_result = origin.Parse(pattern);
      if (URLPattern::PARSE_SUCCESS != parse_result) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kInvalidOrigin,
            pattern,
            URLPattern::GetParseResultString(parse_result));
        return false;
      }
      origins.AddPattern(origin);
    }
  }

  *ptr = new ExtensionPermissionSet(apis, origins, URLPatternSet());
  return true;
}

}  // namespace permissions_api
}  // namespace extensions
