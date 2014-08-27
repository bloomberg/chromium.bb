// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/system_salt_getter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// Used as a GetSystemSaltCallback.
void CopySystemSalt(std::string* out_system_salt,
                    const std::string& system_salt) {
  *out_system_salt = system_salt;
}

class SystemSaltGetterTest : public testing::Test {
 protected:
  SystemSaltGetterTest() : fake_cryptohome_client_(NULL) {}

  virtual void SetUp() OVERRIDE {
    fake_cryptohome_client_ = new FakeCryptohomeClient;
    DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        scoped_ptr<CryptohomeClient>(fake_cryptohome_client_));

    EXPECT_FALSE(SystemSaltGetter::IsInitialized());
    SystemSaltGetter::Initialize();
    ASSERT_TRUE(SystemSaltGetter::IsInitialized());
    ASSERT_TRUE(SystemSaltGetter::Get());
  }

  virtual void TearDown() OVERRIDE {
    SystemSaltGetter::Shutdown();
    DBusThreadManager::Shutdown();
  }

  base::MessageLoopForUI message_loop_;
  FakeCryptohomeClient* fake_cryptohome_client_;
};

TEST_F(SystemSaltGetterTest, GetSystemSalt) {
  // Try to get system salt before the service becomes available.
  fake_cryptohome_client_->SetServiceIsAvailable(false);
  std::string system_salt;
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&CopySystemSalt, &system_salt));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(system_salt.empty());  // System salt is not returned yet.

  // Service becomes available.
  fake_cryptohome_client_->SetServiceIsAvailable(true);
  base::RunLoop().RunUntilIdle();
  const std::string expected_system_salt =
      SystemSaltGetter::ConvertRawSaltToHexString(
          FakeCryptohomeClient::GetStubSystemSalt());
  EXPECT_EQ(expected_system_salt, system_salt);  // System salt is returned.
}

}  // namespace
}  // namespace chromeos
