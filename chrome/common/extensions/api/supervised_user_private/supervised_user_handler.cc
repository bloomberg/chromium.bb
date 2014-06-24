// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/supervised_user_private/supervised_user_handler.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;

SupervisedUserInfo::SupervisedUserInfo() {
}

SupervisedUserInfo::~SupervisedUserInfo() {
}

// static
bool SupervisedUserInfo::IsContentPack(const Extension* extension) {
  SupervisedUserInfo* info = static_cast<SupervisedUserInfo*>(
      extension->GetManifestData(keys::kContentPack));
  return info ? !info->site_list.empty() : false;
}

// static
ExtensionResource SupervisedUserInfo::GetContentPackSiteList(
    const Extension* extension) {
  SupervisedUserInfo* info = static_cast<SupervisedUserInfo*>(
      extension->GetManifestData(keys::kContentPack));
  return info && !info->site_list.empty()
             ? extension->GetResource(info->site_list)
             : ExtensionResource();
}

SupervisedUserHandler::SupervisedUserHandler() {
}

SupervisedUserHandler::~SupervisedUserHandler() {
}

bool SupervisedUserHandler::Parse(Extension* extension, base::string16* error) {
  if (!extension->manifest()->HasKey(keys::kContentPack))
    return true;

  scoped_ptr<SupervisedUserInfo> info(new SupervisedUserInfo);
  const base::DictionaryValue* content_pack_value = NULL;
  if (!extension->manifest()->GetDictionary(keys::kContentPack,
                                            &content_pack_value)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidContentPack);
    return false;
  }

  if (!LoadSites(info.get(), content_pack_value, error) ||
      !LoadConfigurations(info.get(), content_pack_value, error)) {
    return false;
  }

  extension->SetManifestData(keys::kContentPack, info.release());
  return true;
}

const std::vector<std::string> SupervisedUserHandler::Keys() const {
  return SingleKey(keys::kContentPack);
}

bool SupervisedUserHandler::LoadSites(
    SupervisedUserInfo* info,
    const base::DictionaryValue* content_pack_value,
    base::string16* error) {
  if (!content_pack_value->HasKey(keys::kContentPackSites))
    return true;

  base::FilePath::StringType site_list_string;
  if (!content_pack_value->GetString(keys::kContentPackSites,
                                     &site_list_string)) {
    *error = base::ASCIIToUTF16(manifest_errors::kInvalidContentPackSites);
    return false;
  }

  info->site_list = base::FilePath(site_list_string);

  return true;
}

bool SupervisedUserHandler::LoadConfigurations(
    SupervisedUserInfo* info,
    const base::DictionaryValue* content_pack_value,
    base::string16* error) {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace extensions
