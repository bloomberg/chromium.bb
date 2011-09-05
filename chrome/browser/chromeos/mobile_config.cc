// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mobile_config.h"

#include <algorithm>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "content/browser/browser_thread.h"

namespace {

// Config attributes names.
const char kVersionAttr[] = "version";
const char kAcceptedConfigVersion[] = "1.0";
const char kDefaultAttr[] = "default";

// Carrier config attributes.
const char kCarriersAttr[] = "carriers";
const char kCarrierIdsAttr[] = "ids";
const char kCarrierIdAttr[] = "id";
const char kTopUpURLAttr[] = "top_up_url";
const char kDealsAttr[] = "deals";

// Carrier deal attributes.
const char kDealIdAttr[] = "deal_id";
const char kDealLocalesAttr[] = "locales";

const char kInfoURLAttr[] = "info_url";
const char kNotificationCountAttr[] = "notification_count";
const char kDealExpireDateAttr[] = "expire_date";
const char kLocalizedContentAttr[] = "localized_content";
const char kNotificationTextAttr[] = "notification_text";

// Location of the global carrier config.
const char kGlobalCarrierConfigPath[] =
    "/usr/share/chromeos-assets/mobile/carrier_config.json";

}  // anonymous namespace

DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::MobileConfig);

namespace chromeos {

// MobileConfig::CarrierDeal implementation. -----------------------------------

MobileConfig::CarrierDeal::CarrierDeal(DictionaryValue* deal_dict)
    : notification_count_(0),
      localized_strings_(NULL) {
  deal_dict->GetString(kDealIdAttr, &deal_id_);

  // Extract list of deal locales.
  ListValue* locale_list = NULL;
  if (deal_dict->GetList(kDealLocalesAttr, &locale_list)) {
    for (size_t i = 0; i < locale_list->GetSize(); ++i) {
      std::string locale;
      if (locale_list->GetString(i, &locale))
        locales_.push_back(locale);
    }
  }

  deal_dict->GetString(kInfoURLAttr, &info_url_);
  deal_dict->GetInteger(kNotificationCountAttr, &notification_count_);
  std::string date_string;
  if (deal_dict->GetString(kDealExpireDateAttr, &date_string)) {
    if (!base::Time::FromString(date_string.c_str(), &expire_date_))
      LOG(ERROR) << "Error parsing deal_expire_date: " << date_string;
  }
  deal_dict->GetDictionary(kLocalizedContentAttr, &localized_strings_);
}

MobileConfig::CarrierDeal::~CarrierDeal() {
}

std::string MobileConfig::CarrierDeal::GetLocalizedString(
    const std::string& locale, const std::string& id) const {
  std::string result;
  if (localized_strings_) {
    DictionaryValue* locale_dict = NULL;
    if (localized_strings_->GetDictionary(locale, &locale_dict) &&
        locale_dict->GetString(id, &result)) {
      return result;
    } else if (localized_strings_->GetDictionary(kDefaultAttr, &locale_dict) &&
               locale_dict->GetString(id, &result)) {
      return result;
    }
  }
  return result;
}

// MobileConfig::Carrier implementation. ---------------------------------------

MobileConfig::Carrier::Carrier(DictionaryValue* carrier_dict,
                               const std::string& initial_locale) {
  carrier_dict->GetString(kTopUpURLAttr, &top_up_url_);

  // Extract list of external IDs for this carrier.
  ListValue* id_list = NULL;
  if (carrier_dict->GetList(kCarrierIdsAttr, &id_list)) {
    for (size_t i = 0; i < id_list->GetSize(); ++i) {
      DictionaryValue* id_dict = NULL;
      std::string external_id;
      if (id_list->GetDictionary(i, &id_dict) &&
          id_dict->GetString(kCarrierIdAttr, &external_id)) {
        external_ids_.push_back(external_id);
      }
    }
  }

  // Extract list of deals for this carrier.
  ListValue* deals_list = NULL;
  if (carrier_dict->GetList(kDealsAttr, &deals_list)) {
    for (size_t i = 0; i < deals_list->GetSize(); ++i) {
      DictionaryValue* deal_dict = NULL;
      if (deals_list->GetDictionary(i, &deal_dict)) {
        scoped_ptr<CarrierDeal> deal(new CarrierDeal(deal_dict));
        // Filter out deals by initial_locale right away.
        std::vector<std::string>::const_iterator iter =
            std::find(deal->locales().begin(),
                      deal->locales().end(),
                      initial_locale);
        if (iter != deal->locales().end())
          deals_[deal->deal_id()] = deal.release();
      }
    }
  }
}

MobileConfig::Carrier::~Carrier() {
  STLDeleteValues(&deals_);
}

const MobileConfig::CarrierDeal* MobileConfig::Carrier::GetDefaultDeal() const {
  // TODO(nkostylev): Use carrier "default_deal_id" attribute.
  CarrierDeals::const_iterator iter = deals_.begin();
  if (iter != deals_.end())
    return GetDeal((*iter).first);
  else
    return NULL;
}

const MobileConfig::CarrierDeal* MobileConfig::Carrier::GetDeal(
    const std::string& deal_id) const {
  CarrierDeals::const_iterator iter = deals_.find(deal_id);
  if (iter != deals_.end()) {
    CarrierDeal* deal = iter->second;
    // Make sure that deal is still active,
    // i.e. if deal expire date is defined, check it.
    if (!deal->expire_date().is_null() &&
        deal->expire_date() <= base::Time::Now()) {
      return NULL;
    }
    return deal;
  } else {
    return NULL;
  }
}

// MobileConfig implementation. -------------------------------

MobileConfig::MobileConfig()
    : CustomizationDocument(kAcceptedConfigVersion),
      initial_locale_(WizardController::GetInitialLocale()) {
  LoadConfig();
}

MobileConfig::MobileConfig(const std::string& config,
                           const std::string& initial_locale)
    : CustomizationDocument(kAcceptedConfigVersion),
      initial_locale_(initial_locale) {
  LoadManifestFromString(config);
}

MobileConfig::~MobileConfig() {
  STLDeleteValues(&carriers_);
}

// static
MobileConfig* MobileConfig::GetInstance() {
  return Singleton<MobileConfig,
      DefaultSingletonTraits<MobileConfig> >::get();
}

void MobileConfig::LoadConfig() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &MobileConfig::ReadFileInBackground,
          FilePath(kGlobalCarrierConfigPath)));
}

