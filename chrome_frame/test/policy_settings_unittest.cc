// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/policy_constants.h"
#include "chrome_frame/policy_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A best effort way to zap CF policy entries that may be in the registry.
void DeleteChromeFramePolicyEntries(HKEY root) {
  RegKey key;
  if (key.Open(root, policy::kRegistrySubKey, KEY_ALL_ACCESS)) {
    key.DeleteValue(
        ASCIIToWide(policy::key::kChromeFrameRendererSettings).c_str());
    key.DeleteKey(ASCIIToWide(policy::key::kRenderInChromeFrameList).c_str());
    key.DeleteKey(ASCIIToWide(policy::key::kRenderInHostList).c_str());
  }
}

class TempRegKeyOverride {
 public:
  static const wchar_t kTempTestKeyPath[];

  TempRegKeyOverride(HKEY override, const wchar_t* temp_name)
      : override_(override), temp_name_(temp_name) {
    DCHECK(temp_name && lstrlenW(temp_name));
    std::wstring key_path(kTempTestKeyPath);
    key_path += L"\\" + temp_name_;
    EXPECT_TRUE(temp_key_.Create(HKEY_CURRENT_USER, key_path.c_str(),
                                 KEY_ALL_ACCESS));
    EXPECT_EQ(ERROR_SUCCESS,
              ::RegOverridePredefKey(override_, temp_key_.Handle()));
  }

  ~TempRegKeyOverride() {
    ::RegOverridePredefKey(override_, NULL);
    // The temp key will be deleted via a call to DeleteAllTempKeys().
  }

  static void DeleteAllTempKeys() {
    RegKey key;
    if (key.Open(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS)) {
      key.DeleteKey(kTempTestKeyPath);
    }
  }

 protected:
  HKEY override_;
  RegKey temp_key_;
  std::wstring temp_name_;
};

const wchar_t TempRegKeyOverride::kTempTestKeyPath[] =
    L"Software\\Chromium\\TempTestKeys";

bool SetRendererSettings(HKEY policy_root,
                         PolicySettings::RendererForUrl renderer,
                         const wchar_t* exclusions[],
                         int exclusion_count) {
  RegKey policy_key;
  EXPECT_TRUE(policy_key.Create(policy_root, policy::kRegistrySubKey,
                                KEY_ALL_ACCESS));
  if (!policy_key.Valid())
    return false;

  policy_key.WriteValue(
      ASCIIToWide(policy::key::kChromeFrameRendererSettings).c_str(),
      static_cast<DWORD>(renderer));

  std::wstring in_cf(ASCIIToWide(policy::key::kRenderInChromeFrameList));
  std::wstring in_host(ASCIIToWide(policy::key::kRenderInHostList));
  // Remove any previous settings
  policy_key.DeleteKey(in_cf.c_str());
  policy_key.DeleteKey(in_host.c_str());

  std::wstring exclusion_list(
      renderer == PolicySettings::RENDER_IN_CHROME_FRAME ? in_host : in_cf);
  RegKey exclusion_key;
  EXPECT_TRUE(exclusion_key.Create(policy_key.Handle(), exclusion_list.c_str(),
                                   KEY_ALL_ACCESS));
  for (int i = 0; i < exclusion_count; ++i) {
    EXPECT_TRUE(exclusion_key.WriteValue(base::StringPrintf(L"%i", i).c_str(),
                                         exclusions[i]));
  }

  return true;
}

}  // end namespace

TEST(PolicySettings, RendererForUrl) {
  TempRegKeyOverride::DeleteAllTempKeys();

  scoped_ptr<TempRegKeyOverride> hklm_pol(
      new TempRegKeyOverride(HKEY_LOCAL_MACHINE, L"hklm_pol"));
  scoped_ptr<TempRegKeyOverride> hkcu_pol(
      new TempRegKeyOverride(HKEY_CURRENT_USER, L"hkcu_pol"));

  const wchar_t* kTestUrls[] = {
    L"http://www.example.com",
    L"http://www.pattern.com",
    L"http://www.test.com"
  };
  const wchar_t* kTestFilters[] = {
    L"*.example.com",
    L"*.pattern.com",
    L"*.test.com"
  };
  const wchar_t kNoMatchUrl[] = L"http://www.chromium.org";

  scoped_ptr<PolicySettings> settings(new PolicySettings());
  EXPECT_EQ(settings->default_renderer(),
            PolicySettings::RENDERER_NOT_SPECIFIED);
  EXPECT_EQ(settings->GetRendererForUrl(kNoMatchUrl),
            PolicySettings::RENDERER_NOT_SPECIFIED);

  HKEY root[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  for (int i = 0; i < arraysize(root); ++i) {
    EXPECT_TRUE(SetRendererSettings(root[i],
        PolicySettings::RENDER_IN_CHROME_FRAME, kTestFilters,
        arraysize(kTestFilters)));

    settings.reset(new PolicySettings());
    EXPECT_EQ(settings->GetRendererForUrl(kNoMatchUrl),
              PolicySettings::RENDER_IN_CHROME_FRAME);
    for (int j = 0; j < arraysize(kTestUrls); ++j) {
      EXPECT_EQ(settings->GetRendererForUrl(kTestUrls[j]),
                PolicySettings::RENDER_IN_HOST);
    }

    EXPECT_TRUE(SetRendererSettings(root[i],
        PolicySettings::RENDER_IN_HOST, NULL, 0));

    settings.reset(new PolicySettings());
    EXPECT_EQ(settings->GetRendererForUrl(kNoMatchUrl),
              PolicySettings::RENDER_IN_HOST);
    for (int j = 0; j < arraysize(kTestUrls); ++j) {
      EXPECT_EQ(settings->GetRendererForUrl(kTestUrls[j]),
                PolicySettings::RENDER_IN_HOST);
    }

    DeleteChromeFramePolicyEntries(root[i]);
  }

  hkcu_pol.reset(NULL);
  hklm_pol.reset(NULL);
  TempRegKeyOverride::DeleteAllTempKeys();
}
