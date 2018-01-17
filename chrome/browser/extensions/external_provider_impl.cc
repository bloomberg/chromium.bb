// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_migrator.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_component_loader.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/kiosk_app_external_loader.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/arc/arc_util.h"
#else
#include "chrome/browser/extensions/default_apps.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/extensions/external_registry_loader_win.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {

#if defined(OS_CHROMEOS)

// Certain default extensions are no longer needed on ARC devices as they were
// replaced by their ARC counterparts.
bool ShouldUninstallExtensionReplacedByArcApp(const std::string& extension_id) {
  if (arc::IsWebstoreSearchEnabled())
    return false;

  if (extension_id == extension_misc::kGooglePlayBooksAppId ||
      extension_id == extension_misc::kGooglePlayMoviesAppId ||
      extension_id == extension_misc::kGooglePlayMusicAppId) {
    return true;
  }

  return false;
}

#endif  // defined(OS_CHROMEOS)

}  // namespace

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
const char ExternalProviderImpl::kMayBeUntrusted[] = "may_be_untrusted";
const char ExternalProviderImpl::kMinProfileCreatedByVersion[] =
    "min_profile_created_by_version";
const char ExternalProviderImpl::kDoNotInstallForEnterprise[] =
    "do_not_install_for_enterprise";

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
      loader_(loader),
      profile_(profile),
      creation_flags_(creation_flags) {
  loader_->Init(this);
}

ExternalProviderImpl::~ExternalProviderImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  loader_->OwnerShutdown();
}

void ExternalProviderImpl::VisitRegisteredExtension() {
  // The loader will call back to SetPrefs.
  loader_->StartLoading();
}

void ExternalProviderImpl::SetPrefs(
    std::unique_ptr<base::DictionaryValue> prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check if the service is still alive. It is possible that it went
  // away while |loader_| was working on the FILE thread.
  if (!service_) return;

  prefs_ = std::move(prefs);
  ready_ = true;  // Queries for extensions are allowed from this point.

  std::vector<ExternalInstallInfoUpdateUrl> external_update_url_extensions;
  std::vector<ExternalInstallInfoFile> external_file_extensions;

  RetrieveExtensionsFromPrefs(&external_update_url_extensions,
                              &external_file_extensions);
  for (const auto& extension : external_update_url_extensions)
    service_->OnExternalExtensionUpdateUrlFound(extension, true);

  for (const auto& extension : external_file_extensions)
    service_->OnExternalExtensionFileFound(extension);

  service_->OnExternalProviderReady(this);
}

void ExternalProviderImpl::UpdatePrefs(
    std::unique_ptr<base::DictionaryValue> prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(allow_updates_);

  // Check if the service is still alive. It is possible that it went
  // away while |loader_| was working on the FILE thread.
  if (!service_)
    return;

  std::set<std::string> removed_extensions;
  // Find extensions that were removed by this ExternalProvider.
  for (base::DictionaryValue::Iterator i(*prefs_); !i.IsAtEnd(); i.Advance()) {
    const std::string& extension_id = i.key();
    // Don't bother about invalid ids.
    if (!crx_file::id_util::IdIsValid(extension_id))
      continue;
    if (!prefs->HasKey(extension_id))
      removed_extensions.insert(extension_id);
  }

  prefs_ = std::move(prefs);

  std::vector<ExternalInstallInfoUpdateUrl> external_update_url_extensions;
  std::vector<ExternalInstallInfoFile> external_file_extensions;
  RetrieveExtensionsFromPrefs(&external_update_url_extensions,
                              &external_file_extensions);

  // Notify ExtensionService about completion of finding incremental updates
  // from this provider.
  // Provide the list of added and removed extensions.
  service_->OnExternalProviderUpdateComplete(
      this, external_update_url_extensions, external_file_extensions,
      removed_extensions);
}

