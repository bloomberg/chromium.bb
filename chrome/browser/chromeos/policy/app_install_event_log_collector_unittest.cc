// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_collector.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

class FakeAppInstallEventLogCollectorDelegate
    : public AppInstallEventLogCollector::Delegate {
 public:
  FakeAppInstallEventLogCollectorDelegate() = default;
  ~FakeAppInstallEventLogCollectorDelegate() override = default;

  // AppInstallEventLogCollector::Delegate:
  void AddForAllPackages(
      std::unique_ptr<em::AppInstallReportLogEvent> event) override {
    ++add_for_all_count_;
    last_event_ = *event;
  }

  void Add(const std::string& package,
           std::unique_ptr<em::AppInstallReportLogEvent> event) override {
    ++add_count_;
    last_event_ = *event;
  }

  int add_for_all_count() const { return add_for_all_count_; }

  int add_count() const { return add_count_; }

  const em::AppInstallReportLogEvent& last_event() const { return last_event_; }

 private:
  int add_for_all_count_ = 0;
  int add_count_ = 0;
  em::AppInstallReportLogEvent last_event_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppInstallEventLogCollectorDelegate);
};

}  // namespace

class AppInstallEventLogCollectorTest : public testing::Test {
 public:
  AppInstallEventLogCollectorTest() = default;
  ~AppInstallEventLogCollectorTest() override = default;

  void SetUp() override {
    std::unique_ptr<chromeos::FakePowerManagerClient> power_manager_client =
        std::make_unique<chromeos::FakePowerManagerClient>();
    power_manager_client_ = power_manager_client.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        std::move(power_manager_client));

    chromeos::DBusThreadManager::Initialize();
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }
  FakeAppInstallEventLogCollectorDelegate* delegate() { return &delegate_; }
  chromeos::FakePowerManagerClient* power_manager_client() {
    return power_manager_client_;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  FakeAppInstallEventLogCollectorDelegate delegate_;
  chromeos::FakePowerManagerClient* power_manager_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogCollectorTest);
};

// Test the case when collector is created and destroyed inside the one user
// session. In this case no event is generated. This happens for example when
// all apps are installed in context of the same user session.
TEST_F(AppInstallEventLogCollectorTest, NoEventsByDefault) {
  std::set<std::string> pending_packages;
  pending_packages.insert("test");

  std::unique_ptr<AppInstallEventLogCollector> collector =
      std::make_unique<AppInstallEventLogCollector>(delegate(), profile(),
                                                    pending_packages);
  collector.reset();

  EXPECT_EQ(0, delegate()->add_count());
  EXPECT_EQ(0, delegate()->add_for_all_count());
}

TEST_F(AppInstallEventLogCollectorTest, LoginLogout) {
  std::set<std::string> pending_packages;
  pending_packages.insert("test");

  std::unique_ptr<AppInstallEventLogCollector> collector =
      std::make_unique<AppInstallEventLogCollector>(delegate(), profile(),
                                                    pending_packages);

  EXPECT_EQ(0, delegate()->add_for_all_count());

  collector->AddLoginEvent();
  EXPECT_EQ(1, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::LOGIN,
            delegate()->last_event().session_state_change_type());

  collector->AddLogoutEvent();
  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::LOGOUT,
            delegate()->last_event().session_state_change_type());

  collector.reset();

  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(0, delegate()->add_count());
}

TEST_F(AppInstallEventLogCollectorTest, LoginTypes) {
  std::set<std::string> pending_packages;
  pending_packages.insert("test");

  {
    AppInstallEventLogCollector collector(delegate(), profile(),
                                          pending_packages);
    collector.AddLoginEvent();
    EXPECT_EQ(1, delegate()->add_for_all_count());
    EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
              delegate()->last_event().event_type());
    EXPECT_EQ(em::AppInstallReportLogEvent::LOGIN,
              delegate()->last_event().session_state_change_type());
  }

  {
    // Check restart. No log is expected.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kLoginUser);
    EXPECT_EQ(1, delegate()->add_for_all_count());
  }

  EXPECT_EQ(0, delegate()->add_count());
}

TEST_F(AppInstallEventLogCollectorTest, SuspendResume) {
  std::set<std::string> pending_packages;
  pending_packages.insert("test");

  std::unique_ptr<AppInstallEventLogCollector> collector =
      std::make_unique<AppInstallEventLogCollector>(delegate(), profile(),
                                                    pending_packages);

  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::SUSPEND,
            delegate()->last_event().session_state_change_type());

  power_manager_client()->SendSuspendDone();
  EXPECT_EQ(2, delegate()->add_for_all_count());
  EXPECT_EQ(em::AppInstallReportLogEvent::SESSION_STATE_CHANGE,
            delegate()->last_event().event_type());
  EXPECT_EQ(em::AppInstallReportLogEvent::RESUME,
            delegate()->last_event().session_state_change_type());

  collector.reset();

  EXPECT_EQ(0, delegate()->add_count());
}

}  // namespace policy
