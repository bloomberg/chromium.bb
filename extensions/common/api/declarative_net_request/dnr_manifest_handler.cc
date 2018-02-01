// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace declarative_net_request {

DNRManifestHandler::DNRManifestHandler() = default;
DNRManifestHandler::~DNRManifestHandler() = default;

// Retrieves the "hosts" key from the kDeclarativeNetRequestKey manifest key
// dictionary. Returns false in case of an error.
bool GetHostsList(const base::DictionaryValue& dnr_dict,
                  std::vector<std::string>* hosts) {
  DCHECK(hosts);

  const auto* host_permissions = dnr_dict.FindKey(keys::kDeclarativeHostsKey);
  if (!host_permissions || !host_permissions->is_list())
    return false;

  for (const auto& host : host_permissions->GetList()) {
    if (!host.is_string())
      return false;
    hosts->push_back(host.GetString());
  }

  return true;
}

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

  // Parse host permissions.
  std::vector<std::string> hosts;
  if (!GetHostsList(*dict, &hosts)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeHostList, keys::kDeclarativeNetRequestKey,
        keys::kDeclarativeHostsKey);
    return false;
  }

  std::vector<std::string> malformed_hosts, invalid_scheme_hosts;
  URLPatternSet host_permissions;
  PermissionsParser::ParseHostPermissions(
      extension, hosts, PermissionsParser::GetRequiredAPIPermissions(extension),
      &host_permissions, &malformed_hosts, &invalid_scheme_hosts);

  if (!malformed_hosts.empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeHost, malformed_hosts[0],
        keys::kDeclarativeNetRequestKey, keys::kDeclarativeHostsKey);
    return false;
  }

  for (const auto& invalid_scheme_host : invalid_scheme_hosts) {
    std::string warning = ErrorUtils::FormatErrorMessage(
        errors::kInvalidDeclarativeHostScheme, invalid_scheme_host,
        keys::kDeclarativeNetRequestKey, keys::kDeclarativeHostsKey);
    extension->AddInstallWarning(InstallWarning(
        warning, keys::kDeclarativeNetRequestKey, keys::kDeclarativeHostsKey));
  }

  if (host_permissions.is_empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kEmptyDeclarativeHosts, keys::kDeclarativeNetRequestKey,
        keys::kDeclarativeHostsKey);
    return false;
  }

  PermissionsParser::SetHostPermissionsForDNR(extension,
                                              std::move(host_permissions));

  // Parse rule resources. Only a single rules file is supported currently.
  const base::ListValue* rules_file_list = nullptr;
  std::string json_ruleset_location;
  if (!dict->GetList(keys::kDeclarativeRuleResourcesKey, &rules_file_list) ||
      rules_file_list->GetSize() != 1u ||
      !rules_file_list->GetString(0, &json_ruleset_location)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeRulesFileKey,
        keys::kDeclarativeNetRequestKey, keys::kDeclarativeRuleResourcesKey);
    return false;
  }

  extension->SetManifestData(
      keys::kDeclarativeNetRequestKey,
      std::make_unique<DNRManifestData>(
          extension->GetResource(json_ruleset_location)));
  return true;
}

bool DNRManifestHandler::Validate(const Extension* extension,
                                  std::string* error,
                                  std::vector<InstallWarning>* warnings) const {
  DCHECK(IsAPIAvailable());

  const ExtensionResource* resource =
      DNRManifestData::GetRulesetResource(extension);
  DCHECK(resource);

  // Check file path validity.
  if (!resource->GetFilePath().empty())
    return true;

  *error = ErrorUtils::FormatErrorMessage(errors::kRulesFileIsInvalid,
                                          keys::kDeclarativeNetRequestKey,
                                          keys::kDeclarativeRuleResourcesKey);
  return false;
}

const std::vector<std::string> DNRManifestHandler::Keys() const {
  return SingleKey(keys::kDeclarativeNetRequestKey);
}

}  // namespace declarative_net_request
}  // namespace extensions
