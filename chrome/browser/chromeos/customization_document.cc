// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

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
#include "chrome/browser/chromeos/system_access.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/browser/browser_thread.h"

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

const char kHwid[] = "hwid";

// Carrier deals attributes.
const char kCarrierDealsAttr[] = "carrier_deals";
const char kDealLocaleAttr[] = "deal_locale";
const char kTopUpURLAttr[] = "top_up_url";
const char kNotificationCountAttr[] = "notification_count";
const char kDealExpireDateAttr[] = "expire_date";
const char kLocalizedContentAttr[] = "localized_content";
const char kNotificationTextAttr[] = "notification_text";

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

DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::ServicesCustomizationDocument);

namespace chromeos {

// CustomizationDocument implementation. ---------------------------------------

bool CustomizationDocument::LoadManifestFromFile(
    const FilePath& manifest_path) {
  std::string manifest;
  if (!file_util::ReadFileToString(manifest_path, &manifest))
    return false;
  return LoadManifestFromString(manifest);
}

bool CustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  scoped_ptr<Value> root(base::JSONReader::Read(manifest, true));
  DCHECK(root.get() != NULL);
  if (root.get() == NULL)
    return false;
  DCHECK(root->GetType() == Value::TYPE_DICTIONARY);
  if (root->GetType() == Value::TYPE_DICTIONARY) {
    root_.reset(static_cast<DictionaryValue*>(root.release()));
    std::string result;
    if (root_->GetString(kVersionAttr, &result) &&
        result == kAcceptedManifestVersion)
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

StartupCustomizationDocument::StartupCustomizationDocument() {
  {
    // Loading manifest causes us to do blocking IO on UI thread.
    // Temporarily allow it until we fix http://crosbug.com/11103
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    LoadManifestFromFile(FilePath(kStartupCustomizationManifestPath));
  }
  Init(SystemAccess::GetInstance());
}

StartupCustomizationDocument::StartupCustomizationDocument(
    SystemAccess* system_access, const std::string& manifest) {
  LoadManifestFromString(manifest);
  Init(system_access);
}

StartupCustomizationDocument* StartupCustomizationDocument::GetInstance() {
  return Singleton<StartupCustomizationDocument,
      DefaultSingletonTraits<StartupCustomizationDocument> >::get();
}

void StartupCustomizationDocument::Init(SystemAccess* system_access) {
  if (!IsReady())
    return;

  root_->GetString(kInitialLocaleAttr, &initial_locale_);
  root_->GetString(kInitialTimezoneAttr, &initial_timezone_);
  root_->GetString(kKeyboardLayoutAttr, &keyboard_layout_);
  root_->GetString(kRegistrationUrlAttr, &registration_url_);

  std::string hwid;
  if (system_access->GetMachineStatistic(kHwid, &hwid)) {
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

  system_access->GetMachineStatistic(kInitialLocaleAttr, &initial_locale_);
  system_access->GetMachineStatistic(kInitialTimezoneAttr, &initial_timezone_);
  system_access->GetMachineStatistic(kKeyboardLayoutAttr, &keyboard_layout_);
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

ServicesCustomizationDocument::CarrierDeal::CarrierDeal(
    DictionaryValue* deal_dict)
    : notification_count(0),
      localized_strings(NULL) {
  deal_dict->GetString(kDealLocaleAttr, &deal_locale);
  deal_dict->GetString(kTopUpURLAttr, &top_up_url);
  deal_dict->GetInteger(kNotificationCountAttr, &notification_count);
  std::string date_string;
  if (deal_dict->GetString(kDealExpireDateAttr, &date_string)) {
    if (!base::Time::FromString(ASCIIToWide(date_string).c_str(), &expire_date))
      LOG(ERROR) << "Error parsing deal_expire_date: " << date_string;
  }
  deal_dict->GetDictionary(kLocalizedContentAttr, &localized_strings);
}

std::string ServicesCustomizationDocument::CarrierDeal::GetLocalizedString(
    const std::string& locale, const std::string& id) const {
  std::string result;
  if (localized_strings) {
    DictionaryValue* locale_dict = NULL;
    if (localized_strings->GetDictionary(locale, &locale_dict) &&
        locale_dict->GetString(id, &result)) {
      return result;
    } else if (localized_strings->GetDictionary(kDefaultAttr, &locale_dict) &&
               locale_dict->GetString(id, &result)) {
      return result;
    }
  }
  return result;
}

ServicesCustomizationDocument::ServicesCustomizationDocument()
  : url_(kServicesCustomizationManifestUrl),
    initial_locale_(WizardController::GetInitialLocale()) {
}

ServicesCustomizationDocument::ServicesCustomizationDocument(
    const std::string& manifest, const std::string& initial_locale)
    : initial_locale_(initial_locale) {
  LoadManifestFromString(manifest);
}

// static
ServicesCustomizationDocument* ServicesCustomizationDocument::GetInstance() {
  return Singleton<ServicesCustomizationDocument,
      DefaultSingletonTraits<ServicesCustomizationDocument> >::get();
}

// static
void ServicesCustomizationDocument::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterBooleanPref(kServicesCustomizationAppliedPref, false);
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
        NewRunnableMethod(this,
            &ServicesCustomizationDocument::ReadFileInBackground,
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
        NewRunnableMethod(
            this,
            &ServicesCustomizationDocument::LoadManifestFromString,
            manifest));
  } else {
    VLOG(1) << "Failed to load services customization manifest from: "
            << file.value();
  }
}

void ServicesCustomizationDocument::StartFileFetch() {
  DCHECK(url_.is_valid());
  url_fetcher_.reset(new URLFetcher(url_, URLFetcher::GET, this));
  url_fetcher_->set_request_context(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  url_fetcher_->Start();
}

void ServicesCustomizationDocument::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  if (response_code == 200) {
    LoadManifestFromString(data);
  } else {
    NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
    if (!network->Connected() && num_retries_ < kMaxFetchRetries) {
      num_retries_++;
      retry_timer_.Start(base::TimeDelta::FromSeconds(kRetriesDelayInSec),
                         this, &ServicesCustomizationDocument::StartFileFetch);
      return;
    }
    LOG(ERROR) << "URL fetch for services customization failed:"
               << " response code = " << response_code
               << " URL = " << url.spec();
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

const ServicesCustomizationDocument::CarrierDeal*
ServicesCustomizationDocument::GetCarrierDeal(const std::string& carrier_id,
                                              bool check_restrictions) const {
  CarrierDeals::const_iterator iter = carrier_deals_.find(carrier_id);
  if (iter != carrier_deals_.end()) {
    CarrierDeal* deal = iter->second;
    if (check_restrictions) {
      // Deal locale has to match initial_locale (= launch country).
      if (initial_locale_ != deal->deal_locale)
        return NULL;
      // Make sure that deal is still active,
      // i.e. if deal expire date is defined, check it.
      if (!deal->expire_date.is_null() &&
          deal->expire_date <= base::Time::Now()) {
        return NULL;
      }
    }
    return deal;
  } else {
    return NULL;
  }
}

bool ServicesCustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  if (!CustomizationDocument::LoadManifestFromString(manifest))
    return false;

  DictionaryValue* carriers = NULL;
  if (root_.get() && root_->GetDictionary(kCarrierDealsAttr, &carriers)) {
    for (DictionaryValue::key_iterator iter = carriers->begin_keys();
         iter != carriers->end_keys(); ++iter) {
     DictionaryValue* carrier_deal = NULL;
     if (carriers->GetDictionary(*iter, &carrier_deal)) {
       carrier_deals_[*iter] = new CarrierDeal(carrier_deal);
     }
   }
  }
  return true;
}

}  // namespace chromeos
