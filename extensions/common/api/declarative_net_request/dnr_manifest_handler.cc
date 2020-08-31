// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_handler.h"

#include <set>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "tools/json_schema_compiler/util.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {

namespace {

bool IsEmptyExtensionResource(const ExtensionResource& resource) {
  // Note that just checking for ExtensionResource::empty() isn't correct since
  // it checks |ExtensionResource::extension_root()::empty()| which can return
  // true for a dummy extension created as part of the webstore installation
  // flow. See crbug.com/1087348.
  return resource.extension_id().empty() && resource.extension_root().empty() &&
         resource.relative_path().empty();
}

}  // namespace

DNRManifestHandler::DNRManifestHandler() = default;
DNRManifestHandler::~DNRManifestHandler() = default;

bool DNRManifestHandler::Parse(Extension* extension, base::string16* error) {
  DCHECK(extension->manifest()->HasKey(keys::kDeclarativeNetRequestKey));
  DCHECK(IsAPIAvailable());

  if (!PermissionsParser::HasAPIPermission(
          extension, APIPermission::kDeclarativeNetRequest)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kDeclarativeNetRequestPermissionNeeded, kAPIPermission,
        keys::kDeclarativeNetRequestKey);
    return false;
  }

  const base::DictionaryValue* dict = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kDeclarativeNetRequestKey,
                                            &dict)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeNetRequestKey,
        keys::kDeclarativeNetRequestKey);
    return false;
  }

  const base::ListValue* rules_file_list = nullptr;
  if (!dict->GetList(keys::kDeclarativeRuleResourcesKey, &rules_file_list)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeRulesFileKey,
        keys::kDeclarativeNetRequestKey, keys::kDeclarativeRuleResourcesKey);
    return false;
  }

  std::vector<dnr_api::Ruleset> rulesets;
  if (!json_schema_compiler::util::PopulateArrayFromList(*rules_file_list,
                                                         &rulesets, error)) {
    return false;
  }

  if (rulesets.size() >
      static_cast<size_t>(dnr_api::MAX_NUMBER_OF_STATIC_RULESETS)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kRulesetCountExceeded, keys::kDeclarativeNetRequestKey,
        keys::kDeclarativeRuleResourcesKey,
        base::NumberToString(dnr_api::MAX_NUMBER_OF_STATIC_RULESETS));
    return false;
  }

  std::set<base::StringPiece> ruleset_ids;

  // Validates the ruleset at the given |index|. On success, returns true and
  // populates |info|. On failure, returns false and populates |error|.
  auto get_ruleset_info = [extension, error, &rulesets, &ruleset_ids](
                              int index, DNRManifestData::RulesetInfo* info) {
    // Path validation.
    ExtensionResource resource = extension->GetResource(rulesets[index].path);
    if (IsEmptyExtensionResource(resource) ||
        resource.relative_path().ReferencesParent()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
          keys::kDeclarativeRuleResourcesKey, rulesets[index].path);
      return false;
    }

    // ID validation.
    const std::string& manifest_id = rulesets[index].id;
    constexpr char kReservedRulesetIDPrefix = '_';

    // Ensure that the dynamic ruleset ID is reserved.
    DCHECK_EQ(kReservedRulesetIDPrefix, dnr_api::DYNAMIC_RULESET_ID[0]);

    if (manifest_id.empty() || !ruleset_ids.insert(manifest_id).second ||
        manifest_id[0] == kReservedRulesetIDPrefix) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidRulesetID, keys::kDeclarativeNetRequestKey,
          keys::kDeclarativeRuleResourcesKey, base::NumberToString(index));
      return false;
    }

    info->relative_path = resource.relative_path().NormalizePathSeparators();

    info->id = RulesetID(kMinValidStaticRulesetID.value() + index);

    info->enabled = rulesets[index].enabled;
    info->manifest_id = manifest_id;
    return true;
  };

  std::vector<DNRManifestData::RulesetInfo> rulesets_info;
  rulesets_info.reserve(rulesets.size());

  // Note: the static_cast<int> below is safe because we did already verify that
  // |rulesets.size()| <= dnr_api::MAX_NUMBER_OF_STATIC_RULESETS, which is an
  // integer.
  for (int i = 0; i < static_cast<int>(rulesets.size()); ++i) {
    DNRManifestData::RulesetInfo info;
    if (!get_ruleset_info(i, &info))
      return false;

    rulesets_info.push_back(std::move(info));
  }

  extension->SetManifestData(
      keys::kDeclarativeNetRequestKey,
      std::make_unique<DNRManifestData>(std::move(rulesets_info)));
  return true;
}

bool DNRManifestHandler::Validate(const Extension* extension,
                                  std::string* error,
                                  std::vector<InstallWarning>* warnings) const {
  DCHECK(IsAPIAvailable());

  DNRManifestData* data = static_cast<DNRManifestData*>(
      extension->GetManifestData(manifest_keys::kDeclarativeNetRequestKey));
  DCHECK(data);

  for (const DNRManifestData::RulesetInfo& info : data->rulesets) {
    // Check file path validity. We don't use Extension::GetResource since it
    // returns a failure if the relative path contains Windows path separators
    // and we have already normalized the path separators.
    if (ExtensionResource::GetFilePath(
            extension->path(), info.relative_path,
            ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT)
            .empty()) {
      *error = ErrorUtils::FormatErrorMessage(
          errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
          keys::kDeclarativeRuleResourcesKey,
          info.relative_path.AsUTF8Unsafe());
      return false;
    }
  }

  return true;
}

base::span<const char* const> DNRManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kDeclarativeNetRequestKey};
  return kKeys;
}

}  // namespace declarative_net_request
}  // namespace extensions
