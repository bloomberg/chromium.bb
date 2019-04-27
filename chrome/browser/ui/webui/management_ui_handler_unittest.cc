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

#if defined(OS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif  // defined(OS_CHROMEOS)

using testing::_;
using testing::Return;
using testing::ReturnRef;

struct ContextualManagementSourceUpdate {
  base::string16* extension_reporting_title;
  base::string16* browser_management_notice;
  base::string16* subtitle;
  base::string16* management_overview;
  base::string16* management_overview_data_notice;
  base::string16* management_overview_setup_notice;
  bool* managed;
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

  base::DictionaryValue GetContextualManagedDataForTesting(
      Profile* profile) const {
    return GetContextualManagedData(profile);
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
      const base::DictionaryValue& data,
      const ContextualManagementSourceUpdate& extracted) {
    data.GetString("extensionReportingTitle",
                   extracted.extension_reporting_title);
    data.GetString("browserManagementNotice",
                   extracted.browser_management_notice);
    data.GetString("pageSubtitle", extracted.subtitle);
    data.GetString("accountManagedInfo.overview",
                   extracted.management_overview);
    data.GetString("accountManagedInfo.data",
                   extracted.management_overview_data_notice);
    data.GetString("accountManagedInfo.setup",
                   extracted.management_overview_setup_notice);
    data.GetBoolean("managed", extracted.managed);
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

  base::string16 extension_reporting_title;
  base::string16 browser_management_notice;
  base::string16 subtitle;
  base::string16 management_overview;
  base::string16 management_overview_data_notice;
  base::string16 management_overview_setup_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{&extension_reporting_title,
                                             &browser_management_notice,
                                             &subtitle,
                                             &management_overview,
                                             &management_overview_data_notice,
                                             &management_overview_setup_notice,
                                             &managed};

  handler_.SetManagedForTesting(false);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_EQ(management_overview, base::string16());
  EXPECT_EQ(management_overview_data_notice, base::string16());
  EXPECT_EQ(management_overview_setup_notice, base::string16());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedNoDomain) {
  auto profile = TestingProfile::Builder().Build();

  base::string16 extension_reporting_title;
  base::string16 browser_management_notice;
  base::string16 subtitle;
  base::string16 management_overview;
  base::string16 management_overview_data_notice;
  base::string16 management_overview_setup_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{&extension_reporting_title,
                                             &browser_management_notice,
                                             &subtitle,
                                             &management_overview,
                                             &management_overview_data_notice,
                                             &management_overview_setup_notice,
                                             &managed};

  handler_.SetManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 5u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle, l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE));
  EXPECT_EQ(management_overview,
            l10n_util::GetStringUTF16(
                IDS_MANAGEMENT_ACCOUNT_MANAGED_CLARIFICATION_UNKNOWN_DOMAIN));
  EXPECT_EQ(management_overview_data_notice,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_DATA));
  EXPECT_EQ(management_overview_setup_notice,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_SETUP));
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedConsumerDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@gmail.com");
  auto profile = builder.Build();

  base::string16 extensions_installed;
  base::string16 browser_management_notice;
  base::string16 subtitle;
  base::string16 management_overview;
  base::string16 management_overview_data_notice;
  base::string16 management_overview_setup_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{&extensions_installed,
                                             &browser_management_notice,
                                             &subtitle,
                                             &management_overview,
                                             &management_overview_data_notice,
                                             &management_overview_setup_notice,
                                             &managed};

  handler_.SetManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 5u);
  EXPECT_EQ(extensions_installed,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle, l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE));
  EXPECT_EQ(management_overview,
            l10n_util::GetStringUTF16(
                IDS_MANAGEMENT_ACCOUNT_MANAGED_CLARIFICATION_UNKNOWN_DOMAIN));
  EXPECT_EQ(management_overview_data_notice,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_DATA));
  EXPECT_EQ(management_overview_setup_notice,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_SETUP));
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedKnownDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();

  base::string16 extension_reporting_title;
  base::string16 browser_management_notice;
  base::string16 subtitle;
  base::string16 management_overview;
  base::string16 management_overview_data_notice;
  base::string16 management_overview_setup_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{&extension_reporting_title,
                                             &browser_management_notice,
                                             &subtitle,
                                             &management_overview,
                                             &management_overview_data_notice,
                                             &management_overview_setup_notice,
                                             &managed};

  handler_.SetManagedForTesting(false);

  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_EQ(management_overview, base::string16());
  EXPECT_EQ(management_overview_data_notice, base::string16());
  EXPECT_EQ(management_overview_setup_notice, base::string16());
  EXPECT_FALSE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedCustomerDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@googlemail.com");
  auto profile = builder.Build();

  base::string16 extension_reporting_title;
  base::string16 browser_management_notice;
  base::string16 subtitle;
  base::string16 management_overview;
  base::string16 management_overview_data_notice;
  base::string16 management_overview_setup_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{&extension_reporting_title,
                                             &browser_management_notice,
                                             &subtitle,
                                             &management_overview,
                                             &management_overview_data_notice,
                                             &management_overview_setup_notice,
                                             &managed};

  handler_.SetManagedForTesting(false);

  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_EQ(management_overview, base::string16());
  EXPECT_EQ(management_overview_data_notice, base::string16());
  EXPECT_EQ(management_overview_setup_notice, base::string16());
  EXPECT_FALSE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedKnownDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();

  base::string16 extension_reporting_title;
  base::string16 browser_management_notice;
  base::string16 subtitle;
  base::string16 management_overview;
  base::string16 management_overview_data_notice;
  base::string16 management_overview_setup_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{&extension_reporting_title,
                                             &browser_management_notice,
                                             &subtitle,
                                             &management_overview,
                                             &management_overview_data_notice,
                                             &management_overview_setup_notice,
                                             &managed};

  handler_.SetManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 5u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(
      browser_management_notice,
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_MANAGEMENT_BY_NOTICE, base::UTF8ToUTF16("manager.com"),
          base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(
      management_overview,
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_CLARIFICATION,
                                 base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(management_overview_data_notice,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_DATA));
  EXPECT_EQ(management_overview_setup_notice,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_SETUP));
  EXPECT_TRUE(managed);
}

#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedAccountKnownDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  base::string16 extensions_installed;
  base::string16 browser_management_notice;
  base::string16 subtitle;
  base::string16 management_overview;
  base::string16 management_overview_data_notice;
  base::string16 management_overview_setup_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{&extensions_installed,
                                             &browser_management_notice,
                                             &subtitle,
                                             &management_overview,
                                             &management_overview_data_notice,
                                             &management_overview_setup_notice,
                                             &managed};

  handler_.SetManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests, ManagementContextualSourceUpdateUnmanaged) {
  auto profile = TestingProfile::Builder().Build();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  base::string16 extension_reporting_title;
  base::string16 browser_management_notice;
  base::string16 subtitle;
  base::string16 management_overview;
  base::string16 management_overview_data_notice;
  base::string16 management_overview_setup_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{&extension_reporting_title,
                                             &browser_management_notice,
                                             &subtitle,
                                             &management_overview,
                                             &management_overview_data_notice,
                                             &management_overview_setup_notice,
                                             &managed};

  handler_.SetManagedForTesting(false);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);
  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_FALSE(managed);
}
#endif

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
