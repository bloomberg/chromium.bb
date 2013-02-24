// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "chrome/browser/extensions/external_provider_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/app_pack_updater.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/extensions/default_apps.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/extensions/external_registry_loader_win.h"
#endif

using content::BrowserThread;

namespace extensions {

// Constants for keeping track of extension preferences in a dictionary.
const char ExternalProviderImpl::kExternalCrx[] = "external_crx";
const char ExternalProviderImpl::kExternalVersion[] = "external_version";
const char ExternalProviderImpl::kExternalUpdateUrl[] = "external_update_url";
const char ExternalProviderImpl::kSupportedLocales[] = "supported_locales";
const char ExternalProviderImpl::kIsBookmarkApp[] = "is_bookmark_app";
const char ExternalProviderImpl::kIsFromWebstore[] = "is_from_webstore";

ExternalProviderImpl::ExternalProviderImpl(
    VisitorInterface* service,
    ExternalLoader* loader,
    Manifest::Location crx_location,
    Manifest::Location download_location,
    int creation_flags)
  : crx_location_(crx_location),
    download_location_(download_location),
    service_(service),
    prefs_(NULL),
    ready_(false),
    loader_(loader),
    creation_flags_(creation_flags),
    auto_acknowledge_(false) {
  loader_->Init(this);
}

ExternalProviderImpl::~ExternalProviderImpl() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  loader_->OwnerShutdown();
}

void ExternalProviderImpl::VisitRegisteredExtension() {
  // The loader will call back to SetPrefs.
  loader_->StartLoading();
}

