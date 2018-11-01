// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/easy_unlock_settings_handler.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

namespace {

class FakeEasyUnlockService : public EasyUnlockService {
 public:
  explicit FakeEasyUnlockService(Profile* profile)
      : EasyUnlockService(profile, nullptr /* secure_channel_client */),
        is_allowed_(true),
        is_enabled_(false) {}

  bool IsAllowed() const override { return is_allowed_; }
  void set_is_allowed(bool is_allowed) { is_allowed_ = is_allowed; }

  bool IsEnabled() const override { return is_enabled_; }
  void set_is_enabled(bool is_enabled) { is_enabled_ = is_enabled; }

 private:
  Type GetType() const override { return TYPE_REGULAR; }
  AccountId GetAccountId() const override { return EmptyAccountId(); }
  void ClearPermitAccess() override {}

  const base::ListValue* GetRemoteDevices() const override { return nullptr; }
  void SetRemoteDevices(const base::ListValue& devices) override {}

  std::string GetChallenge() const override { return std::string(); }
  std::string GetWrappedSecret() const override { return std::string(); }
  void RecordEasySignInOutcome(const AccountId& account_id,
                               bool success) const override {}
  void RecordPasswordLoginEvent(const AccountId& account_id) const override {}

  void InitializeInternal() override {}
  void ShutdownInternal() override {}
  bool IsAllowedInternal() const override { return false; }
  void OnWillFinalizeUnlock(bool success) override {}
  void OnSuspendDoneInternal() override {}

  bool is_allowed_;
  bool is_enabled_;
};

class TestEasyUnlockSettingsHandler : public EasyUnlockSettingsHandler {
 public:
  explicit TestEasyUnlockSettingsHandler(Profile* profile)
      : EasyUnlockSettingsHandler(profile) {}

  using EasyUnlockSettingsHandler::set_web_ui;
};

std::unique_ptr<KeyedService> CreateEasyUnlockServiceForTest(
    content::BrowserContext* context) {
  return std::make_unique<FakeEasyUnlockService>(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService> CreateNullEasyUnlockServiceForTest(
    content::BrowserContext* context) {
  return nullptr;
}

}  // namespace

class EasyUnlockSettingsHandlerTest : public testing::Test {
 public:
  EasyUnlockSettingsHandlerTest() {}

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        EasyUnlockServiceFactory::GetInstance(),
        base::BindRepeating(&CreateEasyUnlockServiceForTest));
    profile_ = builder.Build();
  }

  Profile* profile() { return profile_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }
  FakeEasyUnlockService* fake_easy_unlock_service() {
    return static_cast<FakeEasyUnlockService*>(
        EasyUnlockService::Get(profile_.get()));
  }

  void MakeEasyUnlockServiceNull() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        EasyUnlockServiceFactory::GetInstance(),
        base::BindRepeating(&CreateNullEasyUnlockServiceForTest));
    profile_ = builder.Build();
  }

  void VerifyEnabledStatusCallback(size_t expected_total_calls,
                                   bool expected_status) {
    std::string event;
    bool status;

    EXPECT_EQ(expected_total_calls, web_ui_.call_data().size());

    const content::TestWebUI::CallData& data = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_TRUE(data.arg1()->GetAsString(&event));
    EXPECT_EQ("easy-unlock-enabled-status", event);
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&status));

    EXPECT_EQ(expected_status, status);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
};

TEST_F(EasyUnlockSettingsHandlerTest, OnlyCreatedWhenEasyUnlockAllowed) {
  std::unique_ptr<EasyUnlockSettingsHandler> handler;
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::Create("test-data-source");
  content::WebUIDataSource::Add(profile(), data_source);
  handler.reset(
      EasyUnlockSettingsHandler::Create(data_source, profile()));
  EXPECT_TRUE(handler.get());

  fake_easy_unlock_service()->set_is_allowed(false);
  handler.reset(EasyUnlockSettingsHandler::Create(data_source, profile()));
  EXPECT_FALSE(handler.get());
}

TEST_F(EasyUnlockSettingsHandlerTest, NotCreatedWhenEasyUnlockServiceNull) {
  MakeEasyUnlockServiceNull();
  std::unique_ptr<EasyUnlockSettingsHandler> handler;
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::Create("test-data-source");
  content::WebUIDataSource::Add(profile(), data_source);
  handler.reset(EasyUnlockSettingsHandler::Create(data_source, profile()));
  EXPECT_FALSE(handler.get());
}

}  // namespace settings
}  // namespace chromeos
