// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
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

static KeyedService* ApiResourceManagerTestFactory(
    content::BrowserContext* context) {
  content::BrowserThread::ID id;
  CHECK(content::BrowserThread::GetCurrentThreadIdentifier(&id));
  return extensions::ApiResourceManager<
      extensions::BluetoothApiSocket>::CreateApiResourceManagerForTest(context,
                                                                       id);
}

static KeyedService* BluetoothAPITestFactory(content::BrowserContext* context) {
  content::BrowserThread::ID id;
  CHECK(content::BrowserThread::GetCurrentThreadIdentifier(&id));
  return new extensions::BluetoothAPI(context);
}

class FakeEventRouter : public extensions::EventRouter {
 public:
  explicit FakeEventRouter(Profile* profile) : EventRouter(profile, NULL) {}

  virtual void DispatchEventToExtension(const std::string& extension_id,
                                        scoped_ptr<extensions::Event> event)
      OVERRIDE {
    extension_id_ = extension_id;
    event_ = event.Pass();
  }

  std::string extension_id() const { return extension_id_; }

  const extensions::Event* event() const { return event_.get(); }

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

class ExtensionBluetoothApiTest : public testing::Test {
 public:
  ExtensionBluetoothApiTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        mock_adapter_(new testing::StrictMock<device::MockBluetoothAdapter>()),
        test_profile_(NULL) {}

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    test_profile_ = profile_manager_.CreateTestingProfile("test");
    ExtensionSystemFactory::GetInstance()->SetTestingFactoryAndUse(
        test_profile_, &BuildFakeExtensionSystem);
    ApiResourceManager<BluetoothApiSocket>::GetFactoryInstance()
        ->SetTestingFactoryAndUse(test_profile_, ApiResourceManagerTestFactory);
    BluetoothAPI::GetFactoryInstance()->SetTestingFactoryAndUse(
        test_profile_, BluetoothAPITestFactory);
    BluetoothAPI::Get(test_profile_)->event_router()->SetAdapterForTest(
        mock_adapter_);
  }

  virtual void TearDown() OVERRIDE {
    // Some profile-dependent services rely on UI thread to clean up. We make
    // sure they are properly cleaned up by running the UI message loop until
    // idle.
    test_profile_ = NULL;
    profile_manager_.DeleteTestingProfile("test");
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  scoped_refptr<testing::StrictMock<device::MockBluetoothAdapter> >
      mock_adapter_;
  testing::NiceMock<device::MockBluetoothProfile> mock_audio_profile_;
  testing::NiceMock<device::MockBluetoothProfile> mock_health_profile_;
  // Profiles are weak pointers, owned by |profile_manager_|.
  TestingProfile* test_profile_;
};

TEST_F(ExtensionBluetoothApiTest, DispatchConnectionEvent) {
  testing::NiceMock<device::MockBluetoothDevice> mock_device(
      mock_adapter_.get(), 0, "device name", "device address", true, false);
  EXPECT_CALL(*mock_adapter_, GetDevice(mock_device.GetAddress()))
      .WillOnce(testing::Return(&mock_device));
  scoped_refptr<testing::NiceMock<device::MockBluetoothSocket> > mock_socket(
      new testing::NiceMock<device::MockBluetoothSocket>());

  BluetoothAPI::Get(test_profile_)->event_router()->AddProfile(
      kAudioProfileUuid, kTestExtensionId, &mock_audio_profile_);

  BluetoothAPI::Get(test_profile_)->DispatchConnectionEvent(
      kTestExtensionId, kAudioProfileUuid, &mock_device, mock_socket);
  // Connection events are dispatched using a couple of PostTask to the UI
  // thread. Waiting until idle ensures the event is dispatched to the
  // receiver(s).
  base::RunLoop().RunUntilIdle();

  FakeEventRouter* fake_event_router = static_cast<FakeEventRouter*>(
      ExtensionSystem::Get(test_profile_)->event_router());

  ASSERT_TRUE(fake_event_router->event());
  EXPECT_STREQ(kTestExtensionId, fake_event_router->extension_id().c_str());
  EXPECT_STREQ(bluetooth::OnConnection::kEventName,
               fake_event_router->event()->event_name.c_str());

  base::ListValue* event_args = fake_event_router->event()->event_args.get();
  base::DictionaryValue* socket_value = NULL;
  ASSERT_TRUE(event_args->GetDictionary(0, &socket_value));
  int socket_id;
  ASSERT_TRUE(socket_value->GetInteger("id", &socket_id));

  ASSERT_TRUE(BluetoothAPI::Get(test_profile_)->socket_data()->Get(
      kTestExtensionId, socket_id));

  std::string uuid;
  ASSERT_TRUE(socket_value->GetString("uuid", &uuid));
  EXPECT_STREQ(kAudioProfileUuid.canonical_value().c_str(), uuid.c_str());

  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
  BluetoothAPI::Get(test_profile_)->socket_data()->Remove(kTestExtensionId,
                                                          socket_id);
}

TEST_F(ExtensionBluetoothApiTest, DoNotDispatchConnectionEvent) {
  testing::NiceMock<device::MockBluetoothDevice> mock_device(
      mock_adapter_.get(), 0, "device name", "device address", true, false);
  scoped_refptr<testing::NiceMock<device::MockBluetoothSocket> > mock_socket(
      new testing::NiceMock<device::MockBluetoothSocket>());

  // Connection event won't be dispatched for non-registered profiles.
  BluetoothAPI::Get(test_profile_)->DispatchConnectionEvent(
      kTestExtensionId, kAudioProfileUuid, &mock_device, mock_socket);
  // Connection events are dispatched using a couple of PostTask to the UI
  // thread. Waiting until idle ensures the event is dispatched to the
  // receiver(s).
  base::RunLoop().RunUntilIdle();

  FakeEventRouter* fake_event_router = static_cast<FakeEventRouter*>(
      ExtensionSystem::Get(test_profile_)->event_router());
  EXPECT_TRUE(fake_event_router->event() == NULL);

  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_)).Times(1);
}

}  // namespace extensions