void ExternalProviderImpl::SetPrefs(DictionaryValue* prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check if the service is still alive. It is possible that it went
  // away while |loader_| was working on the FILE thread.
  if (!service_) return;

  prefs_.reset(prefs);
  ready_ = true; // Queries for extensions are allowed from this point.

  // Set of unsupported extensions that need to be deleted from prefs_.
  std::set<std::string> unsupported_extensions;

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

    base::FilePath::StringType external_crx;
    Value* external_version_value;
    std::string external_version;
    std::string external_update_url;

    bool has_external_crx = extension->GetString(kExternalCrx, &external_crx);

    bool has_external_version = false;
    if (extension->Get(kExternalVersion, &external_version_value)) {
      if (external_version_value->IsType(Value::TYPE_STRING)) {
        external_version_value->GetAsString(&external_version);
        has_external_version = true;
      } else {
        LOG(WARNING) << "Malformed extension dictionary for extension: "
                     << extension_id.c_str() << ". " << kExternalVersion
                     << " value must be a string.";
        continue;
      }
    }

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

    // Check that extension supports current browser locale.
    ListValue* supported_locales = NULL;
    if (extension->GetList(kSupportedLocales, &supported_locales)) {
      std::vector<std::string> browser_locales;
      l10n_util::GetParentLocales(g_browser_process->GetApplicationLocale(),
                                  &browser_locales);

      size_t num_locales = supported_locales->GetSize();
      bool locale_supported = false;
      for (size_t j = 0; j < num_locales; j++) {
        std::string current_locale;
        if (supported_locales->GetString(j, &current_locale) &&
            l10n_util::IsValidLocaleSyntax(current_locale)) {
          current_locale = l10n_util::NormalizeLocale(current_locale);
          if (std::find(browser_locales.begin(), browser_locales.end(),
                        current_locale) != browser_locales.end()) {
            locale_supported = true;
            break;
          }
        } else {
          LOG(WARNING) << "Unrecognized locale '" << current_locale
                       << "' found as supported locale for extension: "
                       << extension_id;
        }
      }

      if (!locale_supported) {
        unsupported_extensions.insert(extension_id);
        LOG(INFO) << "Skip installing (or uninstall) external extension: "
                  << extension_id << " because the extension doesn't support "
                  << "the browser locale.";
        continue;
      }
    }

    int creation_flags = creation_flags_;
    bool is_bookmark_app;
    if (extension->GetBoolean(kIsBookmarkApp, &is_bookmark_app) &&
        is_bookmark_app) {
      creation_flags |= Extension::FROM_BOOKMARK;
    }
    bool is_from_webstore;
    if (extension->GetBoolean(kIsFromWebstore, &is_from_webstore) &&
        is_from_webstore) {
      creation_flags |= Extension::FROM_WEBSTORE;
    }

    if (has_external_crx) {
      if (crx_location_ == Manifest::INVALID_LOCATION) {
        LOG(WARNING) << "This provider does not support installing external "
                     << "extensions from crx files.";
        continue;
      }
      if (external_crx.find(base::FilePath::kParentDirectory) !=
          base::StringPiece::npos) {
        LOG(WARNING) << "Path traversal not allowed in path: "
                     << external_crx.c_str();
        continue;
      }

      // If the path is relative, and the provider has a base path,
      // build the absolute path to the crx file.
      base::FilePath path(external_crx);
      if (!path.IsAbsolute()) {
        base::FilePath base_path = loader_->GetBaseCrxFilePath();
        if (base_path.empty()) {
          LOG(WARNING) << "File path " << external_crx.c_str()
                       << " is relative.  An absolute path is required.";
          continue;
        }
        path = base_path.Append(external_crx);
      }

      Version version(external_version);
      if (!version.IsValid()) {
        LOG(WARNING) << "Malformed extension dictionary for extension: "
                     << extension_id.c_str() << ".  Invalid version string \""
                     << external_version << "\".";
        continue;
      }
      service_->OnExternalExtensionFileFound(extension_id, &version, path,
                                             crx_location_, creation_flags,
                                             auto_acknowledge_);
    } else { // if (has_external_update_url)
      CHECK(has_external_update_url);  // Checking of keys above ensures this.
      if (download_location_ == Manifest::INVALID_LOCATION) {
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

  for (std::set<std::string>::iterator it = unsupported_extensions.begin();
       it != unsupported_extensions.end(); ++it) {
    // Remove extension for the list of know external extensions. The extension
    // will be uninstalled later because provider doesn't provide it anymore.
    prefs_->Remove(*it, NULL);
  }

  service_->OnExternalProviderReady(this);
}

void ExternalProviderImpl::ServiceShutdown() {
  service_ = NULL;
}

bool ExternalProviderImpl::IsReady() const {
  return ready_;
}

bool ExternalProviderImpl::HasExtension(
    const std::string& id) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(prefs_.get());
  CHECK(ready_);
  return prefs_->HasKey(id);
}

bool ExternalProviderImpl::GetExtensionDetails(
    const std::string& id, Manifest::Location* location,
    scoped_ptr<Version>* version) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(prefs_.get());
  CHECK(ready_);
  DictionaryValue* extension = NULL;
  if (!prefs_->GetDictionary(id, &extension))
    return false;

  Manifest::Location loc = Manifest::INVALID_LOCATION;
  if (extension->HasKey(kExternalUpdateUrl)) {
    loc = download_location_;

  } else if (extension->HasKey(kExternalCrx)) {
    loc = crx_location_;

    std::string external_version;
    if (!extension->GetString(kExternalVersion, &external_version))
      return false;

    if (version)
      version->reset(new Version(external_version));

  } else {
    NOTREACHED();  // Chrome should not allow prefs to get into this state.
    return false;
  }

  if (location)
    *location = loc;

  return true;
}

