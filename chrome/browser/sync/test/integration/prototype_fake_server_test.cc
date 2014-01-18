// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

class PrototypeFakeServerTest : public SyncTest {
 public:
  PrototypeFakeServerTest() : SyncTest(SINGLE_CLIENT) {
    UseFakeServer();
  }

  virtual ~PrototypeFakeServerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PrototypeFakeServerTest);
};

// TODO(pvalenzuela): Remove this test when sync_integration_tests is
// transitioned to use the C++ fake server. This test currently exists to
// ensure fake server functionality during development. See Bug 323265.
IN_PROC_BROWSER_TEST_F(PrototypeFakeServerTest, Setup) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
}
