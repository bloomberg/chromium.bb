// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_metrics_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
// Accessing the Device ID API here is a layering violation.
// TODO(bbudge) Move the API so it's usable here.
// http://crbug.com/276485
#include "chrome/browser/extensions/api/music_manager_private/device_id.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/prefs/synced_pref_change_registrar.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "crypto/hmac.h"
#include "grit/browser_resources.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const int kSessionStartupPrefValueMax = SessionStartupPref::kPrefValueMax;

#if defined(OS_ANDROID)
// An unregistered preference to fill in indices in kTrackedPrefs below for
// preferences that aren't defined on every platform. This is fine as the code
// below (e.g. CheckTrackedPreferences()) skips unregistered preferences and
// should thus never report any data about that index on the platforms where
// that preference is unimplemented.
const char kUnregisteredPreference[] = "_";
#endif

// These preferences must be kept in sync with the TrackedPreference enum in
// tools/metrics/histograms/histograms.xml. To add a new preference, append it
// to the array and add a corresponding value to the histogram enum.
const char* kTrackedPrefs[] = {
  prefs::kShowHomeButton,
  prefs::kHomePageIsNewTabPage,
  prefs::kHomePage,
  prefs::kRestoreOnStartup,
  prefs::kURLsToRestoreOnStartup,
  prefs::kExtensionsPref,
  prefs::kGoogleServicesLastUsername,
  prefs::kSearchProviderOverrides,
  prefs::kDefaultSearchProviderSearchURL,
  prefs::kDefaultSearchProviderKeyword,
  prefs::kDefaultSearchProviderName,
#if !defined(OS_ANDROID)
  prefs::kPinnedTabs,
#else
  kUnregisteredPreference,
#endif
  prefs::kExtensionKnownDisabled,
};

const size_t kSHA256DigestSize = 32;

}  // namespace

PrefMetricsService::PrefMetricsService(Profile* profile)
    : profile_(profile),
      prefs_(profile_->GetPrefs()),
      local_state_(g_browser_process->local_state()),
      profile_name_(profile_->GetPath().AsUTF8Unsafe()),
      tracked_pref_paths_(kTrackedPrefs),
      tracked_pref_path_count_(arraysize(kTrackedPrefs)),
      checked_tracked_prefs_(false),
      weak_factory_(this) {
  pref_hash_seed_ = ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_PREF_HASH_SEED_BIN).as_string();

  RecordLaunchPrefs();

  PrefServiceSyncable* prefs = PrefServiceSyncable::FromProfile(profile_);
  synced_pref_change_registrar_.reset(new SyncedPrefChangeRegistrar(prefs));

  RegisterSyncedPrefObservers();

  // The following code might cause callbacks into this instance before we exit
  // the constructor. This instance should be initialized at this point.
#if defined(OS_WIN) || defined(OS_MACOSX)
  // We need the machine id to compute pref value hashes. Fetch that, and then
  // call CheckTrackedPreferences in the callback.
  extensions::api::DeviceId::GetDeviceId(
      "PrefMetricsService",  // non-empty string to obfuscate the device id.
      Bind(&PrefMetricsService::GetDeviceIdCallback,
           weak_factory_.GetWeakPtr()));
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

// For unit testing only.
PrefMetricsService::PrefMetricsService(Profile* profile,
                                       PrefService* local_state,
                                       const std::string& device_id,
                                       const char** tracked_pref_paths,
                                       int tracked_pref_path_count)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      local_state_(local_state),
      profile_name_(profile_->GetPath().AsUTF8Unsafe()),
      pref_hash_seed_(kSHA256DigestSize, 0),
      device_id_(device_id),
      tracked_pref_paths_(tracked_pref_paths),
      tracked_pref_path_count_(tracked_pref_path_count),
      checked_tracked_prefs_(false),
      weak_factory_(this) {
  CheckTrackedPreferences();
}

PrefMetricsService::~PrefMetricsService() {
}

