// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_profile.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

ProfileKeyedService* BuildFakeExtensionSystem(
    content::BrowserContext* profile) {
  return new FakeExtensionSystem(static_cast<Profile*>(profile));
}

}  // namespace

namespace extensions {

class ExtensionBluetoothEventRouterTest : public testing::Test {
 public:
  ExtensionBluetoothEventRouterTest()
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
  ExtensionBluetoothEventRouter router_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(ExtensionBluetoothEventRouterTest, BluetoothEventListener) {
  router_.OnListenerAdded();
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_.OnListenerRemoved();
}

TEST_F(ExtensionBluetoothEventRouterTest, MultipleBluetoothEventListeners) {
  router_.OnListenerAdded();
  router_.OnListenerAdded();
  router_.OnListenerAdded();
  router_.OnListenerRemoved();
  router_.OnListenerRemoved();
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_.OnListenerRemoved();
}

TEST_F(ExtensionBluetoothEventRouterTest, Profiles) {
  EXPECT_FALSE(router_.HasProfile(kAudioProfileUuid));
  EXPECT_FALSE(router_.HasProfile(kHealthProfileUuid));

  router_.AddProfile(kAudioProfileUuid, &mock_audio_profile_);
  router_.AddProfile(kHealthProfileUuid, &mock_health_profile_);
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

TEST_F(ExtensionBluetoothEventRouterTest, DispatchConnectionEvent) {
  router_.AddProfile(kAudioProfileUuid, &mock_audio_profile_);

  FakeExtensionSystem* fake_extension_system =
      static_cast<FakeExtensionSystem*>(ExtensionSystemFactory::GetInstance()->
          SetTestingFactoryAndUse(test_profile_.get(),
                                  &BuildFakeExtensionSystem));

  const char test_extension_id[] = "test extension id";
  testing::NiceMock<device::MockBluetoothDevice> mock_device(
      mock_adapter_, 0, "device name", "device address", true, false);
  scoped_refptr<testing::NiceMock<device::MockBluetoothSocket> > mock_socket(
      new testing::NiceMock<device::MockBluetoothSocket>());

  router_.DispatchConnectionEvent(test_extension_id,
                                  kAudioProfileUuid,
                                  &mock_device,
                                  mock_socket);

  FakeEventRouter* fake_event_router =
      static_cast<FakeEventRouter*>(fake_extension_system->event_router());

  EXPECT_STREQ(test_extension_id, fake_event_router->extension_id().c_str());
  EXPECT_STREQ(event_names::kBluetoothOnConnection,
               fake_event_router->event()->event_name.c_str());

  base::ListValue* event_args = fake_event_router->event()->event_args.get();
  base::DictionaryValue* socket_value = NULL;
  ASSERT_TRUE(event_args->GetDictionary(0, &socket_value));
  int socket_id;
  ASSERT_TRUE(socket_value->GetInteger("id", &socket_id));
  EXPECT_EQ(mock_socket.get(), router_.GetSocket(socket_id).get());

  base::DictionaryValue* profile_value = NULL;
  ASSERT_TRUE(socket_value->GetDictionary("profile", &profile_value));
  std::string profile_uuid;
  ASSERT_TRUE(profile_value->GetString("uuid", &profile_uuid));
  EXPECT_STREQ(kAudioProfileUuid, profile_uuid.c_str());

  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  router_.ReleaseSocket(socket_id);
}

TEST_F(ExtensionBluetoothEventRouterTest, DoNotDispatchConnectionEvent) {
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
