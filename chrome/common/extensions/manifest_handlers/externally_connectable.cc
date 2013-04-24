// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/externally_connectable.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace externally_connectable_errors {
  const char kErrorInvalid[] = "Invalid value for 'externally_connectable'";
  const char kErrorInvalidMatchPattern[] = "Invalid match pattern '*'";
  const char kErrorInvalidId[] = "Invalid ID '*'";
}

namespace keys = extension_manifest_keys;
namespace errors = externally_connectable_errors;
using api::manifest_types::ExternallyConnectable;

namespace {
const char kAllIds[] = "*";
}

ExternallyConnectableHandler::ExternallyConnectableHandler() {}

ExternallyConnectableHandler::~ExternallyConnectableHandler() {}

bool ExternallyConnectableHandler::Parse(Extension* extension,
                                         string16* error) {
  const base::Value* externally_connectable = NULL;
  CHECK(extension->manifest()->Get(keys::kExternallyConnectable,
                                   &externally_connectable));
  scoped_ptr<ExternallyConnectableInfo> info =
      ExternallyConnectableInfo::FromValue(*externally_connectable, error);
  if (!info)
    return false;
  extension->SetManifestData(keys::kExternallyConnectable, info.release());
  return true;
}

const std::vector<std::string> ExternallyConnectableHandler::Keys() const {
  return SingleKey(keys::kExternallyConnectable);
}

// static
ExternallyConnectableInfo* ExternallyConnectableInfo::Get(
    const Extension* extension) {
  return static_cast<ExternallyConnectableInfo*>(
      extension->GetManifestData(keys::kExternallyConnectable));
}

// static
scoped_ptr<ExternallyConnectableInfo> ExternallyConnectableInfo::FromValue(
    const base::Value& value,
    string16* error) {
  scoped_ptr<ExternallyConnectable> externally_connectable =
      ExternallyConnectable::FromValue(value);
  if (!externally_connectable) {
    *error = UTF8ToUTF16(errors::kErrorInvalid);
    return scoped_ptr<ExternallyConnectableInfo>();
  }

  URLPatternSet matches;

  if (externally_connectable->matches) {
    for (std::vector<std::string>::iterator it =
             externally_connectable->matches->begin();
         it != externally_connectable->matches->end(); ++it) {
      // Safe to use SCHEME_ALL here; externally_connectable gives a page ->
      // extension communication path, not the other way.
      URLPattern pattern(URLPattern::SCHEME_ALL);
      if (pattern.Parse(*it) != URLPattern::PARSE_SUCCESS) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kErrorInvalidMatchPattern, *it);
        return scoped_ptr<ExternallyConnectableInfo>();
      }
      matches.AddPattern(pattern);
    }
  }

  std::vector<std::string> ids;
  bool matches_all_ids = false;

  if (externally_connectable->ids) {
    for (std::vector<std::string>::iterator it =
             externally_connectable->ids->begin();
         it != externally_connectable->ids->end(); ++it) {
      if (*it == kAllIds) {
        matches_all_ids = true;
      } else if (Extension::IdIsValid(*it)) {
        ids.push_back(*it);
      } else {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kErrorInvalidId, *it);
        return scoped_ptr<ExternallyConnectableInfo>();
      }
    }
  }

  return make_scoped_ptr(
      new ExternallyConnectableInfo(matches, ids, matches_all_ids));
}

ExternallyConnectableInfo::~ExternallyConnectableInfo() {}

ExternallyConnectableInfo::ExternallyConnectableInfo(
    const URLPatternSet& matches,
    const std::vector<std::string>& ids,
    bool matches_all_ids)
    : matches(matches), ids(ids), matches_all_ids(matches_all_ids) {}

}   // namespace extensions