void ExternalProviderImpl::RetrieveExtensionsFromPrefs(
    std::vector<ExternalInstallInfoUpdateUrl>* external_update_url_extensions,
    std::vector<ExternalInstallInfoFile>* external_file_extensions) {
  // Set of unsupported extensions that need to be deleted from prefs_.
  std::set<std::string> unsupported_extensions;

  // Discover all the extensions this provider has.
  for (base::DictionaryValue::Iterator i(*prefs_); !i.IsAtEnd(); i.Advance()) {
    const std::string& extension_id = i.key();
    const base::DictionaryValue* extension = NULL;

#if defined(OS_CHROMEOS)
    if (ShouldUninstallExtensionReplacedByArcApp(extension_id)) {
      VLOG(1) << "Extension with key: " << extension_id << " was replaced "
              << "by a default ARC app, and will be uninstalled.";
      unsupported_extensions.emplace(extension_id);
      continue;
    }
#endif  // defined(OS_CHROMEOS)

    if (!crx_file::id_util::IdIsValid(extension_id)) {
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
      if (external_version_value->is_string()) {
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
          if (base::ContainsValue(browser_locales, current_locale)) {
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
    bool is_from_webstore = false;
    if (extension->GetBoolean(kIsFromWebstore, &is_from_webstore) &&
        is_from_webstore) {
      creation_flags |= Extension::FROM_WEBSTORE;
    }
    bool keep_if_present = false;
    if (extension->GetBoolean(kKeepIfPresent, &keep_if_present) &&
        keep_if_present && profile_) {
      ExtensionServiceInterface* extension_service =
          ExtensionSystem::Get(profile_)->extension_service();
      const Extension* extension = extension_service ?
          extension_service->GetExtensionById(extension_id, true) : NULL;
      if (!extension) {
        unsupported_extensions.insert(extension_id);
        VLOG(1) << "Skip installing (or uninstall) external extension: "
                << extension_id << " because the extension should be kept "
                << "only if it is already installed.";
        continue;
      }
    }
    bool was_installed_by_oem = false;
    if (extension->GetBoolean(kWasInstalledByOem, &was_installed_by_oem) &&
        was_installed_by_oem) {
      creation_flags |= Extension::WAS_INSTALLED_BY_OEM;
    }
    bool may_be_untrusted = false;
    if (extension->GetBoolean(kMayBeUntrusted, &may_be_untrusted) &&
        may_be_untrusted) {
      creation_flags |= Extension::MAY_BE_UNTRUSTED;
    }

    if (!HandleMinProfileVersion(extension, extension_id,
                                 &unsupported_extensions)) {
      continue;
    }

    if (!HandleDoNotInstallForEnterprise(extension, extension_id,
                                         &unsupported_extensions)) {
      continue;
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

      base::Version version(external_version);
      if (!version.IsValid()) {
        LOG(WARNING) << "Malformed extension dictionary for extension: "
                     << extension_id.c_str() << ".  Invalid version string \""
                     << external_version << "\".";
        continue;
      }
      external_file_extensions->emplace_back(
          extension_id, version, path, crx_location_, creation_flags,
          auto_acknowledge_, install_immediately_);
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
      external_update_url_extensions->emplace_back(
          extension_id, install_parameter, std::move(update_url),
          download_location_, creation_flags, auto_acknowledge_);
    }
  }

  for (std::set<std::string>::iterator it = unsupported_extensions.begin();
       it != unsupported_extensions.end(); ++it) {
    // Remove extension for the list of know external extensions. The extension
    // will be uninstalled later because provider doesn't provide it anymore.
    prefs_->Remove(*it, NULL);
  }
}

void ExternalProviderImpl::ServiceShutdown() {
  service_ = NULL;
}

bool ExternalProviderImpl::IsReady() const {
  return ready_;
}

bool ExternalProviderImpl::HasExtension(
    const std::string& id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(prefs_.get());
  CHECK(ready_);
  return prefs_->HasKey(id);
}

bool ExternalProviderImpl::GetExtensionDetails(
    const std::string& id,
    Manifest::Location* location,
    std::unique_ptr<base::Version>* version) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
      version->reset(new base::Version(external_version));

  } else {
    NOTREACHED();  // Chrome should not allow prefs to get into this state.
    return false;
  }

  if (location)
    *location = loc;

  return true;
}

bool ExternalProviderImpl::HandleMinProfileVersion(
    const base::DictionaryValue* extension,
    const std::string& extension_id,
    std::set<std::string>* unsupported_extensions) {
  std::string min_profile_created_by_version;
  if (profile_ &&
      extension->GetString(kMinProfileCreatedByVersion,
                           &min_profile_created_by_version)) {
    base::Version profile_version(
        profile_->GetPrefs()->GetString(prefs::kProfileCreatedByVersion));
    base::Version min_version(min_profile_created_by_version);
    if (min_version.IsValid() && profile_version.CompareTo(min_version) < 0) {
      unsupported_extensions->insert(extension_id);
      VLOG(1) << "Skip installing (or uninstall) external extension: "
              << extension_id
              << " profile.created_by_version: " << profile_version.GetString()
              << " min_profile_created_by_version: "
              << min_profile_created_by_version;
      return false;
    }
  }
  return true;
}

bool ExternalProviderImpl::HandleDoNotInstallForEnterprise(
    const base::DictionaryValue* extension,
    const std::string& extension_id,
    std::set<std::string>* unsupported_extensions) {
  bool do_not_install_for_enterprise = false;
  if (extension->GetBoolean(kDoNotInstallForEnterprise,
                            &do_not_install_for_enterprise) &&
      do_not_install_for_enterprise) {
    const policy::ProfilePolicyConnector* const connector =
        policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile_);
    if (connector->IsManaged()) {
      unsupported_extensions->insert(extension_id);
      VLOG(1) << "Skip installing (or uninstall) external extension "
              << extension_id << " restricted for managed user";
      return false;
    }
  }
  return true;
}

