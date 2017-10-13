// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/test/fake_policy_instance.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeONC[] =
    "{\"NetworkConfigurations\":["
    "{\"GUID\":\"{485d6076-dd44-6b6d-69787465725f5040}\","
    "\"Type\":\"WiFi\","
    "\"Name\":\"My WiFi Network\","
    "\"WiFi\":{"
    "\"HexSSID\":\"737369642D6E6F6E65\","  // "ssid-none"
    "\"Security\":\"None\"}"
    "}"
    "],"
    "\"GlobalNetworkConfiguration\":{"
    "\"AllowOnlyPolicyNetworksToAutoconnect\":true,"
    "},"
    "\"Certificates\":["
    "{ \"GUID\":\"{f998f760-272b-6939-4c2beffe428697ac}\","
    "\"PKCS12\":\"abc\","
    "\"Type\":\"Client\"},"
    "{\"Type\":\"Authority\","
    "\"TrustBits\":[\"Web\"],"
    "\"X509\":\"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ"
    "1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpc"
    "yBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCB"
    "pbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZ"
    "GdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4"
    "=\","
    "\"GUID\":\"{00f79111-51e0-e6e0-76b3b55450d80a1b}\"}"
    "]}";

constexpr char kPolicyCompliantResponse[] = "{ \"policyCompliant\": true }";

// Helper class to define callbacks that verify that they were run.
// Wraps a bool initially set to |false| and verifies that it's been set to
// |true| before destruction.
class CheckedBoolean {
 public:
  CheckedBoolean() {}
  ~CheckedBoolean() { EXPECT_TRUE(value_); }

  void set_value(bool value) { value_ = value; }

 private:
  bool value_ = false;

  DISALLOW_COPY_AND_ASSIGN(CheckedBoolean);
};

void ExpectString(std::unique_ptr<CheckedBoolean> was_run,
                  const std::string& expected,
                  const std::string& received) {
  EXPECT_EQ(expected, received);
  was_run->set_value(true);
}

void ExpectStringWithClosure(base::Closure quit_closure,
                             std::unique_ptr<CheckedBoolean> was_run,
                             const std::string& expected,
                             const std::string& received) {
  EXPECT_EQ(expected, received);
  was_run->set_value(true);
  quit_closure.Run();
}

arc::ArcPolicyBridge::GetPoliciesCallback PolicyStringCallback(
    const std::string& expected) {
  auto was_run = std::make_unique<CheckedBoolean>();
  return base::Bind(&ExpectString, base::Passed(&was_run), expected);
}

arc::ArcPolicyBridge::ReportComplianceCallback PolicyComplianceCallback(
    base::Closure quit_closure,
    const std::string& expected) {
  auto was_run = std::make_unique<CheckedBoolean>();
  return base::Bind(&ExpectStringWithClosure, quit_closure,
                    base::Passed(&was_run), expected);
}

}  // namespace

using testing::_;
using testing::ReturnRef;

namespace arc {

class ArcPolicyBridgeTest : public testing::Test {
 public:
  ArcPolicyBridgeTest() {}

  void SetUp() override {
    bridge_service_ = std::make_unique<ArcBridgeService>();
    EXPECT_CALL(policy_service_,
                GetPolicies(policy::PolicyNamespace(
                    policy::POLICY_DOMAIN_CHROME, std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));
    EXPECT_CALL(policy_service_, AddObserver(policy::POLICY_DOMAIN_CHROME, _))
        .Times(1);

    policy_instance_ = std::make_unique<FakePolicyInstance>();
    bridge_service_->policy()->SetInstance(policy_instance_.get());

    // Setting up user profile for ReportCompliance() tests.
    chromeos::FakeChromeUserManager* const fake_user_manager =
        new chromeos::FakeChromeUserManager();
    user_manager_enabler_ =
        std::make_unique<chromeos::ScopedUserManagerEnabler>(fake_user_manager);
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("user@gmail.com", "1111111111"));
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile("user@gmail.com");
    ASSERT_TRUE(profile_);

    // TODO(hidehiko): Use Singleton instance tied to BrowserContext.
    policy_bridge_ = std::make_unique<ArcPolicyBridge>(
        profile_, bridge_service_.get(), &policy_service_);
    policy_bridge_->OverrideIsManagedForTesting(true);
  }

 protected:
  ArcPolicyBridge* policy_bridge() { return policy_bridge_.get(); }
  FakePolicyInstance* policy_instance() { return policy_instance_.get(); }
  policy::PolicyMap& policy_map() { return policy_map_; }
  base::RunLoop& run_loop() { return run_loop_; }
  Profile* profile() { return profile_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  data_decoder::TestingJsonParser::ScopedFactoryOverride factory_override_;
  content::TestServiceManagerContext service_manager_context_;
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::RunLoop run_loop_;
  TestingProfile* profile_;

  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<ArcPolicyBridge> policy_bridge_;
  // Always keep policy_instance_ below bridge_service_, so that
  // policy_instance_ is destructed first. It needs to remove itself as
  // observer.
  std::unique_ptr<FakePolicyInstance> policy_instance_;
  policy::PolicyMap policy_map_;
  policy::MockPolicyService policy_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcPolicyBridgeTest);
};

TEST_F(ArcPolicyBridgeTest, UnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  policy_bridge()->GetPolicies(PolicyStringCallback(""));
}

TEST_F(ArcPolicyBridgeTest, EmptyPolicyTest) {
  // No policy is set, result should be empty.
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, ArcPolicyTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(
          "{\"applications\":"
          "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
          "\"installType\":\"REQUIRED\","
          "\"lockTaskAllowed\":false,"
          "\"permissionGrants\":[]"
          "}],"
          "\"defaultPermissionPolicy\":\"GRANT\""
          "}"),
      nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback(
      "{\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.google.android.apps.youtube.kids\","
      "\"permissionGrants\":[]"
      "}],"
      "\"defaultPermissionPolicy\":\"GRANT\""
      "}"));
}