void PrefMetricsService::RecordLaunchPrefs() {
  bool show_home_button = prefs_->GetBoolean(prefs::kShowHomeButton);
  bool home_page_is_ntp = prefs_->GetBoolean(prefs::kHomePageIsNewTabPage);
  UMA_HISTOGRAM_BOOLEAN("Settings.ShowHomeButton", show_home_button);
  if (show_home_button) {
    UMA_HISTOGRAM_BOOLEAN("Settings.GivenShowHomeButton_HomePageIsNewTabPage",
                          home_page_is_ntp);
  }

  // For non-NTP homepages, see if the URL comes from the same TLD+1 as a known
  // search engine.  Note that this is only an approximation of search engine
  // use, due to both false negatives (pages that come from unknown TLD+1 X but
  // consist of a search box that sends to known TLD+1 Y) and false positives
  // (pages that share a TLD+1 with a known engine but aren't actually search
  // pages, e.g. plus.google.com).
  if (!home_page_is_ntp) {
    GURL homepage_url(prefs_->GetString(prefs::kHomePage));
    if (homepage_url.is_valid()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Settings.HomePageEngineType",
          TemplateURLPrepopulateData::GetEngineType(homepage_url),
          SEARCH_ENGINE_MAX);
    }
  }

  int restore_on_startup = prefs_->GetInteger(prefs::kRestoreOnStartup);
  UMA_HISTOGRAM_ENUMERATION("Settings.StartupPageLoadSettings",
                            restore_on_startup, kSessionStartupPrefValueMax);
  if (restore_on_startup == SessionStartupPref::kPrefValueURLs) {
    const ListValue* url_list = prefs_->GetList(prefs::kURLsToRestoreOnStartup);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Settings.StartupPageLoadURLs",
                                url_list->GetSize(), 1, 50, 20);
    // Similarly, check startup pages for known search engine TLD+1s.
    std::string url_text;
    for (size_t i = 0; i < url_list->GetSize(); ++i) {
      if (url_list->GetString(i, &url_text)) {
        GURL start_url(url_text);
        if (start_url.is_valid()) {
          UMA_HISTOGRAM_ENUMERATION(
              "Settings.StartupPageEngineTypes",
              TemplateURLPrepopulateData::GetEngineType(start_url),
              SEARCH_ENGINE_MAX);
        }
      }
    }
  }

#if !defined(OS_ANDROID)
  StartupTabs startup_tabs = PinnedTabCodec::ReadPinnedTabs(profile_);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Settings.PinnedTabs",
                              startup_tabs.size(), 1, 50, 20);
  for (size_t i = 0; i < startup_tabs.size(); ++i) {
    GURL start_url(startup_tabs.at(i).url);
    if (start_url.is_valid()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Settings.PinnedTabEngineTypes",
          TemplateURLPrepopulateData::GetEngineType(start_url),
          SEARCH_ENGINE_MAX);
    }
  }
#endif
}

// static
void PrefMetricsService::RegisterPrefs(PrefRegistrySimple* registry) {
  // Register the top level dictionary to map profile names to dictionaries of
  // tracked preferences.
  registry->RegisterDictionaryPref(prefs::kProfilePreferenceHashes);
}

void PrefMetricsService::RegisterSyncedPrefObservers() {
  LogHistogramValueCallback booleanHandler = base::Bind(
      &PrefMetricsService::LogBooleanPrefChange, base::Unretained(this));

  AddPrefObserver(prefs::kShowHomeButton, "ShowHomeButton", booleanHandler);
  AddPrefObserver(prefs::kHomePageIsNewTabPage, "HomePageIsNewTabPage",
                  booleanHandler);

  AddPrefObserver(prefs::kRestoreOnStartup, "StartupPageLoadSettings",
                  base::Bind(&PrefMetricsService::LogIntegerPrefChange,
                             base::Unretained(this),
                             kSessionStartupPrefValueMax));
}

