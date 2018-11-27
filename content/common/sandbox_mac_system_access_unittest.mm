// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "content/common/sandbox_mac_unittest_helper.h"
#include "crypto/openssl_util.h"
#include "services/service_manager/sandbox/mac/sandbox_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#import "ui/base/clipboard/clipboard_util_mac.h"

namespace content {

//--------------------- OpenSSL Sandboxing ----------------------
// Test case for checking sandboxing of OpenSSL initialization.
class MacSandboxedOpenSSLTestCase : public MacSandboxTestCase {
 public:
  bool SandboxedTest() override;
};

REGISTER_SANDBOX_TEST_CASE(MacSandboxedOpenSSLTestCase);

bool MacSandboxedOpenSSLTestCase::SandboxedTest() {
  crypto::EnsureOpenSSLInit();

  // Ensure that RAND_bytes is functional within the sandbox.
  uint8_t byte;
  return RAND_bytes(&byte, 1) == 1;
}

TEST_F(MacSandboxTest, OpenSSLAccess) {
  EXPECT_TRUE(RunTestInAllSandboxTypes("MacSandboxedOpenSSLTestCase", NULL));
}

}  // namespace content
