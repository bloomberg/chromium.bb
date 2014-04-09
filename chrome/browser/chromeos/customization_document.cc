// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace chromeos {
namespace {

  // Manifest attributes names.
const char kVersionAttr[] = "version";
const char kDefaultAttr[] = "default";
const char kInitialLocaleAttr[] = "initial_locale";
const char kInitialTimezoneAttr[] = "initial_timezone";
const char kKeyboardLayoutAttr[] = "keyboard_layout";
const char kHwidMapAttr[] = "hwid_map";
const char kHwidMaskAttr[] = "hwid_mask";
const char kSetupContentAttr[] = "setup_content";
const char kEulaPageAttr[] = "eula_page";
const char kDefaultWallpaperAttr[] = "default_wallpaper";
const char kDefaultAppsAttr[] = "default_apps";
const char kLocalizedContent[] = "localized_content";
const char kDefaultAppsFolderName[] = "default_apps_folder_name";

const char kAcceptedManifestVersion[] = "1.0";

// Path to OEM partner startup customization manifest.
const char kStartupCustomizationManifestPath[] =
    "/opt/oem/etc/startup_manifest.json";

// Name of local state option that tracks if services customization has been
// applied.
const char kServicesCustomizationAppliedPref[] = "ServicesCustomizationApplied";

// Maximum number of retries to fetch file if network is not available.
const int kMaxFetchRetries = 3;

// Delay between file fetch retries if network is not available.
const int kRetriesDelayInSec = 2;

// Name of profile option that tracks cached version of service customization.
const char kServicesCustomizationKey[] = "customization.manifest_cache";

// Empty customization document that doesn't customize anything.
const char kEmptyServicesCustomizationManifest[] = "{ \"version\": \"1.0\" }";

// Global overrider for ServicesCustomizationDocument for tests.
ServicesCustomizationDocument* g_test_services_customization_document = NULL;

// Services customization document load results reported via the
// "ServicesCustomization.LoadResult" histogram.
// It is append-only enum due to use in a histogram!
enum HistogramServicesCustomizationLoadResult {
  HISTOGRAM_LOAD_RESULT_SUCCESS = 0,
  HISTOGRAM_LOAD_RESULT_FILE_NOT_FOUND = 1,
  HISTOGRAM_LOAD_RESULT_PARSING_ERROR = 2,
  HISTOGRAM_LOAD_RESULT_RETRIES_FAIL = 3,
  HISTOGRAM_LOAD_RESULT_MAX_VALUE = 4
};

void LogManifestLoadResult(HistogramServicesCustomizationLoadResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServicesCustomization.LoadResult",
                            result,
                            HISTOGRAM_LOAD_RESULT_MAX_VALUE);
}

std::string GetLocaleSpecificStringImpl(
    const base::DictionaryValue* root,
    const std::string& locale,
    const std::string& dictionary_name,
    const std::string& entry_name) {
  const base::DictionaryValue* dictionary_content = NULL;
  if (!root || !root->GetDictionary(dictionary_name, &dictionary_content))
    return std::string();

  const base::DictionaryValue* locale_dictionary = NULL;
  if (dictionary_content->GetDictionary(locale, &locale_dictionary)) {
    std::string result;
    if (locale_dictionary->GetString(entry_name, &result))
      return result;
  }

  const base::DictionaryValue* default_dictionary = NULL;
  if (dictionary_content->GetDictionary(kDefaultAttr, &default_dictionary)) {
    std::string result;
    if (default_dictionary->GetString(entry_name, &result))
      return result;
  }

  return std::string();
}

}  // anonymous namespace

// Template URL where to fetch OEM services customization manifest from.
const char ServicesCustomizationDocument::kManifestUrl[] =
    "https://ssl.gstatic.com/chrome/chromeos-customization/%s.json";

