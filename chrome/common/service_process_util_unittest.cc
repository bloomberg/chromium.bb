// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/service_process_util.h"
#include "testing/gtest/include/gtest/gtest.h"


TEST(ServiceProcessUtilTest, ScopedVersionedName) {
  std::string test_str = "test";
  std::string scoped_name = GetServiceProcessScopedVersionedName(test_str);
  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());
  EXPECT_TRUE(EndsWith(scoped_name, test_str, true));
  EXPECT_NE(std::string::npos, scoped_name.find(version_info.Version()));
}

#if defined(OS_WIN)
// Singleton-ness is only implemented on Windows.
TEST(ServiceProcessStateTest, Singleton) {
  ServiceProcessState state;
  EXPECT_TRUE(state.Initialize());
  // The second instance should fail to Initialize.
  ServiceProcessState another_state;
  EXPECT_FALSE(another_state.Initialize());
}
#endif  // defined(OS_WIN)

TEST(ServiceProcessStateTest, ReadyState) {
#if defined(OS_WIN)
  // On Posix, we use a lock file on disk to signal readiness. This lock file
  // could be lying around from previous crashes which could cause
  // CheckServiceProcessReady to lie. On Windows, we use a named event so we
  // don't have this issue. Until we have a more stable signalling mechanism on
  // Posix, this check will only execute on Windows.
  EXPECT_FALSE(CheckServiceProcessReady());
#endif  // defined(OS_WIN)
  ServiceProcessState state;
  EXPECT_TRUE(state.Initialize());
  state.SignalReady(NULL);
  EXPECT_TRUE(CheckServiceProcessReady());
  state.SignalStopped();
  EXPECT_FALSE(CheckServiceProcessReady());
}

TEST(ServiceProcessStateTest, SharedMem) {
#if defined(OS_WIN)
  // On Posix, named shared memory uses a file on disk. This file
  // could be lying around from previous crashes which could cause
  // GetServiceProcessPid to lie. On Windows, we use a named event so we
  // don't have this issue. Until we have a more stable shared memory
  // implementation on Posix, this check will only execute on Windows.
  EXPECT_EQ(0, GetServiceProcessPid());
#endif  // defined(OS_WIN)
  ServiceProcessState state;
  EXPECT_TRUE(state.Initialize());
  EXPECT_EQ(base::GetCurrentProcId(), GetServiceProcessPid());
}

