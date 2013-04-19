// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_singleton.h"

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

TEST(ChromeProcessSingletonTest, Basic) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  int callback_count = 0;

  ChromeProcessSingleton ps1(
      profile_dir.path(),
      base::Bind(&ServerCallback, base::Unretained(&callback_count)));
  ps1.Unlock();

  ChromeProcessSingleton ps2(profile_dir.path(), base::Bind(&ClientCallback));
  ps2.Unlock();

  ProcessSingleton::NotifyResult result = ps1.NotifyOtherProcessOrCreate();

  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);
  ASSERT_EQ(0, callback_count);

  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);

  ASSERT_EQ(1, callback_count);
}

TEST(ChromeProcessSingletonTest, Lock) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  int callback_count = 0;

  ChromeProcessSingleton ps1(
      profile_dir.path(),
      base::Bind(&ServerCallback, base::Unretained(&callback_count)));

  ChromeProcessSingleton ps2(profile_dir.path(), base::Bind(&ClientCallback));
  ps2.Unlock();

  ProcessSingleton::NotifyResult result = ps1.NotifyOtherProcessOrCreate();

  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);
  ASSERT_EQ(0, callback_count);

  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);

  ASSERT_EQ(0, callback_count);
  ps1.Unlock();
  ASSERT_EQ(1, callback_count);
}

#if defined(OS_WIN) && !defined(USE_AURA)
namespace {

void SetForegroundWindowHandler(bool* flag,
                                gfx::NativeWindow /* target_window */) {
  *flag = true;
}

}  // namespace

TEST(ChromeProcessSingletonTest, LockWithModalDialog) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  int callback_count = 0;
  bool called_set_foreground_window = false;

  ChromeProcessSingleton ps1(
      profile_dir.path(),
      base::Bind(&ServerCallback, base::Unretained(&callback_count)),
      base::Bind(&SetForegroundWindowHandler,
                 base::Unretained(&called_set_foreground_window)));
  ps1.SetActiveModalDialog(::GetShellWindow());

  ChromeProcessSingleton ps2(profile_dir.path(), base::Bind(&ClientCallback));
  ps2.Unlock();

  ProcessSingleton::NotifyResult result = ps1.NotifyOtherProcessOrCreate();

  ASSERT_EQ(ProcessSingleton::PROCESS_NONE, result);
  ASSERT_EQ(0, callback_count);

  ASSERT_FALSE(called_set_foreground_window);
  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);
  ASSERT_TRUE(called_set_foreground_window);

  ASSERT_EQ(0, callback_count);
  ps1.SetActiveModalDialog(NULL);
  ps1.Unlock();
  // The notification sent while a modal dialog was present was silently
  // dropped.
  ASSERT_EQ(0, callback_count);

  // But now that the active modal dialog is NULL notifications will be handled.
  result = ps2.NotifyOtherProcessOrCreate();
  ASSERT_EQ(ProcessSingleton::PROCESS_NOTIFIED, result);
  ASSERT_EQ(1, callback_count);
}
#endif  // defined(OS_WIN) && !defined(USE_AURA)
