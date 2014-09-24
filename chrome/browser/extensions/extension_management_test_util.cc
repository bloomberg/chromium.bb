// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_test_util.h"

#include "components/crx_file/id_util.h"

namespace extensions {

namespace schema = schema_constants;

namespace {

std::string make_path(std::string a, std::string b) {
  return a + "." + b;
}

const char kInstallSourcesPath[] = "*.install_sources";
const char kAllowedTypesPath[] = "*.allowed_types";

}  // namespace

ExtensionManagementPrefUpdaterBase::ExtensionManagementPrefUpdaterBase() {
}

ExtensionManagementPrefUpdaterBase::~ExtensionManagementPrefUpdaterBase() {
}

void ExtensionManagementPrefUpdaterBase::UnsetPerExtensionSettings(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_->RemoveWithoutPathExpansion(id, NULL);
}

void ExtensionManagementPrefUpdaterBase::ClearPerExtensionSettings(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_->SetWithoutPathExpansion(id, new base::DictionaryValue());
}

void ExtensionManagementPrefUpdaterBase::SetBlacklistedByDefault(bool value) {
  pref_->SetString(make_path(schema::kWildcard, schema::kInstallationMode),
                   value ? schema::kBlocked : schema::kAllowed);
}

void ExtensionManagementPrefUpdaterBase::
    ClearInstallationModesForIndividualExtensions() {
  for (base::DictionaryValue::Iterator it(*pref_.get()); !it.IsAtEnd();
       it.Advance()) {
    DCHECK(it.value().IsType(base::Value::TYPE_DICTIONARY));
    if (it.key() != schema::kWildcard) {
      DCHECK(crx_file::id_util::IdIsValid(it.key()));
      pref_->Remove(make_path(it.key(), schema::kInstallationMode), NULL);
      pref_->Remove(make_path(it.key(), schema::kUpdateUrl), NULL);
    }
  }
}

void
ExtensionManagementPrefUpdaterBase::SetIndividualExtensionInstallationAllowed(
    const ExtensionId& id,
    bool allowed) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_->SetString(make_path(id, schema::kInstallationMode),
                   allowed ? schema::kAllowed : schema::kBlocked);
  pref_->Remove(make_path(id, schema::kUpdateUrl), NULL);
}

void ExtensionManagementPrefUpdaterBase::SetIndividualExtensionAutoInstalled(
    const ExtensionId& id,
    const std::string& update_url,
    bool forced) {
  DCHECK(crx_file::id_util::IdIsValid(id));
  pref_->SetString(make_path(id, schema::kInstallationMode),
                   forced ? schema::kForceInstalled : schema::kNormalInstalled);
  pref_->SetString(make_path(id, schema::kUpdateUrl), update_url);
}

void ExtensionManagementPrefUpdaterBase::UnsetInstallSources() {
  pref_->Remove(kInstallSourcesPath, NULL);
}

void ExtensionManagementPrefUpdaterBase::ClearInstallSources() {
  ClearList(kInstallSourcesPath);
}

void ExtensionManagementPrefUpdaterBase::AddInstallSource(
    const std::string& install_source) {
  AddStringToList(kInstallSourcesPath, install_source);
}

void ExtensionManagementPrefUpdaterBase::RemoveInstallSource(
    const std::string& install_source) {
  RemoveStringFromList(kInstallSourcesPath, install_source);
}

void ExtensionManagementPrefUpdaterBase::UnsetAllowedTypes() {
  pref_->Remove(kAllowedTypesPath, NULL);
}

void ExtensionManagementPrefUpdaterBase::ClearAllowedTypes() {
  ClearList(kAllowedTypesPath);
}

void ExtensionManagementPrefUpdaterBase::AddAllowedType(
    const std::string& allowed_type) {
  AddStringToList(kAllowedTypesPath, allowed_type);
}

const base::DictionaryValue* ExtensionManagementPrefUpdaterBase::GetPref() {
  return pref_.get();
}

void ExtensionManagementPrefUpdaterBase::SetPref(base::DictionaryValue* pref) {
  pref_.reset(pref);
}

scoped_ptr<base::DictionaryValue>
ExtensionManagementPrefUpdaterBase::TakePref() {
  return pref_.Pass();
}

void ExtensionManagementPrefUpdaterBase::RemoveAllowedType(
    const std::string& allowd_type) {
  RemoveStringFromList(kAllowedTypesPath, allowd_type);
}

void ExtensionManagementPrefUpdaterBase::ClearList(const std::string& path) {
  pref_->Set(path, new base::ListValue());
}

void ExtensionManagementPrefUpdaterBase::AddStringToList(
    const std::string& path,
    const std::string& str) {
  base::ListValue* list_value = NULL;
  if (!pref_->GetList(path, &list_value)) {
    list_value = new base::ListValue();
    pref_->Set(path, list_value);
  }
  CHECK(list_value->AppendIfNotPresent(new base::StringValue(str)));
}

void ExtensionManagementPrefUpdaterBase::RemoveStringFromList(
    const std::string& path,
    const std::string& str) {
  base::ListValue* list_value = NULL;
  if (pref_->GetList(path, &list_value))
    CHECK(list_value->Remove(base::StringValue(str), NULL));
}

}  // namespace extensions