// A custom extensions::ExternalLoader that the ServicesCustomizationDocument
// creates and uses to publish OEM default apps to the extensions system.
class ServicesCustomizationExternalLoader
    : public extensions::ExternalLoader,
      public base::SupportsWeakPtr<ServicesCustomizationExternalLoader> {
 public:
  explicit ServicesCustomizationExternalLoader(Profile* profile)
      : is_apps_set_(false), profile_(profile) {}

  Profile* profile() { return profile_; }

  // Used by the ServicesCustomizationDocument to update the current apps.
  void SetCurrentApps(scoped_ptr<base::DictionaryValue> prefs) {
    apps_.Swap(prefs.get());
    is_apps_set_ = true;
    StartLoading();
  }

  // Implementation of extensions::ExternalLoader:
  virtual void StartLoading() OVERRIDE {
    if (!is_apps_set_) {
      ServicesCustomizationDocument::GetInstance()->StartFetching();
      // No return here to call LoadFinished with empty list initially.
      // When manifest is fetched, it will be called again with real list.
      // It is safe to return empty list because this provider didn't install
      // any app yet so no app can be removed due to returning empty list.
    }

    prefs_.reset(apps_.DeepCopy());
    VLOG(1) << "ServicesCustomization extension loader publishing "
            << apps_.size() << " apps.";
    LoadFinished();
  }

 protected:
  virtual ~ServicesCustomizationExternalLoader() {}

 private:
  bool is_apps_set_;
  base::DictionaryValue apps_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ServicesCustomizationExternalLoader);
};

// CustomizationDocument implementation. ---------------------------------------

CustomizationDocument::CustomizationDocument(
    const std::string& accepted_version)
    : accepted_version_(accepted_version) {}

CustomizationDocument::~CustomizationDocument() {}

bool CustomizationDocument::LoadManifestFromFile(
    const base::FilePath& manifest_path) {
  std::string manifest;
  if (!base::ReadFileToString(manifest_path, &manifest))
    return false;
  return LoadManifestFromString(manifest);
}

bool CustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  int error_code = 0;
  std::string error;
  scoped_ptr<base::Value> root(base::JSONReader::ReadAndReturnError(manifest,
      base::JSON_ALLOW_TRAILING_COMMAS, &error_code, &error));
  if (error_code != base::JSONReader::JSON_NO_ERROR)
    LOG(ERROR) << error;
  DCHECK(root.get() != NULL);
  if (root.get() == NULL)
    return false;
  DCHECK(root->GetType() == base::Value::TYPE_DICTIONARY);
  if (root->GetType() == base::Value::TYPE_DICTIONARY) {
    root_.reset(static_cast<base::DictionaryValue*>(root.release()));
    std::string result;
    if (root_->GetString(kVersionAttr, &result) &&
        result == accepted_version_)
      return true;

    LOG(ERROR) << "Wrong customization manifest version";
    root_.reset(NULL);
  }
  return false;
}

std::string CustomizationDocument::GetLocaleSpecificString(
    const std::string& locale,
    const std::string& dictionary_name,
    const std::string& entry_name) const {
  return GetLocaleSpecificStringImpl(
      root_.get(), locale, dictionary_name, entry_name);
}

// StartupCustomizationDocument implementation. --------------------------------

StartupCustomizationDocument::StartupCustomizationDocument()
    : CustomizationDocument(kAcceptedManifestVersion) {
  {
    // Loading manifest causes us to do blocking IO on UI thread.
    // Temporarily allow it until we fix http://crosbug.com/11103
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    LoadManifestFromFile(base::FilePath(kStartupCustomizationManifestPath));
  }
  Init(system::StatisticsProvider::GetInstance());
}

StartupCustomizationDocument::StartupCustomizationDocument(
    system::StatisticsProvider* statistics_provider,
    const std::string& manifest)
    : CustomizationDocument(kAcceptedManifestVersion) {
  LoadManifestFromString(manifest);
  Init(statistics_provider);
}

StartupCustomizationDocument::~StartupCustomizationDocument() {}

StartupCustomizationDocument* StartupCustomizationDocument::GetInstance() {
  return Singleton<StartupCustomizationDocument,
      DefaultSingletonTraits<StartupCustomizationDocument> >::get();
}

