// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_EXTENSIONS_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_EXTENSIONS_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/registry_util.h"

namespace chrome_cleaner {

struct TestRegistryEntry {
  HKEY hkey;
  base::string16 path;
  base::string16 name;
  base::string16 value;

  TestRegistryEntry(HKEY hkey,
                    const base::string16& path,
                    const base::string16& name,
                    const base::string16& value);
  TestRegistryEntry(const TestRegistryEntry& other);
  TestRegistryEntry& operator=(const TestRegistryEntry& other);
};

const wchar_t kChromeExePath[] = L"google\\chrome\\application";
const wchar_t kFakeChromeFolder[] = L"google\\chrome\\application\\42.12.34.56";
const wchar_t kExtensionSettingsPolicyPath[] =
    L"software\\policies\\google\\chrome";
const wchar_t kExtensionSettingsName[] = L"ExtensionSettings";
const wchar_t kMasterPreferencesFileName[] = L"master_preferences";
const wchar_t kTestExtensionId1[] = L"ababababcdcdcdcdefefefefghghghgh";
const wchar_t kTestExtensionId2[] = L"aaaabbbbccccddddeeeeffffgggghhhh";
const wchar_t kTestExtensionId3[] = L"hhhheeeeebbbbbccccddddaaaaabcdef";
const wchar_t kTestExtensionId4[] = L"abcdefghgfedcbabcdefghgfedcbaced";
const wchar_t kTestExtensionId5[] = L"dcbdcbdcbdcbdcbdcbdcbdcbdcbdcbcd";
const wchar_t kTestExtensionId6[] = L"egegegegegegegegegegegegegegegeg";
const wchar_t kTestExtensionId7[] = L"nopnopnopnopnopnopnopnopnopnopno";

// Test force installed extension settings
const TestRegistryEntry kExtensionForcelistEntries[] = {
    {HKEY_LOCAL_MACHINE, kChromePoliciesForcelistKeyPath, L"test1",
     base::string16(kTestExtensionId3) + L"https://test.test/crxupdate2/crx"}};
const char kValidExtensionSettingsJson[] =
    "{\"extensionwithinstallmodeblockeda\":{\"installation_mode\":\"blocked\","
    "\"update_url\":"
    "\"https://test.test/crx\"},\"extensionwithnosettingsabcdefghi\":{}}";
const wchar_t kExtensionSettingsJson[] =
    LR"(
  {
    "dcbdcbdcbdcbdcbdcbdcbdcbdcbdcbcd": {
      "installation_mode": "force_installed",
      "update_url":"https://test.test/crx"
    },
    "abcdefghgfedcbabcdefghgfedcbaced" : {
      "installation_mode": "force_installed",
      "update_url":"https://test.test/crx"
    },
    "extensionwithinstallmodeblockeda": {
      "installation_mode": "blocked",
      "update_url":"https://test.test/crx"
    },
    "extensionwithnosettingsabcdefghi": {}
  })";
const wchar_t kExtensionSettingsJsonOnlyForced[] =
    LR"(
  {
    "dcbdcbdcbdcbdcbdcbdcbdcbdcbdcbcd": {
      "installation_mode": "force_installed",
      "update_url":"https://test.test/crx"
    },
    "abcdefghgfedcbabcdefghgfedcbaced": {
      "installation_mode": "force_installed",
      "update_url":"https://test.test/crx"
    }
  })";

// Test force installed default extensions
const char kInvalidDefaultExtensionsJson[] = "{ json: invalid }";
const char kDefaultExtensionsJson[] =
    R"(
  {
    "ababababcdcdcdcdefefefefghghghgh" : {
      "external_update_url":"https://test.test/crx"
    },
    "aaaabbbbccccddddeeeeffffgggghhhh" : {
      "external_update_url":"https://test.test/crx"
    },
    // Google Sheets
    "aapocclcgogkmnckokdopfmhonfmgoek" : {
      "external_update_url":"https://test.test/crx"
    },
  })";
const char kValidDefaultExtensionsJson[] =
    "{\"aapocclcgogkmnckokdopfmhonfmgoek\":{"
    "\"external_update_url\":\"https://test.test/crx\"}}";

// Test force installed master preferences
const char kMasterPreferencesJson[] =
    R"(
    {
      "homepage": "http://dev.chromium.org/",
      "extensions": {
        "settings": {
          "egegegegegegegegegegegegegegegeg": {
            "location": 1,
            "manifest": {
              "name": "Test extension"
            }
          },
          "nopnopnopnopnopnopnopnopnopnopno": {
            "location": 1,
            "manifest": {
              "name": "Another one"
            }
          }
        }
      }
    })";
const char kValidMasterPreferencesJson[] =
    "{\"extensions\":{\"settings\":{}},\"homepage\":\"http://dev.chromium.org/"
    "\"}";
const char kMasterPreferencesJsonNoExtensions[] =
    R"(
{
  "homepage": "http://dev.chromium.org/"
})";

bool CreateProfileWithExtensionAndFiles(
    const base::FilePath& profile_path,
    const base::string16& extension_id,
    const std::vector<base::string16>& extension_files);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_EXTENSIONS_H_
