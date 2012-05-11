// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"

using content::BrowserThread;

// Manifest attributes names.

namespace {

const char kVersionAttr[] = "version";
const char kDefaultAttr[] = "default";
const char kInitialLocaleAttr[] = "initial_locale";
const char kInitialTimezoneAttr[] = "initial_timezone";
const char kKeyboardLayoutAttr[] = "keyboard_layout";
const char kRegistrationUrlAttr[] = "registration_url";
const char kHwidMapAttr[] = "hwid_map";
const char kHwidMaskAttr[] = "hwid_mask";
const char kSetupContentAttr[] = "setup_content";
const char kHelpPageAttr[] = "help_page";
const char kEulaPageAttr[] = "eula_page";
const char kAppContentAttr[] = "app_content";
const char kInitialStartPageAttr[] = "initial_start_page";
const char kSupportPageAttr[] = "support_page";

const char kAcceptedManifestVersion[] = "1.0";

const char kHardwareClass[] = "hardware_class";

// Path to OEM partner startup customization manifest.
const char kStartupCustomizationManifestPath[] =
    "/opt/oem/etc/startup_manifest.json";

// URL where to fetch OEM services customization manifest from.
const char kServicesCustomizationManifestUrl[] =
    "file:///opt/oem/etc/services_manifest.json";

// Name of local state option that tracks if services customization has been
// applied.
const char kServicesCustomizationAppliedPref[] = "ServicesCustomizationApplied";

// Maximum number of retries to fetch file if network is not available.
const int kMaxFetchRetries = 3;

// Delay between file fetch retries if network is not available.
const int kRetriesDelayInSec = 2;

}  // anonymous namespace

namespace chromeos {

// CustomizationDocument implementation. ---------------------------------------

CustomizationDocument::CustomizationDocument(
    const std::string& accepted_version)
    : accepted_version_(accepted_version) {}

CustomizationDocument::~CustomizationDocument() {}

bool CustomizationDocument::LoadManifestFromFile(
    const FilePath& manifest_path) {
  std::string manifest;
  if (!file_util::ReadFileToString(manifest_path, &manifest))
    return false;
  return LoadManifestFromString(manifest);
}

bool CustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  int error_code = 0;
  std::string error;
  scoped_ptr<Value> root(base::JSONReader::ReadAndReturnError(manifest,
      base::JSON_ALLOW_TRAILING_COMMAS, &error_code, &error));
  if (error_code != base::JSONReader::JSON_NO_ERROR)
    LOG(ERROR) << error;
  DCHECK(root.get() != NULL);
  if (root.get() == NULL)
    return false;
  DCHECK(root->GetType() == Value::TYPE_DICTIONARY);
  if (root->GetType() == Value::TYPE_DICTIONARY) {
    root_.reset(static_cast<DictionaryValue*>(root.release()));
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
  DictionaryValue* dictionary_content = NULL;
  if (!root_.get() ||
      !root_->GetDictionary(dictionary_name, &dictionary_content))
    return std::string();

  DictionaryValue* locale_dictionary = NULL;
  if (dictionary_content->GetDictionary(locale, &locale_dictionary)) {
    std::string result;
    if (locale_dictionary->GetString(entry_name, &result))
      return result;
  }

  DictionaryValue* default_dictionary = NULL;
  if (dictionary_content->GetDictionary(kDefaultAttr, &default_dictionary)) {
    std::string result;
    if (default_dictionary->GetString(entry_name, &result))
      return result;
  }

  return std::string();
}

// StartupCustomizationDocument implementation. --------------------------------

StartupCustomizationDocument::StartupCustomizationDocument()
    : CustomizationDocument(kAcceptedManifestVersion) {
  {
    // Loading manifest causes us to do blocking IO on UI thread.
    // Temporarily allow it until we fix http://crosbug.com/11103
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    LoadManifestFromFile(FilePath(kStartupCustomizationManifestPath));
  }
  Init(chromeos::system::StatisticsProvider::GetInstance());
}

StartupCustomizationDocument::StartupCustomizationDocument(
    chromeos::system::StatisticsProvider* statistics_provider,
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
    chromeos::system::StatisticsProvider* statistics_provider) {
  if (IsReady()) {
    root_->GetString(kInitialLocaleAttr, &initial_locale_);
    root_->GetString(kInitialTimezoneAttr, &initial_timezone_);
    root_->GetString(kKeyboardLayoutAttr, &keyboard_layout_);
    root_->GetString(kRegistrationUrlAttr, &registration_url_);

    std::string hwid;
    if (statistics_provider->GetMachineStatistic(kHardwareClass, &hwid)) {
      ListValue* hwid_list = NULL;
      if (root_->GetList(kHwidMapAttr, &hwid_list)) {
        for (size_t i = 0; i < hwid_list->GetSize(); ++i) {
          DictionaryValue* hwid_dictionary = NULL;
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
}

std::string StartupCustomizationDocument::GetHelpPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(locale, kSetupContentAttr, kHelpPageAttr);
}

std::string StartupCustomizationDocument::GetEULAPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(locale, kSetupContentAttr, kEulaPageAttr);
}

// ServicesCustomizationDocument implementation. -------------------------------

ServicesCustomizationDocument::ServicesCustomizationDocument()
    : CustomizationDocument(kAcceptedManifestVersion),
      url_(kServicesCustomizationManifestUrl) {
}

ServicesCustomizationDocument::ServicesCustomizationDocument(
    const std::string& manifest)
    : CustomizationDocument(kAcceptedManifestVersion) {
  LoadManifestFromString(manifest);
}

ServicesCustomizationDocument::~ServicesCustomizationDocument() {}

// static
ServicesCustomizationDocument* ServicesCustomizationDocument::GetInstance() {
  return Singleton<ServicesCustomizationDocument,
      DefaultSingletonTraits<ServicesCustomizationDocument> >::get();
}

// static
void ServicesCustomizationDocument::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterBooleanPref(kServicesCustomizationAppliedPref, false,
                                   PrefService::UNSYNCABLE_PREF);
}

// static
bool ServicesCustomizationDocument::WasApplied() {
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(kServicesCustomizationAppliedPref);
}

// static
void ServicesCustomizationDocument::SetApplied(bool val) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(kServicesCustomizationAppliedPref, val);
}

void ServicesCustomizationDocument::StartFetching() {
  if (url_.SchemeIsFile()) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&ServicesCustomizationDocument::ReadFileInBackground,
                   base::Unretained(this),  // this class is a singleton.
                   FilePath(url_.path())));
  } else {
    StartFileFetch();
  }
}

