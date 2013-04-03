// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool ServerCallback(int* callback_count,
                    const CommandLine& command_line,
                    const base::FilePath& current_directory) {
  ++(*callback_count);
  return true;
}

bool ClientCallback(const CommandLine& command_line,
                    const base::FilePath& current_directory) {
  ADD_FAILURE();
  return false;
}

}  // namespace

TEST(ProcessSingletonWinTest, Basic) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  int callback_count = 0;

  ProcessSingleton ps1(
      profile_dir.path(),
      base::Bind(&ServerCallback, base::Unretained(&callback_count)));
  ProcessSingleton ps2(profile_dir.path(), base::Bind(&ClientCallback));

  ProcessSingleton::NotifyResult result = ps1.NotifyOtherProcessOrCreate();

  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);
  ASSERT_EQ(0, callback_count);

  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);

  ASSERT_EQ(1, callback_count);
}

#if !defined(USE_AURA)
TEST(ProcessSingletonWinTest, Lock) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  int callback_count = 0;

  ProcessSingleton ps1(
      profile_dir.path(),
      base::Bind(&ServerCallback, base::Unretained(&callback_count)));
  ps1.Lock(NULL);

  ProcessSingleton ps2(profile_dir.path(), base::Bind(&ClientCallback));

  ProcessSingleton::NotifyResult result = ps1.NotifyOtherProcessOrCreate();

  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);
  ASSERT_EQ(0, callback_count);

  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);

  ASSERT_EQ(0, callback_count);
  ps1.Unlock();
  ASSERT_EQ(1, callback_count);
}

class TestableProcessSingleton : public ProcessSingleton {
 public:
  TestableProcessSingleton(const base::FilePath& user_data_dir,
                           const NotificationCallback& notification_callback)
      : ProcessSingleton(user_data_dir, notification_callback),
        called_set_foreground_window_(false) {}

  bool called_set_foreground_window() { return called_set_foreground_window_; }

 protected:
  virtual void DoSetForegroundWindow(HWND target_window) OVERRIDE {
    called_set_foreground_window_ = true;
  }

 private:
  bool called_set_foreground_window_;
};

TEST(ProcessSingletonWinTest, LockWithModalDialog) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  int callback_count = 0;

  TestableProcessSingleton ps1(
      profile_dir.path(),
      base::Bind(&ServerCallback, base::Unretained(&callback_count)));
  ps1.Lock(::GetShellWindow());

  ProcessSingleton ps2(profile_dir.path(), base::Bind(&ClientCallback));

  ProcessSingleton::NotifyResult result = ps1.NotifyOtherProcessOrCreate();

  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);
  ASSERT_EQ(0, callback_count);

  ASSERT_FALSE(ps1.called_set_foreground_window());
  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);
  ASSERT_TRUE(ps1.called_set_foreground_window());

  ASSERT_EQ(0, callback_count);
  ps1.Unlock();
  // When a modal dialog is present, the new command-line invocation is silently
  // dropped.
  ASSERT_EQ(0, callback_count);
}
#endif  // !defined(USE_AURA)
