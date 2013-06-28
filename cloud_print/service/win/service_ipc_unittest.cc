// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "cloud_print/service/win/service_listener.h"
#include "cloud_print/service/win/service_utils.h"
#include "cloud_print/service/win/setup_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ServiceIpcTest, Timeout) {
  SetupListener setup(GetCurrentUserName());
  ASSERT_FALSE(setup.WaitResponce(base::TimeDelta::FromSeconds(3)));
}

TEST(ServiceIpcTest, Sequence) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  SetupListener setup(GetCurrentUserName());
  ServiceListener service(temp_dir.path());
  ASSERT_TRUE(setup.WaitResponce(base::TimeDelta::FromSeconds(30)));
  EXPECT_EQ(setup.user_data_dir(), temp_dir.path());
  EXPECT_EQ(setup.user_name(), GetCurrentUserName());
}

TEST(ServiceIpcTest, ReverseSequence) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ServiceListener service(temp_dir.path());
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  SetupListener setup(GetCurrentUserName());
  ASSERT_TRUE(setup.WaitResponce(base::TimeDelta::FromSeconds(30)));
  EXPECT_EQ(setup.user_data_dir(), temp_dir.path());
  EXPECT_EQ(setup.user_name(), GetCurrentUserName());
}

TEST(ServiceIpcTest, InvaludUser) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  SetupListener setup(L"guest");
  ServiceListener service(temp_dir.path());
  ASSERT_FALSE(setup.WaitResponce(base::TimeDelta::FromSeconds(3)));
}

