// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_extension_provider.h"

#include "app/app_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/common/json_value_serializer.h"

// Constants for keeping track of extension preferences.
const char kLocation[] = "location";
const char kState[] = "state";
const char kExternalCrx[] = "external_crx";
const char kExternalVersion[] = "external_version";
const char kIncognito[] = "incognito";
const char kExternalUpdateUrl[] = "external_update_url";

ExternalPrefExtensionProvider::ExternalPrefExtensionProvider() {
  FilePath json_file;
  PathService::Get(app::DIR_EXTERNAL_EXTENSIONS, &json_file);
  json_file = json_file.Append(FILE_PATH_LITERAL("external_extensions.json"));

  if (file_util::PathExists(json_file)) {
    JSONFileValueSerializer serializer(json_file);
    SetPreferences(&serializer);
  } else {
    prefs_.reset(new DictionaryValue());
  }
}

ExternalPrefExtensionProvider::~ExternalPrefExtensionProvider() {
}

void ExternalPrefExtensionProvider::SetPreferencesForTesting(
    const std::string& json_data_for_testing) {
  JSONStringValueSerializer serializer(json_data_for_testing);
  SetPreferences(&serializer);
}

void ExternalPrefExtensionProvider::VisitRegisteredExtension(
    Visitor* visitor, const std::set<std::string>& ids_to_ignore) const {
  for (DictionaryValue::key_iterator i = prefs_->begin_keys();
       i != prefs_->end_keys(); ++i) {
    const std::string& extension_id = *i;
    if (ids_to_ignore.find(extension_id) != ids_to_ignore.end())
      continue;

    DictionaryValue* extension;
    if (!prefs_->GetDictionaryWithoutPathExpansion(extension_id, &extension))
      continue;

    FilePath::StringType external_crx;
    std::string external_version;
    std::string external_update_url;
    bool enable_incognito_on_install = false;

    bool has_external_crx = extension->GetString(kExternalCrx, &external_crx);
    bool has_external_version = extension->GetString(kExternalVersion,
                                                     &external_version);
    bool has_external_update_url = extension->GetString(kExternalUpdateUrl,
                                                        &external_update_url);
    extension->GetBoolean(kIncognito, &enable_incognito_on_install);

    if (has_external_crx != has_external_version) {
      LOG(WARNING) << "Malformed extension dictionary for extension: "
                   << extension_id.c_str() << ".  " << kExternalCrx
                   << " and " << kExternalVersion << " must be used together.";
      continue;
    }

    if (has_external_crx == has_external_update_url) {
      LOG(WARNING) << "Malformed extension dictionary for extension: "
                   << extension_id.c_str() << ".  Exactly one of the "
                   << "followng keys should be used: " << kExternalCrx
                   << ", " << kExternalUpdateUrl << ".";
      continue;
    }

    if (has_external_crx) {
      if (external_crx.find(FilePath::kParentDirectory) !=
          base::StringPiece::npos) {
        LOG(WARNING) << "Path traversal not allowed in path: "
                     << external_crx.c_str();
        continue;
      }

      // If the path is relative, make it absolute.
      FilePath path(external_crx);
      if (!path.IsAbsolute()) {
        // Try path as relative path from external extension dir.
        FilePath base_path;
        PathService::Get(app::DIR_EXTERNAL_EXTENSIONS, &base_path);
        path = base_path.Append(external_crx);
      }

      scoped_ptr<Version> version;
      version.reset(Version::GetVersionFromString(external_version));
      if (!version.get()) {
        LOG(ERROR) << "Malformed extension dictionary for extension: "
                   << extension_id.c_str() << ".  Invalid version string \""
                   << external_version << "\".";
        continue;
      }
      visitor->OnExternalExtensionFileFound(extension_id, version.get(), path,
                                            Extension::EXTERNAL_PREF);
      continue;
    }

    DCHECK(has_external_update_url);  // Checking of keys above ensures this.
    GURL update_url(external_update_url);
    if (!update_url.is_valid()) {
      LOG(WARNING) << "Malformed extension dictionary for extension: "
                   << extension_id.c_str() << ".  " << kExternalUpdateUrl
                   << " must be a valid URL.  Saw \"" << external_update_url
                   << "\".";
      continue;
    }
    visitor->OnExternalExtensionUpdateUrlFound(extension_id, update_url,
                                               enable_incognito_on_install);
  }
}

Version* ExternalPrefExtensionProvider::RegisteredVersion(
    const std::string& id, Extension::Location* location) const {
  DictionaryValue* extension = NULL;
  if (!prefs_->GetDictionary(id, &extension))
    return NULL;

  std::string external_version;
  if (!extension->GetString(kExternalVersion, &external_version))
    return NULL;

  if (location)
    *location = Extension::EXTERNAL_PREF;
  return Version::GetVersionFromString(external_version);
}

void ExternalPrefExtensionProvider::SetPreferences(
    ValueSerializer* serializer) {
  std::string error_msg;
  Value* extensions = serializer->Deserialize(NULL, &error_msg);
  scoped_ptr<DictionaryValue> dictionary(new DictionaryValue());
  if (!extensions) {
    LOG(WARNING) << "Unable to deserialize json data: " << error_msg;
  } else {
    if (!extensions->IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED() << "Invalid json data";
    } else {
      dictionary.reset(static_cast<DictionaryValue*>(extensions));
    }
  }
  prefs_.reset(dictionary.release());
}
