// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// Copies the token service and increments the counter.
void CopyTokenServiceAndCount(
    chromeos::DeviceOAuth2TokenService** out_token_service,
    int* counter,
    chromeos::DeviceOAuth2TokenService* in_token_service) {
  *out_token_service = in_token_service;
  ++(*counter);
}

// Sets up and tears down DeviceOAuth2TokenServiceFactory and its
// dependencies. Also exposes FakeDBusThreadManager.
class ScopedDeviceOAuth2TokenServiceFactorySetUp {
 public:
  ScopedDeviceOAuth2TokenServiceFactorySetUp()
      : fake_cryptohome_client_(new FakeCryptohomeClient) {
    FakeDBusThreadManager* fake_dbus_manager = new FakeDBusThreadManager;
    fake_dbus_manager->SetCryptohomeClient(
        scoped_ptr<CryptohomeClient>(fake_cryptohome_client_));
    DBusThreadManager::InitializeForTesting(fake_dbus_manager);
    SystemSaltGetter::Initialize();
    DeviceOAuth2TokenServiceFactory::Initialize();
  }

  ~ScopedDeviceOAuth2TokenServiceFactorySetUp() {
    DeviceOAuth2TokenServiceFactory::Shutdown();
    SystemSaltGetter::Shutdown();
    DBusThreadManager::Shutdown();
  }

  FakeCryptohomeClient* fake_cryptohome_client() {
    return fake_cryptohome_client_;
  }

 private:
  FakeCryptohomeClient* fake_cryptohome_client_;
};

}  // namespace

class DeviceOAuth2TokenServiceFactoryTest : public testing::Test {
 protected:
  content::TestBrowserThreadBundle thread_bundle_;
};

// Test a case where Get() is called before the factory is initialized.
TEST_F(DeviceOAuth2TokenServiceFactoryTest, Get_Uninitialized) {
  DeviceOAuth2TokenService* token_service = NULL;
  int counter = 0;
  DeviceOAuth2TokenServiceFactory::Get(
      base::Bind(&CopyTokenServiceAndCount, &token_service, &counter));
  // The callback will be run asynchronously.
  EXPECT_EQ(0, counter);
  EXPECT_FALSE(token_service);

  // This lets the factory run the callback with NULL.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);
  EXPECT_FALSE(token_service);
}

// Test a case where Get() is called from only one caller.
TEST_F(DeviceOAuth2TokenServiceFactoryTest, Get_Simple) {
  ScopedDeviceOAuth2TokenServiceFactorySetUp scoped_setup;

  DeviceOAuth2TokenService* token_service = NULL;
  int counter = 0;
  DeviceOAuth2TokenServiceFactory::Get(
      base::Bind(&CopyTokenServiceAndCount, &token_service, &counter));
  // The callback will be run asynchronously.
  EXPECT_EQ(0, counter);
  EXPECT_FALSE(token_service);

  // This lets FakeCryptohomeClient return the system salt.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);
  EXPECT_TRUE(token_service);

  // Call Get() again, and confirm it works.
  token_service = NULL;
  DeviceOAuth2TokenServiceFactory::Get(
      base::Bind(&CopyTokenServiceAndCount, &token_service, &counter));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, counter);
  EXPECT_TRUE(token_service);
}

// Test a case where Get() is called from multiple callers, before the token
// service is ready, and confirm that the callback of every caller is run.
TEST_F(DeviceOAuth2TokenServiceFactoryTest, Get_MultipleCallers) {
  ScopedDeviceOAuth2TokenServiceFactorySetUp scoped_setup;

  DeviceOAuth2TokenService* token_service1 = NULL;
  DeviceOAuth2TokenService* token_service2 = NULL;
  DeviceOAuth2TokenService* token_service3 = NULL;
  int counter = 0;
  DeviceOAuth2TokenServiceFactory::Get(
      base::Bind(&CopyTokenServiceAndCount, &token_service1, &counter));
  DeviceOAuth2TokenServiceFactory::Get(
      base::Bind(&CopyTokenServiceAndCount, &token_service2, &counter));
  DeviceOAuth2TokenServiceFactory::Get(
      base::Bind(&CopyTokenServiceAndCount, &token_service3, &counter));
  // The token service will be returned asynchronously.
  EXPECT_EQ(0, counter);
  EXPECT_FALSE(token_service1);
  EXPECT_FALSE(token_service2);
  EXPECT_FALSE(token_service3);

  // This lets FakeCryptohomeClient return the system salt.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, counter);
  EXPECT_TRUE(token_service1);
  EXPECT_TRUE(token_service2);
  EXPECT_TRUE(token_service3);

  // Make sure that token_service1,2,3 are the same one.
  EXPECT_EQ(token_service1, token_service2);
  EXPECT_EQ(token_service1, token_service3);
}

// Test a case where it failed to obtain the system salt.
TEST_F(DeviceOAuth2TokenServiceFactoryTest, Get_NoSystemSalt) {
  ScopedDeviceOAuth2TokenServiceFactorySetUp scoped_setup;
  scoped_setup.fake_cryptohome_client()->
      set_system_salt(std::vector<uint8>());

  DeviceOAuth2TokenService* token_service = NULL;
  int counter = 0;
  DeviceOAuth2TokenServiceFactory::Get(
      base::Bind(&CopyTokenServiceAndCount, &token_service, &counter));
  // The callback will be run asynchronously.
  EXPECT_EQ(0, counter);
  EXPECT_FALSE(token_service);

  // This lets FakeCryptohomeClient return the system salt, which is empty.
  // NULL should be returned to the callback in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);
  EXPECT_FALSE(token_service);

  // Try it again, but the result should remain the same (NULL returned).
  DeviceOAuth2TokenServiceFactory::Get(
      base::Bind(&CopyTokenServiceAndCount, &token_service, &counter));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, counter);
  EXPECT_FALSE(token_service);
}

}  // namespace chromeos
