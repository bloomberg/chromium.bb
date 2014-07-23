// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_component_loader.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/app_pack_updater.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#else
#include "chrome/browser/extensions/default_apps.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/extensions/external_registry_loader_win.h"
#endif

using content::BrowserThread;

namespace extensions {

// Constants for keeping track of extension preferences in a dictionary.
const char ExternalProviderImpl::kInstallParam[] = "install_parameter";
const char ExternalProviderImpl::kExternalCrx[] = "external_crx";
const char ExternalProviderImpl::kExternalVersion[] = "external_version";
const char ExternalProviderImpl::kExternalUpdateUrl[] = "external_update_url";
const char ExternalProviderImpl::kIsBookmarkApp[] = "is_bookmark_app";
const char ExternalProviderImpl::kIsFromWebstore[] = "is_from_webstore";
const char ExternalProviderImpl::kKeepIfPresent[] = "keep_if_present";
const char ExternalProviderImpl::kWasInstalledByOem[] = "was_installed_by_oem";
const char ExternalProviderImpl::kSupportedLocales[] = "supported_locales";

ExternalProviderImpl::ExternalProviderImpl(
    VisitorInterface* service,
    const scoped_refptr<ExternalLoader>& loader,
    Profile* profile,
    Manifest::Location crx_location,
    Manifest::Location download_location,
    int creation_flags)
    : crx_location_(crx_location),
      download_location_(download_location),
      service_(service),
      ready_(false),
      loader_(loader),
      profile_(profile),
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

void ExternalProviderImpl::SetPrefs(base::DictionaryValue* prefs) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check if the service is still alive. It is possible that it went
  // away while |loader_| was working on the FILE thread.
  if (!service_) return;

  prefs_.reset(prefs);
  ready_ = true;  // Queries for extensions are allowed from this point.

  // Set of unsupported extensions that need to be deleted from prefs_.
  std::set<std::string> unsupported_extensions;

  // Notify ExtensionService about all the extensions this provider has.
  for (base::DictionaryValue::Iterator i(*prefs_); !i.IsAtEnd(); i.Advance()) {
    const std::string& extension_id = i.key();
    const base::DictionaryValue* extension = NULL;

    if (!Extension::IdIsValid(extension_id)) {
      LOG(WARNING) << "Malformed extension dictionary: key "
                   << extension_id.c_str() << " is not a valid id.";
      continue;
    }

    if (!i.value().GetAsDictionary(&extension)) {
      LOG(WARNING) << "Malformed extension dictionary: key "
                   << extension_id.c_str()
                   << " has a value that is not a dictionary.";
      continue;
    }

    base::FilePath::StringType external_crx;
    const base::Value* external_version_value = NULL;
    std::string external_version;
    std::string external_update_url;

    bool has_external_crx = extension->GetString(kExternalCrx, &external_crx);

    bool has_external_version = false;
    if (extension->Get(kExternalVersion, &external_version_value)) {
      if (external_version_value->IsType(base::Value::TYPE_STRING)) {
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
    const base::ListValue* supported_locales = NULL;
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
        VLOG(1) << "Skip installing (or uninstall) external extension: "
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
    bool keep_if_present;
    if (extension->GetBoolean(kKeepIfPresent, &keep_if_present) &&
        keep_if_present && profile_) {
      ExtensionServiceInterface* extension_service =
          ExtensionSystem::Get(profile_)->extension_service();
      const Extension* extension = extension_service ?
          extension_service->GetExtensionById(extension_id, true) : NULL;
      if (!extension) {
        VLOG(1) << "Skip installing (or uninstall) external extension: "
                << extension_id << " because the extension should be kept "
                << "only if it is already installed.";
        continue;
      }
    }
    bool was_installed_by_oem;
    if (extension->GetBoolean(kWasInstalledByOem, &was_installed_by_oem) &&
        was_installed_by_oem) {
      creation_flags |= Extension::WAS_INSTALLED_BY_OEM;
    }

    std::string install_parameter;
    extension->GetString(kInstallParam, &install_parameter);

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
    } else {  // if (has_external_update_url)
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
      service_->OnExternalExtensionUpdateUrlFound(extension_id,
                                                  install_parameter,
                                                  update_url,
                                                  download_location_,
                                                  creation_flags,
                                                  auto_acknowledge_);
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
  base::DictionaryValue* extension = NULL;
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
  scoped_refptr<ExternalLoader> external_loader;
  extensions::Manifest::Location crx_location = Manifest::INVALID_LOCATION;
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_chrome_os_public_session = false;
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  policy::DeviceLocalAccount::Type account_type;
  if (user && policy::IsDeviceLocalAccountUser(user->email(), &account_type)) {
    if (account_type == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION)
      is_chrome_os_public_session = true;
    policy::DeviceLocalAccountPolicyBroker* broker =
        connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
            user->email());
    if (broker) {
      external_loader = broker->extension_loader();
      crx_location = Manifest::EXTERNAL_POLICY;
    } else {
      NOTREACHED();
    }
  } else {
    external_loader = new ExternalPolicyLoader(profile);
  }
#else
  external_loader = new ExternalPolicyLoader(profile);
#endif

  // Policies are mandatory so they can't be skipped with command line flag.
  if (external_loader) {
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                external_loader,
                profile,
                crx_location,
                Manifest::EXTERNAL_POLICY_DOWNLOAD,
                Extension::NO_FLAGS)));
  }

  // Load the KioskAppExternalProvider when running in kiosk mode.
  if (chrome::IsRunningInForcedAppMode()) {
#if defined(OS_CHROMEOS)
    chromeos::KioskAppManager* kiosk_app_manager =
        chromeos::KioskAppManager::Get();
    DCHECK(kiosk_app_manager);
    if (kiosk_app_manager && !kiosk_app_manager->external_loader_created()) {
      provider_list->push_back(linked_ptr<ExternalProviderInterface>(
          new ExternalProviderImpl(service,
                                   kiosk_app_manager->CreateExternalLoader(),
                                   profile,
                                   Manifest::EXTERNAL_PREF,
                                   Manifest::INVALID_LOCATION,
                                   Extension::NO_FLAGS)));
    }
#endif
    return;
  }

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
      connector->GetDeviceMode() == policy::DEVICE_MODE_RETAIL_KIOSK;
  bundled_extension_creation_flags = Extension::FROM_WEBSTORE |
      Extension::WAS_INSTALLED_BY_DEFAULT;
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (!profile->IsSupervised()) {
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                new ExternalPrefLoader(
                    chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                    ExternalPrefLoader::NONE),
                profile,
                Manifest::EXTERNAL_PREF,
                Manifest::EXTERNAL_PREF_DOWNLOAD,
                bundled_extension_creation_flags)));
  }
