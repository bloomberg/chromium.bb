// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/managed_mode_private/managed_mode_handler.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_manifest_keys;

namespace extensions {

ManagedModeInfo::ManagedModeInfo() {
}

ManagedModeInfo::~ManagedModeInfo() {
}

// static
bool ManagedModeInfo::IsContentPack(const Extension* extension) {
  ManagedModeInfo* info = static_cast<ManagedModeInfo*>(
      extension->GetManifestData(keys::kContentPack));
  return info ? !info->site_list.empty() : false;
}

// static
ExtensionResource ManagedModeInfo::GetContentPackSiteList(
    const Extension* extension) {
  ManagedModeInfo* info = static_cast<ManagedModeInfo*>(
    extension->GetManifestData(keys::kContentPack));
  return info && !info->site_list.empty() ?
      extension->GetResource(info->site_list) :
      ExtensionResource();
}

ManagedModeHandler::ManagedModeHandler() {
}

ManagedModeHandler::~ManagedModeHandler() {
}

bool ManagedModeHandler::Parse(Extension* extension, string16* error) {
  if (!extension->manifest()->HasKey(keys::kContentPack))
    return true;

  scoped_ptr<ManagedModeInfo> info(new ManagedModeInfo);
  const base::DictionaryValue* content_pack_value = NULL;
  if (!extension->manifest()->GetDictionary(keys::kContentPack,
                                            &content_pack_value)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidContentPack);
    return false;
  }

  if (!LoadSites(info.get(), content_pack_value, error) ||
      !LoadConfigurations(info.get(), content_pack_value, error)) {
    return false;
  }

  extension->SetManifestData(keys::kContentPack, info.release());
  return true;
}

const std::vector<std::string> ManagedModeHandler::Keys() const {
  return SingleKey(keys::kContentPack);
}

bool ManagedModeHandler::LoadSites(
    ManagedModeInfo* info,
    const base::DictionaryValue* content_pack_value,
    string16* error) {
  if (!content_pack_value->HasKey(keys::kContentPackSites))
    return true;

  base::FilePath::StringType site_list_string;
  if (!content_pack_value->GetString(keys::kContentPackSites,
                                     &site_list_string)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidContentPackSites);
    return false;
  }

  info->site_list = base::FilePath(site_list_string);

  return true;
}

bool ManagedModeHandler::LoadConfigurations(
    ManagedModeInfo* info,
    const base::DictionaryValue* content_pack_value,
    string16* error) {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace extensions