void StartupCustomizationDocument::Init(
    system::StatisticsProvider* statistics_provider) {
  if (IsReady()) {
    root_->GetString(kInitialLocaleAttr, &initial_locale_);
    root_->GetString(kInitialTimezoneAttr, &initial_timezone_);
    root_->GetString(kKeyboardLayoutAttr, &keyboard_layout_);

    std::string hwid;
    if (statistics_provider->GetMachineStatistic(
            system::kHardwareClassKey, &hwid)) {
      base::ListValue* hwid_list = NULL;
      if (root_->GetList(kHwidMapAttr, &hwid_list)) {
        for (size_t i = 0; i < hwid_list->GetSize(); ++i) {
          base::DictionaryValue* hwid_dictionary = NULL;
          std::string hwid_mask;
          if (hwid_list->GetDictionary(i, &hwid_dictionary) &&
              hwid_dictionary->GetString(kHwidMaskAttr, &hwid_mask)) {
            if (MatchPattern(hwid, hwid_mask)) {
              // If HWID for this machine matches some mask, use HWID specific
              // settings.
              std::string result;
              if (hwid_dictionary->GetString(kInitialLocaleAttr, &result))
                initial_locale_ = result;

              if (hwid_dictionary->GetString(kInitialTimezoneAttr, &result))
                initial_timezone_ = result;

              if (hwid_dictionary->GetString(kKeyboardLayoutAttr, &result))
                keyboard_layout_ = result;
            }
            // Don't break here to allow other entires to be applied if match.
          } else {
            LOG(ERROR) << "Syntax error in customization manifest";
          }
        }
      }
    } else {
      LOG(ERROR) << "HWID is missing in machine statistics";
    }
  }

  // If manifest doesn't exist still apply values from VPD.
  statistics_provider->GetMachineStatistic(kInitialLocaleAttr,
                                           &initial_locale_);
  statistics_provider->GetMachineStatistic(kInitialTimezoneAttr,
                                           &initial_timezone_);
  statistics_provider->GetMachineStatistic(kKeyboardLayoutAttr,
                                           &keyboard_layout_);
  configured_locales_.resize(0);
  base::SplitString(initial_locale_, ',', &configured_locales_);

  // Convert ICU locale to chrome ("en_US" to "en-US", etc.).
  std::for_each(configured_locales_.begin(),
                configured_locales_.end(),
                l10n_util::GetCanonicalLocale);

  // Let's always have configured_locales_.front() a valid entry.
  if (configured_locales_.size() == 0)
    configured_locales_.push_back(std::string());
}

const std::vector<std::string>&
StartupCustomizationDocument::configured_locales() const {
  return configured_locales_;
}

const std::string& StartupCustomizationDocument::initial_locale_default()
    const {
  DCHECK(configured_locales_.size() > 0);
  return configured_locales_.front();
}

std::string StartupCustomizationDocument::GetEULAPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(locale, kSetupContentAttr, kEulaPageAttr);
}

// ServicesCustomizationDocument implementation. -------------------------------

ServicesCustomizationDocument::ServicesCustomizationDocument()
    : CustomizationDocument(kAcceptedManifestVersion),
      num_retries_(0),
      fetch_started_(false),
      network_delay_(base::TimeDelta::FromMilliseconds(
          kDefaultNetworkRetryDelayMS)),
      weak_ptr_factory_(this) {
}

ServicesCustomizationDocument::ServicesCustomizationDocument(
    const std::string& manifest)
    : CustomizationDocument(kAcceptedManifestVersion),
      network_delay_(base::TimeDelta::FromMilliseconds(
          kDefaultNetworkRetryDelayMS)),
      weak_ptr_factory_(this) {
  LoadManifestFromString(manifest);
}

ServicesCustomizationDocument::~ServicesCustomizationDocument() {}

// static
ServicesCustomizationDocument* ServicesCustomizationDocument::GetInstance() {
  if (g_test_services_customization_document)
    return g_test_services_customization_document;

  return Singleton<ServicesCustomizationDocument,
      DefaultSingletonTraits<ServicesCustomizationDocument> >::get();
}

// static
void ServicesCustomizationDocument::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kServicesCustomizationAppliedPref, false);
}

// static
void ServicesCustomizationDocument::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      kServicesCustomizationKey,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
bool ServicesCustomizationDocument::WasOOBECustomizationApplied() {
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(kServicesCustomizationAppliedPref);
}

// static
void ServicesCustomizationDocument::SetApplied(bool val) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(kServicesCustomizationAppliedPref, val);
}

void ServicesCustomizationDocument::StartFetching() {
  if (IsReady() || fetch_started_)
    return;

  if (!url_.is_valid()) {
    std::string customization_id;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    if (provider->GetMachineStatistic(system::kCustomizationIdKey,
                                      &customization_id) &&
        !customization_id.empty()) {
      url_ = GURL(base::StringPrintf(
          kManifestUrl, StringToLowerASCII(customization_id).c_str()));
    }
  }

  if (url_.is_valid()) {
    fetch_started_ = true;
    if (url_.SchemeIsFile()) {
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          base::Bind(&ServicesCustomizationDocument::ReadFileInBackground,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::FilePath(url_.path())));
    } else {
      StartFileFetch();
    }
  }
}