void PrefMetricsService::AddPrefObserver(
    const std::string& path,
    const std::string& histogram_name_prefix,
    const LogHistogramValueCallback& callback) {
  synced_pref_change_registrar_->Add(path.c_str(),
      base::Bind(&PrefMetricsService::OnPrefChanged,
                 base::Unretained(this),
                 histogram_name_prefix, callback));
}

void PrefMetricsService::OnPrefChanged(
    const std::string& histogram_name_prefix,
    const LogHistogramValueCallback& callback,
    const std::string& path,
    bool from_sync) {
  PrefServiceSyncable* prefs = PrefServiceSyncable::FromProfile(profile_);
  const PrefService::Preference* pref = prefs->FindPreference(path.c_str());
  DCHECK(pref);
  std::string source_name(
      from_sync ? ".PulledFromSync" : ".PushedToSync");
  std::string histogram_name("Settings." + histogram_name_prefix + source_name);
  callback.Run(histogram_name, pref->GetValue());
};

void PrefMetricsService::LogBooleanPrefChange(const std::string& histogram_name,
                                              const Value* value) {
  bool boolean_value = false;
  if (!value->GetAsBoolean(&boolean_value))
    return;
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(boolean_value);
}

void PrefMetricsService::LogIntegerPrefChange(int boundary_value,
                                              const std::string& histogram_name,
                                              const Value* value) {
  int integer_value = 0;
  if (!value->GetAsInteger(&integer_value))
    return;
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name,
      1,
      boundary_value,
      boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(integer_value);
}

void PrefMetricsService::GetDeviceIdCallback(const std::string& device_id) {
  device_id_ = device_id;
  // On Aura, this seems to be called twice.
  if (!checked_tracked_prefs_)
    CheckTrackedPreferences();
}

// To detect changes to Preferences that happen outside of Chrome, we hash
// selected pref values and save them in local state. CheckTrackedPreferences
// compares the saved values to the values observed in the profile's prefs. A
// dictionary of dictionaries in local state holds the hashed values, grouped by
// profile. To make the system more resistant to spoofing, pref values are
// hashed with the pref path and the device id.
void PrefMetricsService::CheckTrackedPreferences() {
  DCHECK(!checked_tracked_prefs_);

  const base::DictionaryValue* pref_hash_dicts =
      local_state_->GetDictionary(prefs::kProfilePreferenceHashes);
  // Get the hashed prefs dictionary if it exists. If it doesn't, it will be
  // created if we set preference values below.
  const base::DictionaryValue* hashed_prefs = NULL;
  pref_hash_dicts->GetDictionaryWithoutPathExpansion(profile_name_,
                                                     &hashed_prefs);
  for (int i = 0; i < tracked_pref_path_count_; ++i) {
    // Skip prefs that haven't been registered.
    if (!prefs_->FindPreference(tracked_pref_paths_[i]))
      continue;

    const base::Value* value = prefs_->GetUserPrefValue(tracked_pref_paths_[i]);
    std::string last_hash;
    // First try to get the stored expected hash...
    if (hashed_prefs &&
        hashed_prefs->GetString(tracked_pref_paths_[i], &last_hash)) {
      // ... if we have one get the hash of the current value...
      const std::string value_hash =
          GetHashedPrefValue(tracked_pref_paths_[i], value,
                             HASHED_PREF_STYLE_NEW);
      // ... and check that it matches...
      if (value_hash == last_hash) {
        UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceUnchanged",
            i, tracked_pref_path_count_);
      } else {
        // ... if it doesn't: was the value simply cleared?
        if (!value) {
          UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceCleared",
              i, tracked_pref_path_count_);
        } else {
          // ... or does it match the old style hash?
          std::string old_style_hash =
              GetHashedPrefValue(tracked_pref_paths_[i], value,
                                 HASHED_PREF_STYLE_DEPRECATED);
          if (old_style_hash == last_hash) {
            UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceMigrated",
                i, tracked_pref_path_count_);
          } else {
            // ... or was it simply changed to something else?
            UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceChanged",
                i, tracked_pref_path_count_);
          }
        }

        // ... either way update the expected hash to match the new value.
        UpdateTrackedPreference(tracked_pref_paths_[i]);
      }
    } else {
      // Record that we haven't tracked this preference yet, or the hash in
      // local state was removed.
      UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferenceInitialized",
          i, tracked_pref_path_count_);
      UpdateTrackedPreference(tracked_pref_paths_[i]);
    }
  }

  checked_tracked_prefs_ = true;

  // Now that we've checked the incoming preferences, register for change
  // notifications, unless this is test code.
  // TODO(bbudge) Fix failing browser_tests and so we can remove this test.
  // Several tests fail when they shutdown before they can write local state.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
    InitializePrefObservers();
  }
}

