// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/at_exit.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome_frame/policy_settings.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "content/public/common/content_switches.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using chrome_frame_test::ScopedVirtualizeHklmAndHkcu;

namespace {

// A best effort way to zap CF policy entries that may be in the registry.
void DeleteChromeFramePolicyEntries(HKEY root) {
  RegKey key;
  if (key.Open(root, policy::kRegistryMandatorySubKey,
               KEY_ALL_ACCESS) == ERROR_SUCCESS) {
    key.DeleteValue(
        ASCIIToWide(policy::key::kChromeFrameRendererSettings).c_str());
    key.DeleteKey(ASCIIToWide(policy::key::kRenderInChromeFrameList).c_str());
    key.DeleteKey(ASCIIToWide(policy::key::kRenderInHostList).c_str());
    key.DeleteKey(ASCIIToWide(policy::key::kChromeFrameContentTypes).c_str());
    key.DeleteKey(ASCIIToWide(policy::key::kApplicationLocaleValue).c_str());
  }
}

bool InitializePolicyKey(HKEY policy_root, RegKey* policy_key) {
  EXPECT_EQ(ERROR_SUCCESS, policy_key->Create(policy_root,
      policy::kRegistryMandatorySubKey, KEY_ALL_ACCESS));
  return policy_key->Valid();
}

void WritePolicyList(RegKey* policy_key,
                     const wchar_t* list_name,
                     const wchar_t* values[], int count) {
  DCHECK(policy_key);
  // Remove any previous settings
  policy_key->DeleteKey(list_name);

  RegKey list_key;
  EXPECT_EQ(ERROR_SUCCESS, list_key.Create(policy_key->Handle(), list_name,
                                           KEY_ALL_ACCESS));
  for (int i = 0; i < count; ++i) {
    EXPECT_EQ(ERROR_SUCCESS,
        list_key.WriteValue(base::StringPrintf(L"%i", i).c_str(), values[i]));
  }
}

bool SetRendererSettings(HKEY policy_root,
                         PolicySettings::RendererForUrl renderer,
                         const wchar_t* exclusions[],
                         int exclusion_count) {
  RegKey policy_key;
  if (!InitializePolicyKey(policy_root, &policy_key))
    return false;

  policy_key.WriteValue(
      ASCIIToWide(policy::key::kChromeFrameRendererSettings).c_str(),
      static_cast<DWORD>(renderer));

  std::wstring in_cf(ASCIIToWide(policy::key::kRenderInChromeFrameList));
  std::wstring in_host(ASCIIToWide(policy::key::kRenderInHostList));
  std::wstring exclusion_list(
      renderer == PolicySettings::RENDER_IN_CHROME_FRAME ? in_host : in_cf);
  WritePolicyList(&policy_key, exclusion_list.c_str(), exclusions,
                  exclusion_count);

  return true;
}

bool SetCFContentTypes(HKEY policy_root,
                       const wchar_t* content_types[],
                       int count) {
  RegKey policy_key;
  if (!InitializePolicyKey(policy_root, &policy_key))
    return false;

  std::wstring type_list(ASCIIToWide(policy::key::kChromeFrameContentTypes));
  WritePolicyList(&policy_key, type_list.c_str(), content_types, count);

  return true;
}

bool SetCFPolicyString(HKEY policy_root,
                       const char* policy_name,
                       const wchar_t* value) {
  RegKey policy_key;
  if (!InitializePolicyKey(policy_root, &policy_key))
    return false;

  std::wstring policy_name_str(ASCIIToWide(policy_name));
  EXPECT_EQ(ERROR_SUCCESS,
      policy_key.WriteValue(policy_name_str.c_str(), value));
  return true;
}

}  // end namespace

class PolicySettingsTest : public testing::Test {
 protected:
  void SetUp() {
    ResetPolicySettings();
  }

  void TearDown() {
  }

  void ResetPolicySettings() {
    //at_exit_manager_.ProcessCallbacksNow();
    DeleteAllSingletons();
  }

  // This is used to manage life cycle of PolicySettings singleton.
  // base::ShadowingAtExitManager at_exit_manager_;

  ScopedVirtualizeHklmAndHkcu registry_virtualization_;
};

