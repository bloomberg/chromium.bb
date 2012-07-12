// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "chrome/service/service_process_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ServiceProcessPrefsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    message_loop_proxy_ = base::MessageLoopProxy::current();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    prefs_.reset(new ServiceProcessPrefs(
        temp_dir_.path().AppendASCII("service_process_prefs.txt"),
        message_loop_proxy_.get()));
  }

  // The path to temporary directory used to contain the test operations.
  ScopedTempDir temp_dir_;
  // A message loop that we can use as the file thread message loop.
  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_ptr<ServiceProcessPrefs> prefs_;
};

// Test ability to retrieve prefs
TEST_F(ServiceProcessPrefsTest, RetrievePrefs) {
  prefs_->SetBoolean("testb", true);
  prefs_->SetString("tests", "testvalue");
  prefs_->WritePrefs();
  MessageLoop::current()->RunAllPending();
  prefs_->SetBoolean("testb", false);   // overwrite
  prefs_->SetString("tests", "");   // overwrite
  prefs_->ReadPrefs();
  bool testb;
  prefs_->GetBoolean("testb", &testb);
  EXPECT_EQ(testb, true);
  std::string tests;
  prefs_->GetString("tests", &tests);
  EXPECT_EQ(tests, "testvalue");
}

