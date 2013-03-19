// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_update_engine_client.h"

using chromeos::UpdateEngineClient;
using ::testing::Return;
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, GetIncognitoModeAvailability) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(prefs::kIncognitoModeAvailability, 1);

  EXPECT_TRUE(RunComponentExtensionTest(
      "system/get_incognito_mode_availability")) << message_;
}

#if defined(OS_CHROMEOS)

class GetUpdateStatusApiTest : public ExtensionApiTest {
 public:
  GetUpdateStatusApiTest() : mock_update_engine_client_(NULL) {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    chromeos::MockDBusThreadManager* mock_dbus_thread_manager =
        new chromeos::MockDBusThreadManager;
    chromeos::DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    mock_update_engine_client_ =
        mock_dbus_thread_manager->mock_update_engine_client();
    EXPECT_CALL(*mock_update_engine_client_, GetLastStatus())
        .Times(1)
        .WillOnce(Return(chromeos::MockUpdateEngineClient::Status()));
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    chromeos::DBusThreadManager::Shutdown();
    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  chromeos::MockUpdateEngineClient* mock_update_engine_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetUpdateStatusApiTest);
};

IN_PROC_BROWSER_TEST_F(GetUpdateStatusApiTest, Progress) {
  UpdateEngineClient::Status status_not_available;
  status_not_available.status = UpdateEngineClient::UPDATE_STATUS_IDLE;
  UpdateEngineClient::Status status_updating;
  status_updating.status = UpdateEngineClient::UPDATE_STATUS_DOWNLOADING;
  status_updating.download_progress = 0.5;
  UpdateEngineClient::Status status_boot_needed;
  status_boot_needed.status =
      UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;

  EXPECT_CALL(*mock_update_engine_client_, GetLastStatus())
      .WillOnce(Return(status_not_available))
      .WillOnce(Return(status_updating))
      .WillOnce(Return(status_boot_needed));
  ASSERT_TRUE(RunComponentExtensionTest(
      "system/get_update_status")) << message_;
}

#endif