TEST_F(PolicySettingsTest, RendererForUrl) {
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

  EXPECT_EQ(PolicySettings::RENDERER_NOT_SPECIFIED,
            PolicySettings::GetInstance()->default_renderer());
  EXPECT_EQ(PolicySettings::RENDERER_NOT_SPECIFIED,
            PolicySettings::GetInstance()->GetRendererForUrl(kNoMatchUrl));

  HKEY root[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  for (int i = 0; i < arraysize(root); ++i) {
    EXPECT_TRUE(SetRendererSettings(root[i],
        PolicySettings::RENDER_IN_CHROME_FRAME, kTestFilters,
        arraysize(kTestFilters)));

    ResetPolicySettings();
    EXPECT_EQ(PolicySettings::RENDER_IN_CHROME_FRAME,
              PolicySettings::GetInstance()->GetRendererForUrl(kNoMatchUrl));
    for (int j = 0; j < arraysize(kTestUrls); ++j) {
      EXPECT_EQ(PolicySettings::RENDER_IN_HOST,
                PolicySettings::GetInstance()->GetRendererForUrl(kTestUrls[j]));
    }

    EXPECT_TRUE(SetRendererSettings(root[i],
        PolicySettings::RENDER_IN_HOST, NULL, 0));

    ResetPolicySettings();
    EXPECT_EQ(PolicySettings::RENDER_IN_HOST,
              PolicySettings::GetInstance()->GetRendererForUrl(kNoMatchUrl));
    for (int j = 0; j < arraysize(kTestUrls); ++j) {
      EXPECT_EQ(PolicySettings::RENDER_IN_HOST,
                PolicySettings::GetInstance()->GetRendererForUrl(kTestUrls[j]));
    }

    DeleteChromeFramePolicyEntries(root[i]);
  }
}

TEST_F(PolicySettingsTest, RendererForContentType) {
  EXPECT_EQ(PolicySettings::RENDERER_NOT_SPECIFIED,
            PolicySettings::GetInstance()->GetRendererForContentType(
                L"text/xml"));

  const wchar_t* kTestPolicyContentTypes[] = {
    L"application/xml",
    L"text/xml",
    L"application/pdf",
  };

  HKEY root[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  for (int i = 0; i < arraysize(root); ++i) {
    SetCFContentTypes(root[i], kTestPolicyContentTypes,
                      arraysize(kTestPolicyContentTypes));
    ResetPolicySettings();
    for (int type = 0; type < arraysize(kTestPolicyContentTypes); ++type) {
      EXPECT_EQ(PolicySettings::RENDER_IN_CHROME_FRAME,
                PolicySettings::GetInstance()->GetRendererForContentType(
                    kTestPolicyContentTypes[type]));
    }

    EXPECT_EQ(PolicySettings::RENDERER_NOT_SPECIFIED,
              PolicySettings::GetInstance()->GetRendererForContentType(
                  L"text/html"));

    DeleteChromeFramePolicyEntries(root[i]);
  }
}

TEST_F(PolicySettingsTest, ApplicationLocale) {
  EXPECT_TRUE(PolicySettings::GetInstance()->ApplicationLocale().empty());

  static const wchar_t kTestApplicationLocale[] = L"fr-CA";

  HKEY root[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  for (int i = 0; i < arraysize(root); ++i) {
    SetCFPolicyString(root[i], policy::key::kApplicationLocaleValue,
                      kTestApplicationLocale);
    ResetPolicySettings();
    EXPECT_EQ(std::wstring(kTestApplicationLocale),
              PolicySettings::GetInstance()->ApplicationLocale());

    DeleteChromeFramePolicyEntries(root[i]);
  }
}

TEST_F(PolicySettingsTest, AdditionalLaunchParameters) {
  EXPECT_TRUE(PolicySettings::GetInstance()->
      AdditionalLaunchParameters().GetProgram().empty());

  std::string test_switches("--");
  test_switches += switches::kDisableMediaSource;

  HKEY root[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
  for (int i = 0; i < arraysize(root); ++i) {
    SetCFPolicyString(root[i], policy::key::kAdditionalLaunchParameters,
                      ASCIIToWide(test_switches).c_str());
    ResetPolicySettings();
    const CommandLine& additional_params =
        PolicySettings::GetInstance()->AdditionalLaunchParameters();
    EXPECT_TRUE(additional_params.HasSwitch(switches::kDisableMediaSource));

    FilePath program_path(FILE_PATH_LITERAL("my_chrome.exe"));
    CommandLine new_cmd_line(program_path);
    new_cmd_line.AppendArguments(additional_params, false);
    EXPECT_NE(new_cmd_line.GetProgram(), additional_params.GetProgram());
    EXPECT_TRUE(new_cmd_line.HasSwitch(switches::kDisableMediaSource));

    DeleteChromeFramePolicyEntries(root[i]);
  }
}
