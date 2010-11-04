// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/stateful_external_extension_provider.h"

#include "app/app_paths.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "base/version.h"

namespace {

// Constants for keeping track of extension preferences.
const char kLocation[] = "location";
const char kState[] = "state";
const char kExternalCrx[] = "external_crx";
const char kExternalVersion[] = "external_version";
const char kExternalUpdateUrl[] = "external_update_url";

}

StatefulExternalExtensionProvider::StatefulExternalExtensionProvider(
    Extension::Location crx_location,
    Extension::Location download_location)
  : crx_location_(crx_location),
    download_location_(download_location) {
}

StatefulExternalExtensionProvider::~StatefulExternalExtensionProvider() {
}

void StatefulExternalExtensionProvider::VisitRegisteredExtension(
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

    bool has_external_crx = extension->GetString(kExternalCrx, &external_crx);
    bool has_external_version = extension->GetString(kExternalVersion,
                                                     &external_version);
    bool has_external_update_url = extension->GetString(kExternalUpdateUrl,
                                                        &external_update_url);
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
      if (crx_location_ == Extension::INVALID) {
        LOG(WARNING) << "This provider does not support installing external "
                     << "extensions from crx files.";
        continue;
      }
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
        LOG(WARNING) << "Malformed extension dictionary for extension: "
                     << extension_id.c_str() << ".  Invalid version string \""
                     << external_version << "\".";
        continue;
      }
      visitor->OnExternalExtensionFileFound(extension_id, version.get(), path,
                                            crx_location_);
    } else { // if (has_external_update_url)
      DCHECK(has_external_update_url);  // Checking of keys above ensures this.
      if (download_location_ == Extension::INVALID) {
        LOG(WARNING) << "This provider does not support installing external "
                     << "extensions from update URLs.";
        continue;
      }
      GURL update_url(external_update_url);
      if (!update_url.is_valid()) {
        LOG(WARNING) << "Malformed extension dictionary for extension: "
                     << extension_id.c_str() << ".  " << kExternalUpdateUrl
                     << " must be a valid URL.  Saw \"" << external_update_url
                     << "\".";
        continue;
      }
      visitor->OnExternalExtensionUpdateUrlFound(
          extension_id, update_url, download_location_);
    }
  }
}

bool StatefulExternalExtensionProvider::HasExtension(
    const std::string& id) const {
  return prefs_->HasKey(id);
}

bool StatefulExternalExtensionProvider::GetExtensionDetails(
    const std::string& id, Extension::Location* location,
    scoped_ptr<Version>* version) const {
  DictionaryValue* extension = NULL;
  if (!prefs_->GetDictionary(id, &extension))
    return false;

  Extension::Location loc = Extension::INVALID;
  if (extension->HasKey(kExternalUpdateUrl)) {
    loc = download_location_;

  } else if (extension->HasKey(kExternalCrx)) {
    loc = crx_location_;

    std::string external_version;
    if (!extension->GetString(kExternalVersion, &external_version))
      return false;

    if (version)
      version->reset(Version::GetVersionFromString(external_version));

  } else {
    NOTREACHED();  // Chrome should not allow prefs to get into this state.
    return false;
  }

  if (location)
    *location = loc;

  return true;
}