void ServicesCustomizationDocument::ReadFileInBackground(const FilePath& file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string manifest;
  if (file_util::ReadFileToString(file, &manifest)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(
           base::IgnoreResult(
               &ServicesCustomizationDocument::LoadManifestFromString),
           base::Unretained(this),  // this class is a singleton.
           manifest));
  } else {
    VLOG(1) << "Failed to load services customization manifest from: "
            << file.value();
  }
}

void ServicesCustomizationDocument::StartFileFetch() {
  DCHECK(url_.is_valid());
  url_fetcher_.reset(content::URLFetcher::Create(
      url_, content::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  url_fetcher_->Start();
}

void ServicesCustomizationDocument::OnURLFetchComplete(
    const net::URLFetcher* source) {
  if (source->GetResponseCode() == 200) {
    std::string data;
    source->GetResponseAsString(&data);
    LoadManifestFromString(data);
  } else {
    NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
    if (!network->Connected() && num_retries_ < kMaxFetchRetries) {
      num_retries_++;
      retry_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromSeconds(kRetriesDelayInSec),
                         this, &ServicesCustomizationDocument::StartFileFetch);
      return;
    }
    LOG(ERROR) << "URL fetch for services customization failed:"
               << " response code = " << source->GetResponseCode()
               << " URL = " << source->GetURL().spec();
  }
}

bool ServicesCustomizationDocument::ApplyCustomization() {
  // TODO(dpolukhin): apply customized apps, exts and support page.
  SetApplied(true);
  return true;
}

std::string ServicesCustomizationDocument::GetInitialStartPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(
      locale, kAppContentAttr, kInitialStartPageAttr);
}

std::string ServicesCustomizationDocument::GetSupportPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(
      locale, kAppContentAttr, kSupportPageAttr);
}

}  // namespace chromeos
