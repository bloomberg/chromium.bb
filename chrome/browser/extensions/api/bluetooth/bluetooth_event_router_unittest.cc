// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_profile.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestExtensionId[] = "test extension id";
const device::BluetoothUUID kAudioProfileUuid("1234");
const device::BluetoothUUID kHealthProfileUuid("4321");

}  // namespace

namespace extensions {

namespace bluetooth = api::bluetooth;

class BluetoothEventRouterTest : public testing::Test {
 public:
  BluetoothEventRouterTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        mock_adapter_(new testing::StrictMock<device::MockBluetoothAdapter>()),
        test_profile_(new TestingProfile()),
        router_(test_profile_.get()) {
    router_.SetAdapterForTest(mock_adapter_);
  }

  virtual void TearDown() OVERRIDE {
    // Some profile-dependent services rely on UI thread to clean up. We make
    // sure they are properly cleaned up by running the UI message loop until
    // idle.
    test_profile_.reset(NULL);
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  // Note: |ui_thread_| must be declared before |router_|.
  content::TestBrowserThread ui_thread_;
  testing::StrictMock<device::MockBluetoothAdapter>* mock_adapter_;
  testing::NiceMock<device::MockBluetoothProfile> mock_audio_profile_;
  testing::NiceMock<device::MockBluetoothProfile> mock_health_profile_;
  scoped_ptr<TestingProfile> test_profile_;
  BluetoothEventRouter router_;
};

TEST_F(BluetoothEventRouterTest, BluetoothEventListener) {
  router_.OnListenerAdded();
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_.OnListenerRemoved();
}

TEST_F(BluetoothEventRouterTest, MultipleBluetoothEventListeners) {
  router_.OnListenerAdded();
  router_.OnListenerAdded();
  router_.OnListenerAdded();
  router_.OnListenerRemoved();
  router_.OnListenerRemoved();
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_.OnListenerRemoved();
}

TEST_F(BluetoothEventRouterTest, Profiles) {
  EXPECT_FALSE(router_.HasProfile(kAudioProfileUuid));
  EXPECT_FALSE(router_.HasProfile(kHealthProfileUuid));

  router_.AddProfile(
      kAudioProfileUuid, kTestExtensionId, &mock_audio_profile_);
  router_.AddProfile(
      kHealthProfileUuid, kTestExtensionId, &mock_health_profile_);
  EXPECT_TRUE(router_.HasProfile(kAudioProfileUuid));
  EXPECT_TRUE(router_.HasProfile(kHealthProfileUuid));

  EXPECT_CALL(mock_audio_profile_, Unregister()).Times(1);
  router_.RemoveProfile(kAudioProfileUuid);
  EXPECT_FALSE(router_.HasProfile(kAudioProfileUuid));
  EXPECT_TRUE(router_.HasProfile(kHealthProfileUuid));

  // Make sure remaining profiles are unregistered in destructor.
  EXPECT_CALL(mock_health_profile_, Unregister()).Times(1);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
}

TEST_F(BluetoothEventRouterTest, UnloadExtension) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
              .Set("name", "BT event router test")
              .Set("version", "1.0")
              .Set("manifest_version", 2))
          .SetID(kTestExtensionId)
          .Build();

  router_.AddProfile(
      kAudioProfileUuid, kTestExtensionId, &mock_audio_profile_);
  router_.AddProfile(
      kHealthProfileUuid, kTestExtensionId, &mock_health_profile_);
  EXPECT_TRUE(router_.HasProfile(kAudioProfileUuid));
  EXPECT_TRUE(router_.HasProfile(kHealthProfileUuid));

  // Unloading the extension should unregister all profiles added by it.
  EXPECT_CALL(mock_audio_profile_, Unregister()).Times(1);
  EXPECT_CALL(mock_health_profile_, Unregister()).Times(1);

  content::NotificationService* notifier =
      content::NotificationService::current();
  UnloadedExtensionInfo details(
      extension, UnloadedExtensionInfo::REASON_DISABLE);
  notifier->Notify(chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                   content::Source<Profile>(test_profile_.get()),
                   content::Details<UnloadedExtensionInfo>(&details));

  EXPECT_FALSE(router_.HasProfile(kAudioProfileUuid));
  EXPECT_FALSE(router_.HasProfile(kHealthProfileUuid));
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
}

}  // namespace extensions