// static
void ExternalProviderImpl::CreateExternalProviders(
    VisitorInterface* service,
    Profile* profile,
    ProviderCollection* provider_list) {
  TRACE_EVENT0("browser,startup",
               "ExternalProviderImpl::CreateExternalProviders");
  scoped_refptr<ExternalLoader> external_loader;
  scoped_refptr<ExternalLoader> external_recommended_loader;
  extensions::Manifest::Location crx_location = Manifest::INVALID_LOCATION;

#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(profile)) {
    // Download apps installed by policy in the login profile. Flags
    // FROM_WEBSTORE/WAS_INSTALLED_BY_DEFAULT are applied because these apps are
    // downloaded from the webstore, and we want to treat them as built-in
    // extensions.
    external_loader = new ExternalPolicyLoader(
        ExtensionManagementFactory::GetForBrowserContext(profile),
        ExternalPolicyLoader::FORCED);
    auto signin_profile_provider = std::make_unique<ExternalProviderImpl>(
        service, external_loader, profile, crx_location,
        Manifest::EXTERNAL_POLICY_DOWNLOAD,
        Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT);
    signin_profile_provider->set_allow_updates(true);
    provider_list->push_back(std::move(signin_profile_provider));
    return;
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_chrome_os_public_session = false;
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  policy::DeviceLocalAccount::Type account_type;
  if (user && connector->IsEnterpriseManaged() &&
      policy::IsDeviceLocalAccountUser(user->GetAccountId().GetUserEmail(),
                                       &account_type)) {
    if (account_type == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION)
      is_chrome_os_public_session = true;
    policy::DeviceLocalAccountPolicyBroker* broker =
        connector->GetDeviceLocalAccountPolicyService()->GetBrokerForUser(
            user->GetAccountId().GetUserEmail());
    if (broker) {
      external_loader = broker->extension_loader();
      crx_location = Manifest::EXTERNAL_POLICY;
    } else {
      NOTREACHED();
    }
  } else {
    external_loader = new ExternalPolicyLoader(
        ExtensionManagementFactory::GetForBrowserContext(profile),
        ExternalPolicyLoader::FORCED);
    external_recommended_loader = new ExternalPolicyLoader(
        ExtensionManagementFactory::GetForBrowserContext(profile),
        ExternalPolicyLoader::RECOMMENDED);
  }
#else
  external_loader = new ExternalPolicyLoader(
      ExtensionManagementFactory::GetForBrowserContext(profile),
      ExternalPolicyLoader::FORCED);
  external_recommended_loader = new ExternalPolicyLoader(
      ExtensionManagementFactory::GetForBrowserContext(profile),
      ExternalPolicyLoader::RECOMMENDED);
#endif

  // Policies are mandatory so they can't be skipped with command line flag.
  if (external_loader.get()) {
    auto policy_provider = std::make_unique<ExternalProviderImpl>(
        service, external_loader, profile, crx_location,
        Manifest::EXTERNAL_POLICY_DOWNLOAD, Extension::NO_FLAGS);
    policy_provider->set_allow_updates(true);
    provider_list->push_back(std::move(policy_provider));
  }

  // Load the KioskAppExternalProvider when running in kiosk mode.
  if (chrome::IsRunningInForcedAppMode()) {
#if defined(OS_CHROMEOS)
    // Kiosk primary app external provider.
    // For enterprise managed kiosk apps, change the location to
    // "force-installed by policy".
    policy::BrowserPolicyConnectorChromeOS* const connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    Manifest::Location location = Manifest::EXTERNAL_PREF;
    if (connector && connector->IsEnterpriseManaged())
      location = Manifest::EXTERNAL_POLICY;

    std::unique_ptr<ExternalProviderImpl> kiosk_app_provider(
        new ExternalProviderImpl(
            service,
            base::MakeRefCounted<chromeos::KioskAppExternalLoader>(
                chromeos::KioskAppExternalLoader::AppClass::kPrimary),
            profile, location, Manifest::INVALID_LOCATION,
            Extension::NO_FLAGS));
    kiosk_app_provider->set_auto_acknowledge(true);
    kiosk_app_provider->set_install_immediately(true);
    kiosk_app_provider->set_allow_updates(true);
    provider_list->push_back(std::move(kiosk_app_provider));

    // Kiosk secondary app external provider.
    std::unique_ptr<ExternalProviderImpl> secondary_kiosk_app_provider(
        new ExternalProviderImpl(
            service,
            base::MakeRefCounted<chromeos::KioskAppExternalLoader>(
                chromeos::KioskAppExternalLoader::AppClass::kSecondary),
            profile, Manifest::EXTERNAL_PREF, Manifest::EXTERNAL_PREF_DOWNLOAD,
            Extension::NO_FLAGS));
    secondary_kiosk_app_provider->set_auto_acknowledge(true);
    secondary_kiosk_app_provider->set_install_immediately(true);
    secondary_kiosk_app_provider->set_allow_updates(true);
    provider_list->push_back(std::move(secondary_kiosk_app_provider));
#endif
    return;
  }

  // Extensions provided by recommended policies.
  if (external_recommended_loader.get()) {
    provider_list->push_back(std::make_unique<ExternalProviderImpl>(
        service, external_recommended_loader, profile, crx_location,
        Manifest::EXTERNAL_PREF_DOWNLOAD, Extension::NO_FLAGS));
  }

  // In tests don't install extensions from default external sources.
  // It would only slowdown tests and make them flaky.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
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

#if !defined(OS_WIN)
  int bundled_extension_creation_flags = Extension::NO_FLAGS;
#endif
#if defined(OS_CHROMEOS)
  bundled_extension_creation_flags = Extension::FROM_WEBSTORE |
      Extension::WAS_INSTALLED_BY_DEFAULT;

  if (!is_chrome_os_public_session) {
    std::vector<int> external_apps_path_ids;
    if (profile->IsChild()) {
      external_apps_path_ids.push_back(chrome::DIR_CHILD_USERS_DEFAULT_APPS);
    } else if (profile->IsSupervised()) {
      external_apps_path_ids.push_back(
          chrome::DIR_SUPERVISED_USERS_DEFAULT_APPS);
    } else {
      external_apps_path_ids.push_back(
          chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS);
      external_apps_path_ids.push_back(chrome::DIR_CHILD_USERS_DEFAULT_APPS);
    }
    const ExternalPrefLoader::Options pref_load_flags =
        profile->IsNewProfile()
            ? ExternalPrefLoader::DELAY_LOAD_UNTIL_PRIORITY_SYNC
            : ExternalPrefLoader::NONE;
    for (const auto external_apps_path_id : external_apps_path_ids) {
      provider_list->push_back(std::make_unique<ExternalProviderImpl>(
          service,
          new ExternalPrefLoader(external_apps_path_id, pref_load_flags,
                                 profile),
          profile, Manifest::EXTERNAL_PREF, Manifest::EXTERNAL_PREF_DOWNLOAD,
          bundled_extension_creation_flags));
    }

    // OEM default apps.
    int oem_extension_creation_flags =
        bundled_extension_creation_flags | Extension::WAS_INSTALLED_BY_OEM;
    chromeos::ServicesCustomizationDocument* customization =
        chromeos::ServicesCustomizationDocument::GetInstance();
    provider_list->push_back(std::make_unique<ExternalProviderImpl>(
        service, customization->CreateExternalLoader(profile), profile,
        Manifest::EXTERNAL_PREF, Manifest::EXTERNAL_PREF_DOWNLOAD,
        oem_extension_creation_flags));
  }
#elif defined(OS_LINUX)
  if (!profile->IsLegacySupervised()) {
    provider_list->push_back(std::make_unique<ExternalProviderImpl>(
        service,
        new ExternalPrefLoader(chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                               ExternalPrefLoader::NONE, nullptr),
        profile, Manifest::EXTERNAL_PREF, Manifest::EXTERNAL_PREF_DOWNLOAD,
        bundled_extension_creation_flags));
  }
#endif

  if (!profile->IsLegacySupervised()) {
#if defined(OS_WIN)
    auto registry_provider = std::make_unique<ExternalProviderImpl>(
        service, new ExternalRegistryLoader, profile,
        Manifest::EXTERNAL_REGISTRY, Manifest::EXTERNAL_PREF_DOWNLOAD,
        Extension::NO_FLAGS);
    registry_provider->set_allow_updates(true);
    provider_list->push_back(std::move(registry_provider));
#else
    provider_list->push_back(std::make_unique<ExternalProviderImpl>(
        service,
        new ExternalPrefLoader(chrome::DIR_EXTERNAL_EXTENSIONS,
                               check_admin_permissions_on_mac, nullptr),
        profile, Manifest::EXTERNAL_PREF, Manifest::EXTERNAL_PREF_DOWNLOAD,
        bundled_extension_creation_flags));

    // Define a per-user source of external extensions.
#if defined(OS_MACOSX) || (defined(OS_LINUX) && defined(CHROMIUM_BUILD))
    provider_list->push_back(std::make_unique<ExternalProviderImpl>(
        service,
        new ExternalPrefLoader(chrome::DIR_USER_EXTERNAL_EXTENSIONS,
                               ExternalPrefLoader::NONE, nullptr),
        profile, Manifest::EXTERNAL_PREF, Manifest::EXTERNAL_PREF_DOWNLOAD,
        Extension::NO_FLAGS));
#endif
#endif

#if !defined(OS_CHROMEOS)
    // The default apps are installed as INTERNAL but use the external
    // extension installer codeflow.
    provider_list->push_back(std::make_unique<default_apps::Provider>(
        profile, service,
        new ExternalPrefLoader(chrome::DIR_DEFAULT_APPS,
                               ExternalPrefLoader::NONE, nullptr),
        Manifest::INTERNAL, Manifest::INTERNAL,
        Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT));
#endif

    std::unique_ptr<ExternalProviderImpl> drive_migration_provider(
        new ExternalProviderImpl(
            service,
            new ExtensionMigrator(profile, extension_misc::kDriveHostedAppId,
                                  extension_misc::kDriveExtensionId),
            profile, Manifest::EXTERNAL_PREF, Manifest::EXTERNAL_PREF_DOWNLOAD,
            Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT));
    drive_migration_provider->set_auto_acknowledge(true);
    provider_list->push_back(std::move(drive_migration_provider));
  }

  provider_list->push_back(std::make_unique<ExternalProviderImpl>(
      service, new ExternalComponentLoader(profile), profile,
      Manifest::INVALID_LOCATION, Manifest::EXTERNAL_COMPONENT,
      Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT));
}

}  // namespace extensions