TEST_F(ArcPolicyBridgeTest, HompageLocationTest) {
  // This policy will not be passed on, result should be empty.
  policy_map().Set(
      policy::key::kHomepageLocation, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>("http://chromium.org"), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, DisableScreenshotsTest) {
  policy_map().Set(policy::key::kDisableScreenshots,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(true), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"screenCaptureDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, VideoCaptureAllowedTest) {
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(false), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"cameraDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, AudioCaptureAllowedTest) {
  policy_map().Set(policy::key::kAudioCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(false), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"unmuteMicrophoneDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, DefaultGeolocationSettingTest) {
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(1), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":false}"));
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(2), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":true}"));
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(3), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":false}"));
}

TEST_F(ArcPolicyBridgeTest, ExternalStorageDisabledTest) {
  policy_map().Set(policy::key::kExternalStorageDisabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(true), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"mountPhysicalMediaDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, WallpaperImageSetTest) {
  base::DictionaryValue dict;
  dict.SetString("url", "https://example.com/wallpaper.jpg");
  dict.SetString("hash", "somehash");
  policy_map().Set(policy::key::kWallpaperImage, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   dict.CreateDeepCopy(), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"setWallpaperDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, WallpaperImageSet_NotCompletePolicyTest) {
  base::DictionaryValue dict;
  dict.SetString("url", "https://example.com/wallpaper.jpg");
  // "hash" attribute is missing, so the policy shouldn't be set
  policy_map().Set(policy::key::kWallpaperImage, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   dict.CreateDeepCopy(), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, CaCertificateTest) {
  // Enable CA certificates sync.
  policy_map().Set(
      policy::key::kArcCertificatesSyncMode, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(ArcCertsSyncMode::COPY_CA_CERTS), nullptr);
  policy_map().Set(policy::key::kOpenNetworkConfiguration,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(kFakeONC), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback(
      "{\"caCerts\":"
      "[{\"X509\":\"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24"
      "sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGl"
      "jaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGV"
      "saWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Y"
      "ga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCB"
      "wbGVhc3VyZS4=\"}"
      "]}"));

  // Disable CA certificates sync.
  policy_map().Set(
      policy::key::kArcCertificatesSyncMode, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(ArcCertsSyncMode::SYNC_DISABLED), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, DeveloperToolsDisabledTest) {
  policy_map().Set(policy::key::kDeveloperToolsDisabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(true), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"debuggingFeaturesDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, MultiplePoliciesTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(
          "{\"applications\":"
          "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
          "\"installType\":\"REQUIRED\","
          "\"lockTaskAllowed\":false,"
          "\"permissionGrants\":[]"
          "}],"
          "\"defaultPermissionPolicy\":\"GRANT\"}"),
      nullptr);
  policy_map().Set(
      policy::key::kHomepageLocation, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>("http://chromium.org"), nullptr);
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(false), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback(
      "{\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.google.android.apps.youtube.kids\","
      "\"permissionGrants\":[]"
      "}],"
      "\"cameraDisabled\":true,"
      "\"defaultPermissionPolicy\":\"GRANT\""
      "}"));
}

TEST_F(ArcPolicyBridgeTest, EmptyReportComplianceTest) {
  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  policy_bridge()->ReportCompliance(
      "{}", PolicyComplianceCallback(run_loop().QuitClosure(),
                                     kPolicyCompliantResponse));
  run_loop().Run();
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, ParsableReportComplianceTest) {
  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  policy_bridge()->ReportCompliance(
      "{\"nonComplianceDetails\" : []}",
      PolicyComplianceCallback(run_loop().QuitClosure(),
                               kPolicyCompliantResponse));
  run_loop().Run();
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, NonParsableReportComplianceTest) {
  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  policy_bridge()->ReportCompliance(
      "\"nonComplianceDetails\" : [}",
      PolicyComplianceCallback(run_loop().QuitClosure(),
                               kPolicyCompliantResponse));
  run_loop().Run();
  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

TEST_F(ArcPolicyBridgeTest, ReportComplianceTest_WithNonCompliantDetails) {
  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
  policy_bridge()->ReportCompliance(
      "{\"nonComplianceDetails\" : "
      "[{\"fieldPath\":\"\",\"nonComplianceReason\":0,\"packageName\":\"\","
      "\"settingName\":\"someSetting\",\"cachedSize\":-1}]}",
      PolicyComplianceCallback(run_loop().QuitClosure(),
                               kPolicyCompliantResponse));
  run_loop().Run();
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcPolicyComplianceReported));
}

// This and the following test send the policies through a mojo connection
// between a PolicyInstance and the PolicyBridge.
TEST_F(ArcPolicyBridgeTest, PolicyInstanceUnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  policy_instance()->CallGetPolicies(PolicyStringCallback(""));
}

TEST_F(ArcPolicyBridgeTest, PolicyInstanceManagedTest) {
  policy_instance()->CallGetPolicies(PolicyStringCallback("{}"));
}

}  // namespace arc
