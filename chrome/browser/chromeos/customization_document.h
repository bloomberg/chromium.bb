// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#define CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#pragma once

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/timer.h"
#include "base/values.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class FilePath;
class ListValue;
class PrefService;

namespace base {
  class Time;
}

namespace chromeos {

class SystemAccess;

// Base class for OEM customization document classes.
class CustomizationDocument {
 public:
  virtual ~CustomizationDocument() {}

  // Return true if the document was successfully fetched and parsed.
  bool IsReady() const { return root_.get(); }

 protected:
  CustomizationDocument() {}

  virtual bool LoadManifestFromFile(const FilePath& manifest_path);
  virtual bool LoadManifestFromString(const std::string& manifest);

  std::string GetLocaleSpecificString(const std::string& locale,
                                      const std::string& dictionary_name,
                                      const std::string& entry_name) const;

  scoped_ptr<DictionaryValue> root_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomizationDocument);
};

// OEM startup customization document class.
// Now StartupCustomizationDocument is loaded in c-tor so just after create it
// may be ready or not (if manifest is missing or corrupted) and this state
// won't be changed later (i.e. IsReady() always return the same value).
class StartupCustomizationDocument : public CustomizationDocument {
 public:
  static StartupCustomizationDocument* GetInstance();

  const std::string& initial_locale() const { return initial_locale_; }
  const std::string& initial_timezone() const { return initial_timezone_; }
  const std::string& keyboard_layout() const { return keyboard_layout_; }
  const std::string& registration_url() const { return registration_url_; }

  std::string GetHelpPage(const std::string& locale) const;
  std::string GetEULAPage(const std::string& locale) const;

 private:
  FRIEND_TEST(StartupCustomizationDocumentTest, Basic);
  FRIEND_TEST(StartupCustomizationDocumentTest, VPD);
  FRIEND_TEST(StartupCustomizationDocumentTest, BadManifest);
  friend struct DefaultSingletonTraits<StartupCustomizationDocument>;

  // C-tor for singleton construction.
  StartupCustomizationDocument();

  // C-tor for test construction.
  StartupCustomizationDocument(SystemAccess* system_access,
                               const std::string& manifest);

  void Init(SystemAccess* system_access);

  // If |attr| exists in machine stat, assign it to |value|.
  void InitFromMachineStatistic(const char* attr, std::string* value);

  std::string initial_locale_;
  std::string initial_timezone_;
  std::string keyboard_layout_;
  std::string registration_url_;

  DISALLOW_COPY_AND_ASSIGN(StartupCustomizationDocument);
};

// OEM services customization document class.
// ServicesCustomizationDocument is fetched from network or local file but on
// FILE thread therefore it may not be ready just after creation. Fetching of
// the manifest should be initiated outside this class by calling
// StartFetching() method. User of the file should check IsReady before use it.
class ServicesCustomizationDocument : public CustomizationDocument,
                                      private URLFetcher::Delegate {
 public:
  // OEM specific carrier deal.
  struct CarrierDeal {
    explicit CarrierDeal(DictionaryValue* deal_dict);

    // Returns string with the specified |locale| and |id|.
    // If there's no version for |locale|, default one is returned.
    // If there's no string with specified |id|, empty string is returned.
    std::string GetLocalizedString(const std::string& locale,
                                   const std::string& id) const;

    std::string deal_locale;
    std::string top_up_url;
    int notification_count;
    base::Time expire_date;
    DictionaryValue* localized_strings;
  };

  // Carrier ID (ex. "Verizon (us)") mapping to carrier deals.
  typedef std::map<std::string, CarrierDeal*> CarrierDeals;

  static ServicesCustomizationDocument* GetInstance();

  // Registers preferences.
  static void RegisterPrefs(PrefService* local_state);

  // Return true if the customization was applied. Customization is applied only
  // once per machine.
  static bool WasApplied();

  // Start fetching customization document.
  void StartFetching();

  // Apply customization and save in machine options that customization was
  // applied successfully. Return true if customization was applied.
  bool ApplyCustomization();

  std::string GetInitialStartPage(const std::string& locale) const;
  std::string GetSupportPage(const std::string& locale) const;

  // Returns carrier deal by specified |carrier_id|.
  // Also checks deal restrictions, such as deal locale (launch locale) and
  // deal expiration date if |check_restrictions| is true.
  const ServicesCustomizationDocument::CarrierDeal* GetCarrierDeal(
      const std::string& carrier_id, bool check_restrictions) const;

 protected:
  virtual bool LoadManifestFromString(const std::string& manifest) OVERRIDE;

 private:
  FRIEND_TEST(ServicesCustomizationDocumentTest, Basic);
  FRIEND_TEST(ServicesCustomizationDocumentTest, BadManifest);
  FRIEND_TEST(ServicesCustomizationDocumentTest, DealOtherLocale);
  FRIEND_TEST(ServicesCustomizationDocumentTest, NoDealRestrictions);
  FRIEND_TEST(ServicesCustomizationDocumentTest, OldDeal);
  friend struct DefaultSingletonTraits<ServicesCustomizationDocument>;

  // C-tor for singleton construction.
  ServicesCustomizationDocument();

  // C-tor for test construction.
  ServicesCustomizationDocument(const std::string& manifest,
                                const std::string& initial_locale);

  // Save applied state in machine settings.
  static void SetApplied(bool val);

  // Overriden from URLFetcher::Delegate:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Initiate file fetching.
  void StartFileFetch();

  // Executes on FILE thread and reads file to string.
  void ReadFileInBackground(const FilePath& file);

  // Services customization manifest URL.
  GURL url_;

  // URLFetcher instance.
  scoped_ptr<URLFetcher> url_fetcher_;

  // Timer to retry fetching file if network is not available.
  base::OneShotTimer<ServicesCustomizationDocument> retry_timer_;

  // How many times we already tried to fetch customization manifest file.
  int num_retries_;

  // Carrier-specific deals.
  CarrierDeals carrier_deals_;

  // Initial locale value.
  std::string initial_locale_;

  DISALLOW_COPY_AND_ASSIGN(ServicesCustomizationDocument);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
