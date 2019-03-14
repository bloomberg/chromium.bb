// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/strings/utf_string_conversions.h"

#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/test/base/testing_profile.h"

#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::Return;
using testing::ReturnRef;

struct ContextualManagementSourceUpdate {
  base::string16* extensions_installed;
  base::string16* browser_management_notice;
  base::string16* title;
};

class TestManagementUIHandler : public ManagementUIHandler {
 public:
  TestManagementUIHandler() = default;
  explicit TestManagementUIHandler(policy::PolicyService* policy_service)
      : policy_service_(policy_service) {}
  ~TestManagementUIHandler() override = default;

  void EnableCloudReportingExtension(bool enable) {
    cloud_reporting_extension_exists_ = enable;
  }

  std::unique_ptr<base::DictionaryValue> GetDataSourceUpdate(
      Profile* profile) const {
    return GetDataManagementContextualSourceUpdate(profile);
  }

  base::Value GetExtensionReportingInfo() {
    base::Value report_sources(base::Value::Type::LIST);
    AddExtensionReportingInfo(&report_sources);
    return report_sources;
  }

  policy::PolicyService* GetPolicyService() const override {
    return policy_service_;
  }

  const extensions::Extension* GetEnabledExtension(
      const std::string& extensionId) const override {
    if (cloud_reporting_extension_exists_)
      return extensions::ExtensionBuilder("dummy").SetID("id").Build().get();
    return nullptr;
  }

 private:
  bool cloud_reporting_extension_exists_ = false;
  policy::PolicyService* policy_service_ = nullptr;
};

class ManagementUIHandlerTests : public testing::Test {
 public:
  ManagementUIHandlerTests() : handler_(&policy_service_) {
    ON_CALL(policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(empty_policy_map_));
  }

  void EnablePolicy(const char* policy_key, policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(true), nullptr);
  }

  void ExtractContextualSourceUpdate(
      base::DictionaryValue* data,
      const ContextualManagementSourceUpdate& extracted) {
    data->GetString("extensionsInstalled", extracted.extensions_installed);
    data->GetString("managementNotice", extracted.browser_management_notice);
    data->GetString("title", extracted.title);
  }

 protected:
  TestManagementUIHandler handler_;
  content::TestBrowserThreadBundle thread_bundle_;
  policy::MockPolicyService policy_service_;
  policy::PolicyMap empty_policy_map_;
};

#if !defined(OS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedNoDomain) {
  auto profile = TestingProfile::Builder().Build();

  base::string16 extensions_installed;
  base::string16 browser_management_notice;
  base::string16 title;
  ContextualManagementSourceUpdate extracted{
      &extensions_installed, &browser_management_notice, &title};

  handler_.SetManagedForTesting(false);
  auto data = handler_.GetDataSourceUpdate(profile.get());
  ExtractContextualSourceUpdate(data.get(), extracted);

  EXPECT_EQ(data->DictSize(), 3u);
  EXPECT_EQ(extensions_installed,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(title, l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_TITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManageNoDomain) {
  auto profile = TestingProfile::Builder().Build();

  base::string16 extensions_installed;
  base::string16 browser_management_notice;
  base::string16 title;
  ContextualManagementSourceUpdate extracted{
      &extensions_installed, &browser_management_notice, &title};

  handler_.SetManagedForTesting(true);
  auto data = handler_.GetDataSourceUpdate(profile.get());
  ExtractContextualSourceUpdate(data.get(), extracted);

  EXPECT_EQ(data->DictSize(), 3u);
  EXPECT_EQ(extensions_installed,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(title, l10n_util::GetStringUTF16(IDS_MANAGEMENT_TITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedKnownDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();

  base::string16 extensions_installed;
  base::string16 browser_management_notice;
  base::string16 title;
  ContextualManagementSourceUpdate extracted{
      &extensions_installed, &browser_management_notice, &title};

  handler_.SetManagedForTesting(false);

  auto data = handler_.GetDataSourceUpdate(profile.get());
  ExtractContextualSourceUpdate(data.get(), extracted);

  EXPECT_EQ(data->DictSize(), 3u);
  EXPECT_EQ(extensions_installed,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(title, l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_TITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedKnownDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();

  base::string16 extensions_installed;
  base::string16 browser_management_notice;
  base::string16 title;
  ContextualManagementSourceUpdate extracted{
      &extensions_installed, &browser_management_notice, &title};

  handler_.SetManagedForTesting(true);
  auto data = handler_.GetDataSourceUpdate(profile.get());
  ExtractContextualSourceUpdate(data.get(), extracted);

  EXPECT_EQ(data->DictSize(), 3u);
  EXPECT_EQ(extensions_installed,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(
      browser_management_notice,
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_MANAGEMENT_BY_NOTICE, base::UTF8ToUTF16("manager.com"),
          base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(title,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_TITLE_BY,
                                       base::UTF8ToUTF16("manager.com")));
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoNoPolicySetNoMessage) {
  handler_.EnableCloudReportingExtension(false);
  auto reporting_info = handler_.GetExtensionReportingInfo();
  EXPECT_EQ(reporting_info.GetList().size(), 0u);
}

TEST_F(ManagementUIHandlerTests,
       ExtensionReportingInfoCloudExtensionAddsDefaultPolicies) {
  handler_.EnableCloudReportingExtension(true);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportSafeBrowsingWarnings};

  auto reporting_info = handler_.GetExtensionReportingInfo();
  const auto& reporting_info_list = reporting_info.GetList();

  for (const base::Value& info : reporting_info_list) {
    const std::string* messageId = info.FindStringKey("messageId");
    EXPECT_TRUE(expected_messages.find(*messageId) != expected_messages.end());
  }
  EXPECT_EQ(reporting_info.GetList().size(), expected_messages.size());
}

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoPoliciesMerge) {
  policy::PolicyMap on_prem_reporting_extension_beta_policies;
  policy::PolicyMap on_prem_reporting_extension_stable_policies;

  EnablePolicy(kPolicyKeyReportUserIdData,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kManagementExtensionReportVersion,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kPolicyKeyReportUserIdData,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kPolicyKeyReportPolicyData,
               on_prem_reporting_extension_stable_policies);

  EnablePolicy(kPolicyKeyReportMachineIdData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportSafeBrowsingData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportSystemTelemetryData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportUserBrowsingData,
               on_prem_reporting_extension_stable_policies);

  const policy::PolicyNamespace
      on_prem_reporting_extension_stable_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionStableId);
  const policy::PolicyNamespace
      on_prem_reporting_extension_beta_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionBetaId);

  EXPECT_CALL(policy_service_,
              GetPolicies(on_prem_reporting_extension_stable_policy_namespace))
      .WillOnce(ReturnRef(on_prem_reporting_extension_stable_policies));

  EXPECT_CALL(policy_service_,
              GetPolicies(on_prem_reporting_extension_beta_policy_namespace))
      .WillOnce(ReturnRef(on_prem_reporting_extension_beta_policies));

  handler_.EnableCloudReportingExtension(true);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineNameAddress,
      kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportSafeBrowsingWarnings,
      kManagementExtensionReportUserBrowsingData,
      kManagementExtensionReportPerfCrash};

  auto reporting_info = handler_.GetExtensionReportingInfo();
  const auto& reporting_info_list = reporting_info.GetList();

  for (const base::Value& info : reporting_info_list) {
    const std::string* message_id = info.FindStringKey("messageId");
    ASSERT_TRUE(message_id != nullptr);
    EXPECT_TRUE(expected_messages.find(*message_id) != expected_messages.end());
  }
  EXPECT_EQ(reporting_info.GetList().size(), expected_messages.size());
}
