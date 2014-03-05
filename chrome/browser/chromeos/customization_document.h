// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#define CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class Profile;

namespace base {
class DictionaryValue;
class FilePath;
}

namespace extensions {
class ExternalLoader;
}

namespace net {
class URLFetcher;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This test is in global namespace so it must be declared here.
void Test__InitStartupCustomizationDocument(const std::string& manifest);

namespace chromeos {

class ServicesCustomizationExternalLoader;

namespace system {
class StatisticsProvider;
}  // system

// Base class for OEM customization document classes.
class CustomizationDocument {
 public:
  virtual ~CustomizationDocument();

  // Return true if the document was successfully fetched and parsed.
  bool IsReady() const { return root_.get(); }

 protected:
  explicit CustomizationDocument(const std::string& accepted_version);

  virtual bool LoadManifestFromFile(const base::FilePath& manifest_path);
  virtual bool LoadManifestFromString(const std::string& manifest);

  std::string GetLocaleSpecificString(const std::string& locale,
                                      const std::string& dictionary_name,
                                      const std::string& entry_name) const;

  scoped_ptr<base::DictionaryValue> root_;

  // Value of the "version" attribute that is supported.
  // Otherwise config is not loaded.
  std::string accepted_version_;

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

  std::string GetEULAPage(const std::string& locale) const;

  // These methods can be called even if !IsReady(), in this case VPD values
  // will be returned.
  //
  // Raw value of "initial_locale" like initial_locale="en-US,sv,da,fi,no" .
  const std::string& initial_locale() const { return initial_locale_; }

  // Vector of individual locale values.
  const std::vector<std::string>& configured_locales() const;

  // Default locale value (first value in initial_locale list).
  const std::string& initial_locale_default() const;
  const std::string& initial_timezone() const { return initial_timezone_; }
  const std::string& keyboard_layout() const { return keyboard_layout_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, VPD);
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, BadManifest);
  FRIEND_TEST_ALL_PREFIXES(ServicesCustomizationDocumentTest, MultiLanguage);
  friend class OobeLocalizationTest;
  friend void ::Test__InitStartupCustomizationDocument(
      const std::string& manifest);
  friend struct DefaultSingletonTraits<StartupCustomizationDocument>;

  // C-tor for singleton construction.
  StartupCustomizationDocument();

  // C-tor for test construction.
  StartupCustomizationDocument(system::StatisticsProvider* provider,
                               const std::string& manifest);

  virtual ~StartupCustomizationDocument();

  void Init(system::StatisticsProvider* provider);

  // If |attr| exists in machine stat, assign it to |value|.
  void InitFromMachineStatistic(const char* attr, std::string* value);

  std::string initial_locale_;
  std::vector<std::string> configured_locales_;
  std::string initial_timezone_;
  std::string keyboard_layout_;

  DISALLOW_COPY_AND_ASSIGN(StartupCustomizationDocument);
};

// OEM services customization document class.
// ServicesCustomizationDocument is fetched from network therefore it is not
// ready just after creation. Fetching of the manifest should be initiated
// outside this class by calling StartFetching() method.
// User of the file should check IsReady before use it.
class ServicesCustomizationDocument : public CustomizationDocument,
                                      private net::URLFetcherDelegate {
 public:
  static ServicesCustomizationDocument* GetInstance();

  // Registers preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static const char kManifestUrl[];

  // Return true if the customization was applied. Customization is applied only
  // once per machine.
  static bool WasOOBECustomizationApplied();

  // Start fetching customization document.
  void StartFetching();

  // Apply customization and save in machine options that customization was
  // applied successfully. Return true if customization was applied.
  bool ApplyOOBECustomization();

  // Returns default wallpaper URL.
  GURL GetDefaultWallpaperUrl() const;

  // Returns list of default apps.
  bool GetDefaultApps(std::vector<std::string>* ids) const;

  // Creates an extensions::ExternalLoader that will provide OEM default apps.
  // Cache of OEM default apps stored in profile preferences.
  extensions::ExternalLoader* CreateExternalLoader(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<ServicesCustomizationDocument>;

  typedef std::vector<base::WeakPtr<ServicesCustomizationExternalLoader> >
      ExternalLoaders;

  // C-tor for singleton construction.
  ServicesCustomizationDocument();

  // C-tor for test construction.
  explicit ServicesCustomizationDocument(const std::string& manifest);

  virtual ~ServicesCustomizationDocument();

  // Save applied state in machine settings.
  static void SetApplied(bool val);

  // Overriden from CustomizationDocument:
  virtual bool LoadManifestFromString(const std::string& manifest) OVERRIDE;

  // Overriden from net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Initiate file fetching.
  void StartFileFetch();

  // Executes on FILE thread and reads file to string.
  void ReadFileInBackground(const base::FilePath& file);

  // Called on UI thread with results of ReadFileInBackground.
  void OnManifesteRead(const std::string& manifest);

  // Method called when manifest was successfully loaded.
  void OnManifestLoaded();

  // Returns list of default apps in ExternalProvider format.
  static scoped_ptr<base::DictionaryValue> GetDefaultAppsInProviderFormat(
      const base::DictionaryValue& root);

  // Update cached manifest for |profile|.
  void UpdateCachedManifest(Profile* profile);

  // Services customization manifest URL.
  GURL url_;

  // URLFetcher instance.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // Timer to retry fetching file if network is not available.
  base::OneShotTimer<ServicesCustomizationDocument> retry_timer_;

  // How many times we already tried to fetch customization manifest file.
  int num_retries_;

  // Manifest fetch is already in progress.
  bool fetch_started_;

  // Known external loaders.
  ExternalLoaders external_loaders_;

  DISALLOW_COPY_AND_ASSIGN(ServicesCustomizationDocument);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
