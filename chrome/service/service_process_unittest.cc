// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base64.h"
#include "base/crypto/rsa_private_key.h"
#include "base/message_loop.h"
#include "base/waitable_event.h"
#include "chrome/service/service_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ServiceProcessTest, Run) {
  MessageLoopForUI main_message_loop;
  ServiceProcess process;
  EXPECT_TRUE(process.Initialize(&main_message_loop));
  EXPECT_TRUE(process.Teardown());
}

#if defined(ENABLE_REMOTING)
// TODO(hclam): This method should be made into a util.
static std::string GeneratePrivateKey() {
  scoped_ptr<base::RSAPrivateKey> key(base::RSAPrivateKey::Create(2048));

  std::vector<uint8> private_key_buf;
  key->ExportPrivateKey(&private_key_buf);
  std::string private_key_str(private_key_buf.begin(), private_key_buf.end());
  std::string private_key_base64;
  base::Base64Encode(private_key_str, &private_key_base64);
  return private_key_base64;
}

// This test seems to break randomly so disabling it.
TEST(ServiceProcessTest, DISABLED_RunChromoting) {
  MessageLoopForUI main_message_loop;
  ServiceProcess process;
  EXPECT_TRUE(process.Initialize(&main_message_loop));

  // Then config the chromoting host and start it.
  process.SaveChromotingConfig("hello", "world", "it's a", "good day",
                               GeneratePrivateKey());
  EXPECT_TRUE(process.StartChromotingHost());
  EXPECT_TRUE(process.ShutdownChromotingHost());
  EXPECT_TRUE(process.Teardown());
}

class MockServiceProcess : public ServiceProcess {
 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessTest, RunChromotingUntilShutdown);
  MOCK_METHOD0(OnChromotingHostShutdown, void());
};

ACTION_P(QuitMessageLoop, message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

TEST(ServiceProcessTest, DISABLED_RunChromotingUntilShutdown) {
  MessageLoopForUI main_message_loop;
  MockServiceProcess process;
  EXPECT_TRUE(process.Initialize(&main_message_loop));

  // Expect chromoting shutdown be called because the login token is invalid.
  EXPECT_CALL(process, OnChromotingHostShutdown())
      .WillOnce(QuitMessageLoop(&main_message_loop));

  // Then config the chromoting host and start it.
  process.SaveChromotingConfig("hello", "world", "it's a", "good day",
                               GeneratePrivateKey());
  EXPECT_TRUE(process.StartChromotingHost());
  MessageLoop::current()->Run();

  EXPECT_TRUE(process.Teardown());
}
#endif  // defined(ENABLE_REMOTING)