void MobileConfig::ReadFileInBackground(const FilePath& file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string config;
  if (file_util::ReadFileToString(file, &config)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this,
            &MobileConfig::LoadManifestFromString,
            config));
  } else {
    VLOG(1) << "Failed to load mobile config from: "
            << file.value();
  }
}

const MobileConfig::Carrier* MobileConfig::GetCarrier(
    const std::string& carrier_id) const {
  CarrierIdMap::const_iterator id_iter = carrier_id_map_.find(carrier_id);
  std::string internal_id;
  if (id_iter != carrier_id_map_.end())
    internal_id = id_iter->second;
  else
    return NULL;
  Carriers::const_iterator iter = carriers_. find(internal_id);
  if (iter != carriers_.end())
    return iter->second;
  else
    return NULL;
}

bool MobileConfig::LoadManifestFromString(const std::string& manifest) {
  if (!CustomizationDocument::LoadManifestFromString(manifest))
    return false;

  // Other parts are optional.
  DictionaryValue* carriers = NULL;
  if (root_.get() && root_->GetDictionary(kCarriersAttr, &carriers)) {
    for (DictionaryValue::key_iterator iter = carriers->begin_keys();
         iter != carriers->end_keys(); ++iter) {
      DictionaryValue* carrier_dict = NULL;
      if (carriers->GetDictionary(*iter, &carrier_dict)) {
        const std::string& internal_id = *iter;
        Carrier* carrier = new Carrier(carrier_dict, initial_locale_);
        if (!carrier->external_ids().empty()) {
          // Map all external IDs to a single internal one.
          for (std::vector<std::string>::const_iterator
               i = carrier->external_ids().begin();
               i != carrier->external_ids().end(); ++i) {
            carrier_id_map_[*i] = internal_id;
          }
        } else {
          // Trivial case - using same ID for external/internal one.
          carrier_id_map_[internal_id] = internal_id;
        }
        carriers_[internal_id] = carrier;
      }
    }
  }
  return true;
}

}  // namespace chromeos