// static
void ExternalProviderImpl::CreateExternalProviders(
    VisitorInterface* service,
    Profile* profile,
    ProviderCollection* provider_list) {
  // Policies are mandatory so they can't be skipped with command line flag.
  provider_list->push_back(
      linked_ptr<ExternalProviderInterface>(
          new ExternalProviderImpl(
              service,
              new ExternalPolicyLoader(profile),
              Manifest::INVALID_LOCATION,
              Manifest::EXTERNAL_POLICY_DOWNLOAD,
              Extension::NO_FLAGS)));

  // In tests don't install extensions from default external sources.
  // It would only slowdown tests and make them flaky.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDefaultApps))
    return;

  // On Mac OS, items in /Library/... should be written by the superuser.
  // Check that all components of the path are writable by root only.
  ExternalPrefLoader::Options check_admin_permissions_on_mac;
#if defined(OS_MACOSX)
  check_admin_permissions_on_mac =
    ExternalPrefLoader::ENSURE_PATH_CONTROLLED_BY_ADMIN;
#else
  check_admin_permissions_on_mac = ExternalPrefLoader::NONE;
#endif

  bool is_chromeos_demo_session = false;
  int bundled_extension_creation_flags = Extension::NO_FLAGS;
#if defined(OS_CHROMEOS)
  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  is_chromeos_demo_session =
      user_manager && user_manager->IsLoggedInAsDemoUser() &&
      g_browser_process->browser_policy_connector()->GetDeviceMode() ==
          policy::DEVICE_MODE_KIOSK;
  bundled_extension_creation_flags = Extension::FROM_WEBSTORE |
      Extension::WAS_INSTALLED_BY_DEFAULT;
#endif

  bool is_managed_profile = false;
  int external_apps_path_id = chrome::DIR_EXTERNAL_EXTENSIONS;
#if defined(ENABLE_MANAGED_USERS)
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  is_managed_profile = managed_user_service->ProfileIsManaged();
  if (is_managed_profile)
    external_apps_path_id = chrome::DIR_MANAGED_USERS_DEFAULT_APPS;
#endif

  if (!is_chromeos_demo_session) {
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                new ExternalPrefLoader(external_apps_path_id,
                                       check_admin_permissions_on_mac),
                Manifest::EXTERNAL_PREF,
                Manifest::EXTERNAL_PREF_DOWNLOAD,
                bundled_extension_creation_flags)));
  }

  if (!is_managed_profile) {
#if defined(OS_CHROMEOS) || defined (OS_MACOSX)
    // Define a per-user source of external extensions.
    // On Chrome OS, this serves as a source for OEM customization.
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                new ExternalPrefLoader(chrome::DIR_USER_EXTERNAL_EXTENSIONS,
                                      ExternalPrefLoader::NONE),
                Manifest::EXTERNAL_PREF,
                Manifest::EXTERNAL_PREF_DOWNLOAD,
                Extension::NO_FLAGS)));
#endif

#if defined(OS_WIN)
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                new ExternalRegistryLoader,
                Manifest::EXTERNAL_REGISTRY,
                Manifest::INVALID_LOCATION,
                Extension::NO_FLAGS)));
#endif

#if defined(OS_LINUX)
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                new ExternalPrefLoader(
                    chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                    ExternalPrefLoader::NONE),
                Manifest::EXTERNAL_PREF,
                Manifest::EXTERNAL_PREF_DOWNLOAD,
                bundled_extension_creation_flags)));
#endif

#if !defined(OS_CHROMEOS)
    // The default apps are installed as INTERNAL but use the external
    // extension installer codeflow.
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new default_apps::Provider(
                profile,
                service,
                new ExternalPrefLoader(chrome::DIR_DEFAULT_APPS,
                                       ExternalPrefLoader::NONE),
                Manifest::INTERNAL,
                Manifest::INVALID_LOCATION,
                Extension::FROM_WEBSTORE |
                    Extension::WAS_INSTALLED_BY_DEFAULT)));
#endif

#if defined(OS_CHROMEOS)
    policy::BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    if (is_chromeos_demo_session && connector->GetAppPackUpdater()) {
      provider_list->push_back(
          linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                connector->GetAppPackUpdater()->CreateExternalLoader(),
                Manifest::EXTERNAL_PREF,
                Manifest::INVALID_LOCATION,
                Extension::NO_FLAGS)));
    }
#endif
  }
}

}  // namespace extensions