void PrefMetricsService::UpdateTrackedPreference(const char* path) {
  const base::Value* value = prefs_->GetUserPrefValue(path);
  DictionaryPrefUpdate update(local_state_, prefs::kProfilePreferenceHashes);
  DictionaryValue* child_dictionary = NULL;

  // Get the dictionary corresponding to the profile name,
  // which may have a '.'
  if (!update->GetDictionaryWithoutPathExpansion(profile_name_,
                                                 &child_dictionary)) {
    child_dictionary = new DictionaryValue;
    update->SetWithoutPathExpansion(profile_name_, child_dictionary);
  }

  child_dictionary->SetString(path,
                              GetHashedPrefValue(path, value,
                                                 HASHED_PREF_STYLE_NEW));
}

std::string PrefMetricsService::GetHashedPrefValue(
    const char* path,
    const base::Value* value,
    HashedPrefStyle desired_style) {
  // Dictionary values may contain empty lists and sub-dictionaries. Make a
  // deep copy with those removed to make the hash more stable.
  const DictionaryValue* dict_value;
  scoped_ptr<DictionaryValue> canonical_dict_value;
  if (value &&
      value->GetAsDictionary(&dict_value)) {
    canonical_dict_value.reset(dict_value->DeepCopyWithoutEmptyChildren());
    value = canonical_dict_value.get();
  }

  std::string value_as_string;
  // If |value| is NULL, we will still build a unique hash based on |device_id_|
  // and |path| below.
  if (value) {
    JSONStringValueSerializer serializer(&value_as_string);
    serializer.Serialize(*value);
  }

  std::string string_to_hash;
  // TODO(gab): Remove this as the old style is phased out.
  if (desired_style == HASHED_PREF_STYLE_NEW) {
    string_to_hash.append(device_id_);
    string_to_hash.append(path);
  }
  string_to_hash.append(value_as_string);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  unsigned char digest[kSHA256DigestSize];
  if (!hmac.Init(pref_hash_seed_) ||
      !hmac.Sign(string_to_hash, digest, kSHA256DigestSize)) {
    NOTREACHED();
    return std::string();
  }

  return base::HexEncode(digest, kSHA256DigestSize);
}

void PrefMetricsService::InitializePrefObservers() {
  pref_registrar_.Init(prefs_);
  for (int i = 0; i < tracked_pref_path_count_; ++i) {
    // Skip prefs that haven't been registered.
    if (!prefs_->FindPreference(tracked_pref_paths_[i]))
      continue;

    pref_registrar_.Add(
        tracked_pref_paths_[i],
        base::Bind(&PrefMetricsService::UpdateTrackedPreference,
                   weak_factory_.GetWeakPtr(),
                   tracked_pref_paths_[i]));
  }
}

// static
PrefMetricsService::Factory* PrefMetricsService::Factory::GetInstance() {
  return Singleton<PrefMetricsService::Factory>::get();
}

// static
PrefMetricsService* PrefMetricsService::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<PrefMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrefMetricsService::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
        "PrefMetricsService",
        BrowserContextDependencyManager::GetInstance()) {
}

PrefMetricsService::Factory::~Factory() {
}

BrowserContextKeyedService*
PrefMetricsService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new PrefMetricsService(static_cast<Profile*>(profile));
}

bool PrefMetricsService::Factory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PrefMetricsService::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

content::BrowserContext* PrefMetricsService::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