// static
void ServicesCustomizationDocument::ReadFileInBackground(
    base::WeakPtr<ServicesCustomizationDocument> self,
    const base::FilePath& file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string manifest;
  if (!base::ReadFileToString(file, &manifest)) {
    manifest.clear();
    LOG(ERROR) << "Failed to load services customization manifest from: "
               << file.value();
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ServicesCustomizationDocument::OnManifesteRead,
                 self,
                 manifest));
}

void ServicesCustomizationDocument::OnManifesteRead(
    const std::string& manifest) {
  if (!manifest.empty())
    LoadManifestFromString(manifest);

  fetch_started_ = false;
}

void ServicesCustomizationDocument::StartFileFetch() {
  DelayNetworkCall(base::Bind(&ServicesCustomizationDocument::DoStartFileFetch,
                              weak_ptr_factory_.GetWeakPtr()),
      network_delay_);
}

void ServicesCustomizationDocument::DoStartFileFetch() {
  url_fetcher_.reset(net::URLFetcher::Create(
      url_, net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(g_browser_process->system_request_context());
  url_fetcher_->AddExtraRequestHeader("Accept: application/json");
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
}

bool ServicesCustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  if (CustomizationDocument::LoadManifestFromString(manifest)) {
    LogManifestLoadResult(HISTOGRAM_LOAD_RESULT_SUCCESS);
    OnManifestLoaded();
    return true;
  }

  LogManifestLoadResult(HISTOGRAM_LOAD_RESULT_PARSING_ERROR);
  return false;
}

void ServicesCustomizationDocument::OnManifestLoaded() {
  if (!ServicesCustomizationDocument::WasOOBECustomizationApplied())
    ApplyOOBECustomization();

  scoped_ptr<base::DictionaryValue> prefs =
      GetDefaultAppsInProviderFormat(*root_);
  for (ExternalLoaders::iterator it = external_loaders_.begin();
       it != external_loaders_.end(); ++it) {
    if (*it) {
      UpdateCachedManifest((*it)->profile());
      (*it)->SetCurrentApps(
          scoped_ptr<base::DictionaryValue>(prefs->DeepCopy()));
      SetOemFolderName((*it)->profile(), *root_);
    }
  }
}

void ServicesCustomizationDocument::OnURLFetchComplete(
    const net::URLFetcher* source) {
  std::string mime_type;
  std::string data;
  if (source->GetStatus().is_success() &&
      source->GetResponseCode() == net::HTTP_OK &&
      source->GetResponseHeaders()->GetMimeType(&mime_type) &&
      mime_type == "application/json" &&
      source->GetResponseAsString(&data)) {
    LoadManifestFromString(data);
  } else if (source->GetResponseCode() == net::HTTP_NOT_FOUND) {
    LOG(ERROR) << "Customization manifest is missing on server: "
               << source->GetURL().spec();
    OnCustomizationNotFound();
  } else {
    if (num_retries_ < kMaxFetchRetries) {
      num_retries_++;
      content::BrowserThread::PostDelayedTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ServicesCustomizationDocument::StartFileFetch,
                     weak_ptr_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(kRetriesDelayInSec));
      return;
    }
    // This doesn't stop fetching manifest on next restart.
    LOG(ERROR) << "URL fetch for services customization failed:"
               << " response code = " << source->GetResponseCode()
               << " URL = " << source->GetURL().spec();

    LogManifestLoadResult(HISTOGRAM_LOAD_RESULT_RETRIES_FAIL);
  }
  fetch_started_ = false;
}

bool ServicesCustomizationDocument::ApplyOOBECustomization() {
  // TODO(dpolukhin): apply default wallpaper, crbug.com/348136.
  SetApplied(true);
  return true;
}

GURL ServicesCustomizationDocument::GetDefaultWallpaperUrl() const {
  if (!IsReady())
    return GURL();

  std::string url;
  root_->GetString(kDefaultWallpaperAttr, &url);
  return GURL(url);
}

