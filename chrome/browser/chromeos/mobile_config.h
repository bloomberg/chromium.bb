// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MOBILE_CONFIG_H_
#define CHROME_BROWSER_CHROMEOS_MOBILE_CONFIG_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/time.h"
#include "chrome/browser/chromeos/customization_document.h"

class FilePath;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

// Class that processes mobile (carrier) configuration.
// Confugration is defined as a JSON file - global and local one.
// First global configuration is loaded then local one if it exists.
// Notes on global/local configuration:
// 1. All global config data is inherited unless some carrier properties
//    are overidden or carrier deals are explicitly marked as excluded.
// 2. Local config could mark that all carrier deals should be excluded or
//    only specific carrier deals are excluded.
// 3. New ID mappings in local config are not supported.
// 4. If local config exists, at least trivial global config should exist too.
// 5. If any error occurs while parsing global/local config,
//    MobileConfig::IsReady() will return false.
class MobileConfig : public CustomizationDocument  {
 public:
  // Carrier deal.
  class CarrierDeal {
   public:
    explicit CarrierDeal(base::DictionaryValue* deal_dict);
    ~CarrierDeal();

    // Returns string with the specified |locale| and |id|.
    // If there's no version for |locale|, default one is returned.
    // If there's no string with specified |id|, empty string is returned.
    std::string GetLocalizedString(const std::string& locale,
                                   const std::string& id) const;

    const std::string& deal_id() const { return deal_id_; }
    const std::vector<std::string>& locales() const { return locales_; }
    const std::string& info_url() const { return info_url_; }
    int notification_count() const { return notification_count_; }
    base::Time expire_date() const { return expire_date_; }

   private:
    std::string deal_id_;
    std::vector<std::string> locales_;
    std::string info_url_;
    int notification_count_;
    base::Time expire_date_;
    base::DictionaryValue* localized_strings_;

    DISALLOW_COPY_AND_ASSIGN(CarrierDeal);
  };

  // Carrier config.
  class Carrier {
   public:
    Carrier(base::DictionaryValue* carrier_dict,
            const std::string& initial_locale);
    ~Carrier();

    const std::vector<std::string>& external_ids() { return external_ids_; }
    const std::string& top_up_url() const { return top_up_url_; }

    // Returns "default" carrier deal i.e. first deal defined or NULL
    // if there're no deals defined.
    const CarrierDeal* GetDefaultDeal() const;

    // Returns carrier deal by ID.
    const CarrierDeal* GetDeal(const std::string& deal_id) const;

    // Initializes carrier from supplied dictionary.
    // Multiple calls supported (i.e. second call for local config).
    void InitFromDictionary(base::DictionaryValue* carrier_dict,
                            const std::string& initial_locale);

    // Removes all carrier deals. Might be executed when local config is loaded.
    void RemoveDeals();

   private:
    // Maps deal id to deal instance.
    typedef std::map<std::string, CarrierDeal*> CarrierDeals;

    std::string top_up_url_;
    std::vector<std::string> external_ids_;

    CarrierDeals deals_;

    DISALLOW_COPY_AND_ASSIGN(Carrier);
  };

  // External carrier ID (ex. "Verizon (us)") mapping to internal carrier ID.
  typedef std::map<std::string, std::string> CarrierIdMap;

  // Internal carrier ID mapping to Carrier config.
  typedef std::map<std::string, Carrier*> Carriers;

  static MobileConfig* GetInstance();

  // Returns carrier by external ID.
  const MobileConfig::Carrier* GetCarrier(const std::string& carrier_id) const;

 protected:
  virtual bool LoadManifestFromString(const std::string& manifest) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(MobileConfigTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(MobileConfigTest, BadManifest);
  FRIEND_TEST_ALL_PREFIXES(MobileConfigTest, DealOtherLocale);
  FRIEND_TEST_ALL_PREFIXES(MobileConfigTest, OldDeal);
  FRIEND_TEST_ALL_PREFIXES(MobileConfigTest, LocalConfigNoDeals);
  FRIEND_TEST_ALL_PREFIXES(MobileConfigTest, LocalConfig);
  friend struct DefaultSingletonTraits<MobileConfig>;

  // C-tor for singleton construction.
  MobileConfig();

  // C-tor for test construction.
  MobileConfig(const std::string& config,
               const std::string& initial_locale);

  virtual ~MobileConfig();

  // Loads carrier configuration.
  void LoadConfig();

  // Processes global/local config.
  void ProcessConfig(const std::string& global_config,
                     const std::string& local_config);

  // Executes on FILE thread and reads config files to string.
  void ReadConfigInBackground(const FilePath& global_config_file,
                              const FilePath& local_config_file);

  // Maps external carrier ID to internal carrier ID.
  CarrierIdMap carrier_id_map_;

  // Carrier configuration (including carrier deals).
  Carriers carriers_;

  // Initial locale value.
  std::string initial_locale_;

  // Root value of the local config (if it exists).
  // Global config is stored in root_ of the base class.
  scoped_ptr<base::DictionaryValue> local_config_root_;

  DISALLOW_COPY_AND_ASSIGN(MobileConfig);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MOBILE_CONFIG_H_
