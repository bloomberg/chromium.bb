// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_TEST_UTIL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"

namespace extensions {

// Base class for essential routines on preference manipulation.
class ExtensionManagementPrefUpdaterBase {
 public:
  ExtensionManagementPrefUpdaterBase();
  virtual ~ExtensionManagementPrefUpdaterBase();

  // Helper functions for per extension settings.
  void UnsetPerExtensionSettings(const ExtensionId& id);
  void ClearPerExtensionSettings(const ExtensionId& id);

  // Helper functions for 'installation_mode' manipulation.
  void SetBlacklistedByDefault(bool value);
  void ClearInstallationModesForIndividualExtensions();
  void SetIndividualExtensionInstallationAllowed(const ExtensionId& id,
                                                 bool allowed);
  void SetIndividualExtensionAutoInstalled(const ExtensionId& id,
                                           const std::string& update_url,
                                           bool forced);

  // Helper functions for 'install_sources' manipulation.
  void UnsetInstallSources();
  void ClearInstallSources();
  void AddInstallSource(const std::string& install_source);
  void RemoveInstallSource(const std::string& install_source);

  // Helper functions for 'allowed_types' manipulation.
  void UnsetAllowedTypes();
  void ClearAllowedTypes();
  void AddAllowedType(const std::string& allowed_type);
  void RemoveAllowedType(const std::string& allowd_type);

  // Expose a read-only preference to user.
  const base::DictionaryValue* GetPref();

 protected:
  // Set the preference with |pref|, pass the ownership of it as well.
  // This function must be called before accessing publicly exposed functions,
  // for example in constructor of subclass.
  void SetPref(base::DictionaryValue* pref);

  // Take the preference. Caller takes ownership of it as well.
  // This function must be called after accessing publicly exposed functions,
  // for example in destructor of subclass.
  scoped_ptr<base::DictionaryValue> TakePref();

 private:
  // Helper functions for manipulating sub properties like list of strings.
  void ClearList(const std::string& path);
  void AddStringToList(const std::string& path, const std::string& str);
  void RemoveStringFromList(const std::string& path, const std::string& str);

  scoped_ptr<base::DictionaryValue> pref_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagementPrefUpdaterBase);
};

// A helper class to manipulate the extension management preference in unit
// tests.
template <class TestingPrefService>
class ExtensionManagementPrefUpdater
    : public ExtensionManagementPrefUpdaterBase {
 public:
  explicit ExtensionManagementPrefUpdater(TestingPrefService* service)
      : service_(service) {
    const base::Value* pref_value =
        service_->GetManagedPref(pref_names::kExtensionManagement);
    if (pref_value) {
      const base::DictionaryValue* dict_value = NULL;
      pref_value->GetAsDictionary(&dict_value);
      SetPref(dict_value->DeepCopy());
    } else {
      SetPref(new base::DictionaryValue);
    }
  }

  virtual ~ExtensionManagementPrefUpdater() {
    service_->SetManagedPref(pref_names::kExtensionManagement,
                             TakePref().release());
  }

 private:
  TestingPrefService* service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagementPrefUpdater);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_TEST_UTIL_H_