bool ServicesCustomizationDocument::GetDefaultApps(
    std::vector<std::string>* ids) const {
  ids->clear();
  if (!IsReady())
    return false;

  base::ListValue* apps_list = NULL;
  if (!root_->GetList(kDefaultAppsAttr, &apps_list))
    return false;

  for (size_t i = 0; i < apps_list->GetSize(); ++i) {
    std::string app_id;
    if (apps_list->GetString(i, &app_id)) {
      ids->push_back(app_id);
    } else {
      LOG(ERROR) << "Wrong format of default application list";
      return false;
    }
  }

  return true;
}

std::string ServicesCustomizationDocument::GetOemAppsFolderName(
    const std::string& locale) const {
  if (!IsReady())
    return std::string();

  return GetOemAppsFolderNameImpl(locale, *root_);
}

scoped_ptr<base::DictionaryValue>
ServicesCustomizationDocument::GetDefaultAppsInProviderFormat(
    const base::DictionaryValue& root) {
  scoped_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
  const base::ListValue* apps_list = NULL;
  if (root.GetList(kDefaultAppsAttr, &apps_list)) {
    for (size_t i = 0; i < apps_list->GetSize(); ++i) {
      std::string app_id;
      if (apps_list->GetString(i, &app_id)) {
        base::DictionaryValue* entry = new base::DictionaryValue;
        entry->SetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                         extension_urls::GetWebstoreUpdateUrl().spec());
        prefs->Set(app_id, entry);
      } else {
        LOG(ERROR) << "Wrong format of default application list";
        prefs->Clear();
        break;
      }
    }
  }

  return prefs.Pass();
}

void ServicesCustomizationDocument::UpdateCachedManifest(Profile* profile) {
  profile->GetPrefs()->Set(kServicesCustomizationKey, *root_);
}

extensions::ExternalLoader* ServicesCustomizationDocument::CreateExternalLoader(
    Profile* profile) {
  ServicesCustomizationExternalLoader* loader =
      new ServicesCustomizationExternalLoader(profile);
  external_loaders_.push_back(loader->AsWeakPtr());

  if (IsReady()) {
    UpdateCachedManifest(profile);
    loader->SetCurrentApps(GetDefaultAppsInProviderFormat(*root_));
    SetOemFolderName(profile, *root_);
  } else {
    const base::DictionaryValue* root =
        profile->GetPrefs()->GetDictionary(kServicesCustomizationKey);
    std::string version;
    if (root && root->GetString(kVersionAttr, &version)) {
      // If version exists, profile has cached version of customization.
      loader->SetCurrentApps(GetDefaultAppsInProviderFormat(*root));
      SetOemFolderName(profile, *root);
    } else {
      // StartFetching will be called from ServicesCustomizationExternalLoader
      // when StartLoading is called. We can't initiate manifest fetch here
      // because caller may never call StartLoading for the provider.
    }
  }

  return loader;
}

void ServicesCustomizationDocument::OnCustomizationNotFound() {
  LogManifestLoadResult(HISTOGRAM_LOAD_RESULT_FILE_NOT_FOUND);
  LoadManifestFromString(kEmptyServicesCustomizationManifest);
}

void ServicesCustomizationDocument::SetOemFolderName(
    Profile* profile,
    const base::DictionaryValue& root) {
  std::string locale = g_browser_process->GetApplicationLocale();
  std::string name = GetOemAppsFolderNameImpl(locale, root);
  if (!name.empty()) {
    app_list::AppListSyncableService* service =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile);
    if (!service) {
      LOG(WARNING) << "AppListSyncableService is not ready for setting OEM "
                      "folder name";
      return;
    }
    service->SetOemFolderName(name);
  }
}

std::string ServicesCustomizationDocument::GetOemAppsFolderNameImpl(
    const std::string& locale,
    const base::DictionaryValue& root) const {
  return GetLocaleSpecificStringImpl(
      &root, locale, kLocalizedContent, kDefaultAppsFolderName);
}

// static
void ServicesCustomizationDocument::InitializeForTesting() {
  g_test_services_customization_document = new ServicesCustomizationDocument;
  g_test_services_customization_document->network_delay_ = base::TimeDelta();
}

// static
void ServicesCustomizationDocument::ShutdownForTesting() {
  delete g_test_services_customization_document;
  g_test_services_customization_document = NULL;
}

}  // namespace chromeos
