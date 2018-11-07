// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_win.h"

#include <Windows.h>

#include "base/process/process.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// It has been observed that calling
// :SetThreadPriority(THREAD_MODE_BACKGROUND_BEGIN) in a IDLE_PRIORITY_CLASS
// process has no effect on the return value of ::GetThreadPriority() or on the
// base priority reported in Process Explorer on Windows 8+. It does however set
// the memory and I/O priorities to very low. This test confirms that behavior
// which we suspect is a Windows kernel bug. If this test starts failing, the
// mitigation for https://crbug.com/901483 in
// PlatformThread::SetCurrentThreadPriority() should be revisited.
TEST(PlatformThreadWinTest, SetBackgroundThreadModeFailsInIdlePriorityProcess) {
  PlatformThreadHandle::Handle thread_handle =
      PlatformThread::CurrentHandle().platform_handle();

  // ::GetThreadPriority() is NORMAL. Memory priority is NORMAL.
  // Note: There is no practical way to verify the I/O priority.
  EXPECT_EQ(THREAD_PRIORITY_NORMAL, ::GetThreadPriority(thread_handle));
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_NORMAL);

  // Set the process priority to IDLE.
  // Note: Do not use Process::SetProcessBackgrounded() because it uses
  // PROCESS_MODE_BACKGROUND_BEGIN instead of IDLE_PRIORITY_CLASS when
  // the target process is the current process.
  EXPECT_EQ(static_cast<DWORD>(NORMAL_PRIORITY_CLASS),
            ::GetPriorityClass(Process::Current().Handle()));
  ::SetPriorityClass(Process::Current().Handle(), IDLE_PRIORITY_CLASS);

  // GetThreadPriority() stays NORMAL. Memory priority stays NORMAL.
  EXPECT_EQ(THREAD_PRIORITY_NORMAL, ::GetThreadPriority(thread_handle));
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_NORMAL);

  // Begin thread mode background.
  EXPECT_TRUE(::SetThreadPriority(thread_handle, THREAD_MODE_BACKGROUND_BEGIN));

  // GetThreadPriority() becomes IDLE on Win 7 but stays normal on Win 8+.
  // Memory priority becomes VERY_LOW.
  //
  // Note: this documents the aforementioned Windows 8+ kernel bug. Ideally this
  // would *not* be the case.
  if (win::GetVersion() == win::VERSION_WIN7)
    EXPECT_EQ(THREAD_PRIORITY_IDLE, ::GetThreadPriority(thread_handle));
  else
    EXPECT_EQ(THREAD_PRIORITY_NORMAL, ::GetThreadPriority(thread_handle));
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_VERY_LOW);

  // Set the process priority to NORMAL.
  ::SetPriorityClass(Process::Current().Handle(), NORMAL_PRIORITY_CLASS);

  // GetThreadPriority() stays NORMAL on Win 8+. Memory priority stays VERY_LOW.
  if (win::GetVersion() == win::VERSION_WIN7)
    EXPECT_EQ(THREAD_PRIORITY_IDLE, ::GetThreadPriority(thread_handle));
  else
    EXPECT_EQ(THREAD_PRIORITY_NORMAL, ::GetThreadPriority(thread_handle));
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_VERY_LOW);

  // End thread mode background.
  //
  // Note: at least "ending" the semi-enforced background mode works...
  EXPECT_TRUE(::SetThreadPriority(thread_handle, THREAD_MODE_BACKGROUND_END));

  // GetThreadPriority() stays NORMAL. Memory priority becomes NORMAL.
  EXPECT_EQ(THREAD_PRIORITY_NORMAL, ::GetThreadPriority(thread_handle));
  internal::AssertMemoryPriority(thread_handle, MEMORY_PRIORITY_NORMAL);
}

}  // namespace base
