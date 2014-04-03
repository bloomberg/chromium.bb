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
const char kAudioProfileUuid[] = "audio profile uuid";
const char kHealthProfileUuid[] = "health profile uuid";

class FakeEventRouter : public extensions::EventRouter {
 public:
  explicit FakeEventRouter(Profile* profile) : EventRouter(profile, NULL) {}

  virtual void DispatchEventToExtension(
      const std::string& extension_id,
      scoped_ptr<extensions::Event> event) OVERRIDE {
    extension_id_ = extension_id;
    event_ = event.Pass();
  }

  std::string extension_id() const {
    return extension_id_;
  }

  const extensions::Event* event() const {
    return event_.get();
  }

 private:
  std::string extension_id_;
  scoped_ptr<extensions::Event> event_;

  DISALLOW_COPY_AND_ASSIGN(FakeEventRouter);
};

class FakeExtensionSystem : public extensions::TestExtensionSystem {
 public:
  explicit FakeExtensionSystem(Profile* profile)
      : extensions::TestExtensionSystem(profile) {}

  virtual extensions::EventRouter* event_router() OVERRIDE {
    if (!fake_event_router_)
      fake_event_router_.reset(new FakeEventRouter(profile_));
    return fake_event_router_.get();
  }

 private:
  scoped_ptr<FakeEventRouter> fake_event_router_;

  DISALLOW_COPY_AND_ASSIGN(FakeExtensionSystem);
};

KeyedService* BuildFakeExtensionSystem(content::BrowserContext* profile) {
  return new FakeExtensionSystem(static_cast<Profile*>(profile));
}

}  // namespace

namespace extensions {

namespace bluetooth = api::bluetooth;

class BluetoothEventRouterTest : public testing::Test {
 public:
  BluetoothEventRouterTest()
      : mock_adapter_(new testing::StrictMock<device::MockBluetoothAdapter>()),
        test_profile_(new TestingProfile()),
        router_(test_profile_.get()),
        ui_thread_(content::BrowserThread::UI, &message_loop_) {
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
  testing::StrictMock<device::MockBluetoothAdapter>* mock_adapter_;
  testing::NiceMock<device::MockBluetoothProfile> mock_audio_profile_;
  testing::NiceMock<device::MockBluetoothProfile> mock_health_profile_;
  scoped_ptr<TestingProfile> test_profile_;
  BluetoothEventRouter router_;
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
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

TEST_F(BluetoothEventRouterTest, DispatchConnectionEvent) {
  router_.AddProfile(
      kAudioProfileUuid, kTestExtensionId, &mock_audio_profile_);

  FakeExtensionSystem* fake_extension_system =
      static_cast<FakeExtensionSystem*>(ExtensionSystemFactory::GetInstance()->
          SetTestingFactoryAndUse(test_profile_.get(),
                                  &BuildFakeExtensionSystem));

  testing::NiceMock<device::MockBluetoothDevice> mock_device(
      mock_adapter_, 0, "device name", "device address", true, false);
  scoped_refptr<testing::NiceMock<device::MockBluetoothSocket> > mock_socket(
      new testing::NiceMock<device::MockBluetoothSocket>());

  router_.DispatchConnectionEvent(kTestExtensionId,
                                  kAudioProfileUuid,
                                  &mock_device,
                                  mock_socket);

  FakeEventRouter* fake_event_router =
      static_cast<FakeEventRouter*>(fake_extension_system->event_router());

  EXPECT_STREQ(kTestExtensionId, fake_event_router->extension_id().c_str());
  EXPECT_STREQ(bluetooth::OnConnection::kEventName,
               fake_event_router->event()->event_name.c_str());

  base::ListValue* event_args = fake_event_router->event()->event_args.get();
  base::DictionaryValue* socket_value = NULL;
  ASSERT_TRUE(event_args->GetDictionary(0, &socket_value));
  int socket_id;
  ASSERT_TRUE(socket_value->GetInteger("id", &socket_id));
  EXPECT_EQ(mock_socket.get(), router_.GetSocket(socket_id).get());

  base::DictionaryValue* profile_value = NULL;
  ASSERT_TRUE(socket_value->GetDictionary("profile", &profile_value));
  std::string uuid;
  ASSERT_TRUE(profile_value->GetString("uuid", &uuid));
  EXPECT_STREQ(kAudioProfileUuid, uuid.c_str());

  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_.ReleaseSocket(socket_id);
}

TEST_F(BluetoothEventRouterTest, DoNotDispatchConnectionEvent) {
  FakeExtensionSystem* fake_extension_system =
      static_cast<FakeExtensionSystem*>(ExtensionSystemFactory::GetInstance()->
          SetTestingFactoryAndUse(test_profile_.get(),
                                  &BuildFakeExtensionSystem));
  testing::NiceMock<device::MockBluetoothDevice> mock_device(
      mock_adapter_, 0, "device name", "device address", true, false);
  scoped_refptr<testing::NiceMock<device::MockBluetoothSocket> > mock_socket(
      new testing::NiceMock<device::MockBluetoothSocket>());

  // Connection event won't be dispatched for non-registered profiles.
  router_.DispatchConnectionEvent("test extension id",
                                  kAudioProfileUuid,
                                  &mock_device,
                                  mock_socket);
  FakeEventRouter* fake_event_router =
      static_cast<FakeEventRouter*>(fake_extension_system->event_router());
  EXPECT_TRUE(fake_event_router->event() == NULL);

  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
}

}  // namespace extensions