#endif

#if defined(OS_CHROMEOS)
  if (!is_chromeos_demo_session && !is_chrome_os_public_session) {
    int external_apps_path_id = profile->IsSupervised() ?
        chrome::DIR_SUPERVISED_USERS_DEFAULT_APPS :
        chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS;
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(new ExternalProviderImpl(
            service,
            new ExternalPrefLoader(external_apps_path_id,
                                   ExternalPrefLoader::NONE),
            profile,
            Manifest::EXTERNAL_PREF,
            Manifest::EXTERNAL_PREF_DOWNLOAD,
            bundled_extension_creation_flags)));

    // OEM default apps.
    int oem_extension_creation_flags =
        bundled_extension_creation_flags | Extension::WAS_INSTALLED_BY_OEM;
    chromeos::ServicesCustomizationDocument* customization =
        chromeos::ServicesCustomizationDocument::GetInstance();
    provider_list->push_back(linked_ptr<ExternalProviderInterface>(
        new ExternalProviderImpl(service,
                                 customization->CreateExternalLoader(profile),
                                 profile,
                                 Manifest::EXTERNAL_PREF,
                                 Manifest::EXTERNAL_PREF_DOWNLOAD,
                                 oem_extension_creation_flags)));
  }

  policy::AppPackUpdater* app_pack_updater = connector->GetAppPackUpdater();
  if (is_chromeos_demo_session && app_pack_updater &&
      !app_pack_updater->created_external_loader()) {
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
          new ExternalProviderImpl(
              service,
              app_pack_updater->CreateExternalLoader(),
              profile,
              Manifest::EXTERNAL_PREF,
              Manifest::INVALID_LOCATION,
              Extension::NO_FLAGS)));
  }
#endif

  if (!profile->IsSupervised() && !is_chromeos_demo_session) {
#if !defined(OS_WIN)
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                new ExternalPrefLoader(chrome::DIR_EXTERNAL_EXTENSIONS,
                                       check_admin_permissions_on_mac),
                profile,
                Manifest::EXTERNAL_PREF,
                Manifest::EXTERNAL_PREF_DOWNLOAD,
                bundled_extension_creation_flags)));
#endif

    // Define a per-user source of external extensions.
#if defined(OS_MACOSX)
    provider_list->push_back(
        linked_ptr<ExternalProviderInterface>(
            new ExternalProviderImpl(
                service,
                new ExternalPrefLoader(chrome::DIR_USER_EXTERNAL_EXTENSIONS,
                                       ExternalPrefLoader::NONE),
                profile,
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
                profile,
                Manifest::EXTERNAL_REGISTRY,
                Manifest::EXTERNAL_PREF_DOWNLOAD,
                Extension::NO_FLAGS)));
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
                Manifest::INTERNAL,
                Extension::FROM_WEBSTORE |
                    Extension::WAS_INSTALLED_BY_DEFAULT)));
#endif

    provider_list->push_back(
      linked_ptr<ExternalProviderInterface>(
        new ExternalProviderImpl(
            service,
            new ExternalComponentLoader(profile),
            profile,
            Manifest::INVALID_LOCATION,
            Manifest::EXTERNAL_COMPONENT,
            Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT)));
  }
}

}  // namespace extensions
