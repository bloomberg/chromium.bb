// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/webview_info.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/pattern.h"
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

// A PartitionItem represents a set of accessible resources given a partition
// ID pattern.
class PartitionItem {
 public:
  explicit PartitionItem(const std::string& partition_pattern)
      : partition_pattern_(partition_pattern) {
  }

  virtual ~PartitionItem() {
  }

  bool Matches(const std::string& partition_id) const {
    return base::MatchPattern(partition_id, partition_pattern_);
  }

  // Adds a pattern to the set. Returns true if a new pattern was inserted,
  // false if the pattern was already in the set.
  bool AddPattern(const URLPattern& pattern) {
    return accessible_resources_.AddPattern(pattern);
  }

  const URLPatternSet& accessible_resources() const {
    return accessible_resources_;
  }
 private:
  // A pattern string that matches partition IDs.
  const std::string partition_pattern_;
  // A URL pattern set of resources accessible to the given
  // |partition_pattern_|.
  URLPatternSet accessible_resources_;
};

WebviewInfo::WebviewInfo(const std::string& extension_id)
    : extension_id_(extension_id) {
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

  const WebviewInfo* webview_info = static_cast<const WebviewInfo*>(
      extension->GetManifestData(keys::kWebviewAccessibleResources));
  if (!webview_info)
    return false;

  for (const auto& item : webview_info->partition_items_) {
    if (item->Matches(partition_id) &&
        extension->ResourceMatches(item->accessible_resources(),
                                   relative_path)) {
      return true;
    }
  }

  return false;
}

// static
bool WebviewInfo::HasWebviewAccessibleResources(
    const Extension& extension,
    const std::string& partition_id) {
  const WebviewInfo* webview_info = static_cast<const WebviewInfo*>(
      extension.GetManifestData(keys::kWebviewAccessibleResources));
  if (!webview_info)
    return false;

  for (const auto& item : webview_info->partition_items_) {
    if (item->Matches(partition_id))
      return true;
  }
  return false;
}

void WebviewInfo::AddPartitionItem(std::unique_ptr<PartitionItem> item) {
  partition_items_.push_back(std::move(item));
}

WebviewHandler::WebviewHandler() {
}

WebviewHandler::~WebviewHandler() {
}

bool WebviewHandler::Parse(Extension* extension, base::string16* error) {
  std::unique_ptr<WebviewInfo> info(new WebviewInfo(extension->id()));

  const base::DictionaryValue* dict_value = NULL;
  if (!extension->manifest()->GetDictionary(keys::kWebview,
                                            &dict_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebview);
    return false;
  }

  const base::ListValue* partition_list = NULL;
  if (!dict_value->GetList(keys::kWebviewPartitions, &partition_list)) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebviewPartitionsList);
    return false;
  }

  // The partition list must have at least one entry.
  if (partition_list->GetSize() == 0) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebviewPartitionsList);
    return false;
  }

  for (size_t i = 0; i < partition_list->GetSize(); ++i) {
    const base::DictionaryValue* partition = NULL;
    if (!partition_list->GetDictionary(i, &partition)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebviewPartition, base::NumberToString(i));
      return false;
    }

    std::string partition_pattern;
    if (!partition->GetString(keys::kWebviewName, &partition_pattern)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebviewPartitionName, base::NumberToString(i));
      return false;
    }

    const base::ListValue* url_list = NULL;
    if (!partition->GetList(keys::kWebviewAccessibleResources,
                            &url_list)) {
      *error = base::ASCIIToUTF16(
          errors::kInvalidWebviewAccessibleResourcesList);
      return false;
    }

    // The URL list should have at least one entry.
    if (url_list->GetSize() == 0) {
      *error = base::ASCIIToUTF16(
          errors::kInvalidWebviewAccessibleResourcesList);
      return false;
    }

    std::unique_ptr<PartitionItem> partition_item(
        new PartitionItem(partition_pattern));

    for (size_t i = 0; i < url_list->GetSize(); ++i) {
      std::string relative_path;
      if (!url_list->GetString(i, &relative_path)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidWebviewAccessibleResource, base::NumberToString(i));
        return false;
      }
      URLPattern pattern(URLPattern::SCHEME_EXTENSION,
                         Extension::GetResourceURL(extension->url(),
                                                   relative_path).spec());
      partition_item->AddPattern(pattern);
    }
    info->AddPartitionItem(std::move(partition_item));
  }

  extension->SetManifestData(keys::kWebviewAccessibleResources,
                             std::move(info));
  return true;
}

const std::vector<std::string> WebviewHandler::Keys() const {
  return SingleKey(keys::kWebview);
}

}  // namespace extensions
