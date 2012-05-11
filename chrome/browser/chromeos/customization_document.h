// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#define CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/timer.h"
#include "base/values.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class FilePath;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace chromeos {

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

  virtual bool LoadManifestFromFile(const FilePath& manifest_path);
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

  std::string GetHelpPage(const std::string& locale) const;
  std::string GetEULAPage(const std::string& locale) const;

  const std::string& registration_url() const { return registration_url_; }

  // These methods can be called even if !IsReady(), in this case VPD values
  // will be returned.
  const std::string& initial_locale() const { return initial_locale_; }
  const std::string& initial_timezone() const { return initial_timezone_; }
  const std::string& keyboard_layout() const { return keyboard_layout_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, VPD);
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, BadManifest);
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
                                      private content::URLFetcherDelegate {
 public:
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

 private:
  FRIEND_TEST_ALL_PREFIXES(ServicesCustomizationDocumentTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ServicesCustomizationDocumentTest, BadManifest);
  friend struct DefaultSingletonTraits<ServicesCustomizationDocument>;

  // C-tor for singleton construction.
  ServicesCustomizationDocument();

  // C-tor for test construction.
  explicit ServicesCustomizationDocument(const std::string& manifest);

  virtual ~ServicesCustomizationDocument();

  // Save applied state in machine settings.
  static void SetApplied(bool val);

  // Overriden from content::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Initiate file fetching.
  void StartFileFetch();

  // Executes on FILE thread and reads file to string.
  void ReadFileInBackground(const FilePath& file);

  // Services customization manifest URL.
  GURL url_;

  // URLFetcher instance.
  scoped_ptr<content::URLFetcher> url_fetcher_;

  // Timer to retry fetching file if network is not available.
  base::OneShotTimer<ServicesCustomizationDocument> retry_timer_;

  // How many times we already tried to fetch customization manifest file.
  int num_retries_;

  DISALLOW_COPY_AND_ASSIGN(ServicesCustomizationDocument);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
