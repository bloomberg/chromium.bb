// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_extension_provider_impl.h"

#include "app/app_paths.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/linked_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/external_extension_provider_interface.h"
#include "chrome/browser/extensions/external_policy_extension_loader.h"
#include "chrome/browser/extensions/external_pref_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"

#if defined(OS_WIN)
#include "chrome/browser/extensions/external_registry_extension_loader_win.h"
#endif

// Constants for keeping track of extension preferences in a dictionary.
const char ExternalExtensionProviderImpl::kLocation[] = "location";
const char ExternalExtensionProviderImpl::kState[] = "state";
const char ExternalExtensionProviderImpl::kExternalCrx[] = "external_crx";
const char ExternalExtensionProviderImpl::kExternalVersion[] =
    "external_version";
const char ExternalExtensionProviderImpl::kExternalUpdateUrl[] =
    "external_update_url";

ExternalExtensionProviderImpl::ExternalExtensionProviderImpl(
    VisitorInterface* service,
    ExternalExtensionLoader* loader,
    Extension::Location crx_location,
    Extension::Location download_location)
  : crx_location_(crx_location),
    download_location_(download_location),
    service_(service),
    prefs_(NULL),
    ready_(false),
    loader_(loader) {
  loader_->Init(this);
}

ExternalExtensionProviderImpl::~ExternalExtensionProviderImpl() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  loader_->OwnerShutdown();
}

void ExternalExtensionProviderImpl::VisitRegisteredExtension() const {
  // The loader will call back to SetPrefs.
  loader_->StartLoading();
}

void ExternalExtensionProviderImpl::SetPrefs(DictionaryValue* prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check if the service is still alive. It is possible that it had went
  // away while |loader_| was working on the FILE thread.
  if (!service_) return;

  prefs_.reset(prefs);
  ready_ = true; // Queries for extensions are allowed from this point.

  // Notify ExtensionService about all the extensions this provider has.
  for (DictionaryValue::key_iterator i = prefs_->begin_keys();
       i != prefs_->end_keys(); ++i) {
    const std::string& extension_id = *i;
    DictionaryValue* extension;

    if (!Extension::IdIsValid(extension_id)) {
      LOG(WARNING) << "Malformed extension dictionary: key "
                   << extension_id.c_str() << " is not a valid id.";
      continue;
    }

    if (!prefs_->GetDictionaryWithoutPathExpansion(extension_id, &extension)) {
      LOG(WARNING) << "Malformed extension dictionary: key "
                   << extension_id.c_str()
                   << " has a value that is not a dictionary.";
      continue;
    }

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

      // If the path is relative, and the provider has a base path,
      // build the absolute path to the crx file.
      FilePath path(external_crx);
      if (!path.IsAbsolute()) {
        FilePath base_path = loader_->GetBaseCrxFilePath();
        if (base_path.empty()) {
          LOG(WARNING) << "File path " << external_crx.c_str()
                       << " is relative.  An absolute path is required.";
          continue;
        }
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
      service_->OnExternalExtensionFileFound(extension_id, version.get(), path,
                                             crx_location_);
    } else { // if (has_external_update_url)
      CHECK(has_external_update_url);  // Checking of keys above ensures this.
      if (download_location_ == Extension::INVALID) {
        LOG(WARNING) << "This provider does not support installing external "
                     << "extensions from update URLs.";
        continue;
      }
      GURL update_url(external_update_url);
      if (!update_url.is_valid()) {
        LOG(WARNING) << "Malformed extension dictionary for extension: "
                     << extension_id.c_str() << ".  Key " << kExternalUpdateUrl
                     << " has value \"" << external_update_url
                     << "\", which is not a valid URL.";
        continue;
      }
      service_->OnExternalExtensionUpdateUrlFound(
          extension_id, update_url, download_location_);
    }
  }

  service_->OnExternalProviderReady();
}

void ExternalExtensionProviderImpl::ServiceShutdown() {
  service_ = NULL;
}

bool ExternalExtensionProviderImpl::IsReady() {
  return ready_;
}

bool ExternalExtensionProviderImpl::HasExtension(
    const std::string& id) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(prefs_.get());
  CHECK(ready_);
  return prefs_->HasKey(id);
}

bool ExternalExtensionProviderImpl::GetExtensionDetails(
    const std::string& id, Extension::Location* location,
    scoped_ptr<Version>* version) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(prefs_.get());
  CHECK(ready_);
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

// static
void ExternalExtensionProviderImpl::CreateExternalProviders(
    VisitorInterface* service,
    Profile* profile,
    ProviderCollection* provider_list) {
  provider_list->push_back(
      linked_ptr<ExternalExtensionProviderInterface>(
          new ExternalExtensionProviderImpl(
              service,
              new ExternalPrefExtensionLoader(
                  app::DIR_EXTERNAL_EXTENSIONS),
              Extension::EXTERNAL_PREF,
              Extension::EXTERNAL_PREF_DOWNLOAD)));

#if defined(OS_CHROMEOS)
  // Chrome OS specific source for OEM customization.
  provider_list->push_back(
      linked_ptr<ExternalExtensionProviderInterface>(
          new ExternalExtensionProviderImpl(
              service,
              new ExternalPrefExtensionLoader(
                  chrome::DIR_USER_EXTERNAL_EXTENSIONS),
              Extension::EXTERNAL_PREF,
              Extension::EXTERNAL_PREF_DOWNLOAD)));
#endif
#if defined(OS_WIN)
  provider_list->push_back(
      linked_ptr<ExternalExtensionProviderInterface>(
          new ExternalExtensionProviderImpl(
              service,
              new ExternalRegistryExtensionLoader,
              Extension::EXTERNAL_REGISTRY,
              Extension::INVALID)));
#endif
  provider_list->push_back(
      linked_ptr<ExternalExtensionProviderInterface>(
          new ExternalExtensionProviderImpl(
              service,
              new ExternalPolicyExtensionLoader(profile),
              Extension::INVALID,
              Extension::EXTERNAL_POLICY_DOWNLOAD)));
}
