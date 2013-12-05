// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/webview_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = extensions::manifest_keys;
namespace errors = extensions::manifest_errors;

namespace {

const WebviewInfo* GetResourcesInfo(
    const Extension& extension) {
  return static_cast<WebviewInfo*>(
      extension.GetManifestData(keys::kWebviewAccessibleResources));
}

}  // namespace

WebviewInfo::WebviewInfo() {
}

WebviewInfo::~WebviewInfo() {
}

// static
bool WebviewInfo::IsResourceWebviewAccessible(
    const Extension* extension,
    const std::string& partition_id,
    const std::string& relative_path) {
  if (!extension)
    return false;

  const WebviewInfo* info = GetResourcesInfo(*extension);
  if (!info)
    return false;

  bool partition_is_privileged = false;
  for (size_t i = 0;
       i < info->webview_privileged_partitions_.size();
       ++i) {
    if (MatchPattern(partition_id, info->webview_privileged_partitions_[i])) {
      partition_is_privileged = true;
      break;
    }
  }

  return partition_is_privileged && extension->ResourceMatches(
      info->webview_accessible_resources_, relative_path);
}

WebviewHandler::WebviewHandler() {
}

WebviewHandler::~WebviewHandler() {
}

bool WebviewHandler::Parse(Extension* extension, base::string16* error) {
  scoped_ptr<WebviewInfo> info(new WebviewInfo());

  const base::DictionaryValue* dict_value = NULL;
  if (!extension->manifest()->GetDictionary(keys::kWebview,
                                            &dict_value)) {
    *error = ASCIIToUTF16(errors::kInvalidWebview);
    return false;
  }

  const base::ListValue* url_list = NULL;
  if (!dict_value->GetList(keys::kWebviewAccessibleResources,
                           &url_list)) {
    *error = ASCIIToUTF16(errors::kInvalidWebviewAccessibleResourcesList);
    return false;
  }

  for (size_t i = 0; i < url_list->GetSize(); ++i) {
    std::string relative_path;
    if (!url_list->GetString(i, &relative_path)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebviewAccessibleResource, base::IntToString(i));
      return false;
    }
    URLPattern pattern(URLPattern::SCHEME_EXTENSION);
    if (pattern.Parse(extension->url().spec()) != URLPattern::PARSE_SUCCESS) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidURLPatternError, extension->url().spec());
      return false;
    }
    while (relative_path[0] == '/')
      relative_path = relative_path.substr(1, relative_path.length() - 1);
    pattern.SetPath(pattern.path() + relative_path);
    info->webview_accessible_resources_.AddPattern(pattern);
  }

  const base::ListValue* partition_list = NULL;
  if (!dict_value->GetList(keys::kWebviewPrivilegedPartitions,
                           &partition_list)) {
    *error = ASCIIToUTF16(errors::kInvalidWebviewPrivilegedPartitionList);
    return false;
  }
  for (size_t i = 0; i < partition_list->GetSize(); ++i) {
    std::string partition_wildcard;
    if (!partition_list->GetString(i, &partition_wildcard)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebviewPrivilegedPartition, base::IntToString(i));
      return false;
    }
    info->webview_privileged_partitions_.push_back(partition_wildcard);
  }
  extension->SetManifestData(keys::kWebviewAccessibleResources, info.release());
  return true;
}

const std::vector<std::string> WebviewHandler::Keys() const {
  return SingleKey(keys::kWebview);
}

}  // namespace extensions
