// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::AtLeast;
using ::testing::Return;

namespace chromeos {

namespace {

void CheckNotification(VersionUpdater::Status /* status */,
                       int /* progress */,
                       const base::string16& /* message */) {
}

}  // namespace

class VersionUpdaterCrosTest : public ::testing::Test {
 protected:
  VersionUpdaterCrosTest()
      : version_updater_(VersionUpdater::Create()),
        fake_update_engine_client_(NULL),
        mock_user_manager_(new MockUserManager()),
        user_manager_enabler_(mock_user_manager_) {}

  virtual ~VersionUpdaterCrosTest() {}

  virtual void SetUp() OVERRIDE {
    fake_update_engine_client_ = new FakeUpdateEngineClient();
    scoped_ptr<DBusThreadManagerSetter> dbus_setter =
        DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetUpdateEngineClient(
        scoped_ptr<UpdateEngineClient>(fake_update_engine_client_).Pass());

    EXPECT_CALL(*mock_user_manager_, IsCurrentUserOwner())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_user_manager_, Shutdown()).Times(AtLeast(0));

    DeviceSettingsService::Initialize();
    CrosSettings::Initialize();

    NetworkHandler::Initialize();
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->AddService("/service/eth",
                             "eth" /* guid */,
                             "eth",
                             shill::kTypeEthernet, shill::kStateOnline,
                             true /* visible */);
    loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    NetworkHandler::Shutdown();

    CrosSettings::Shutdown();
    DeviceSettingsService::Shutdown();
  }

  scoped_ptr<VersionUpdater> version_updater_;
  FakeUpdateEngineClient* fake_update_engine_client_;  // Not owned.

  MockUserManager* mock_user_manager_;  // Not owned.
  ScopedUserManagerEnabler user_manager_enabler_;

  base::MessageLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterCrosTest);
};

// The test checks following behaviour:
// 1. The device is currently on the dev channel and an user decides to switch
// to the beta channel.
// 2. In the middle of channel switch the user decides to switch to the stable
// channel.
// 3. Update engine reports an error because downloading channel (beta) is not
// equal
// to the target channel (stable).
// 4. When update engine becomes idle downloading of the stable channel is
// initiated.
TEST_F(VersionUpdaterCrosTest, TwoOverlappingSetChannelRequests) {
  version_updater_->SetChannel("beta-channel", true);

  {
    UpdateEngineClient::Status status;
    status.status = UpdateEngineClient::UPDATE_STATUS_IDLE;
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  EXPECT_EQ(0, fake_update_engine_client_->request_update_check_call_count());

  // IDLE -> DOWNLOADING transition after update check.
  version_updater_->CheckForUpdate(base::Bind(&CheckNotification));
  EXPECT_EQ(1, fake_update_engine_client_->request_update_check_call_count());

  {
    UpdateEngineClient::Status status;
    status.status = UpdateEngineClient::UPDATE_STATUS_DOWNLOADING;
    status.download_progress = 0.1;
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  version_updater_->SetChannel("stable-channel", true);

  // DOWNLOADING -> REPORTING_ERROR_EVENT transition since target channel is not
  // equal to downloading channel now.
  {
    UpdateEngineClient::Status status;
    status.status = UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT;
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  version_updater_->CheckForUpdate(base::Bind(&CheckNotification));
  EXPECT_EQ(1, fake_update_engine_client_->request_update_check_call_count());

  // REPORTING_ERROR_EVENT -> IDLE transition, update check should be
  // automatically scheduled.
  {
    UpdateEngineClient::Status status;
    status.status = UpdateEngineClient::UPDATE_STATUS_IDLE;
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  EXPECT_EQ(2, fake_update_engine_client_->request_update_check_call_count());
}

}  // namespace chromeos
