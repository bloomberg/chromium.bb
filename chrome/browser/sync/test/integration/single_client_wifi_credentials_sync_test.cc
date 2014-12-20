// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wifi_credentials_helper.h"
#include "chrome/common/chrome_switches.h"

class SingleClientWifiCredentialsSyncTest : public SyncTest {
 public:
  SingleClientWifiCredentialsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientWifiCredentialsSyncTest() override {}

  void SetUpCommandLine(CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableWifiCredentialSync);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientWifiCredentialsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientWifiCredentialsSyncTest, NoCredentials) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(wifi_credentials_helper::VerifierIsEmpty());
  ASSERT_TRUE(wifi_credentials_helper::ProfileMatchesVerifier(0));
}
