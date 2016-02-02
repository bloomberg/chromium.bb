// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <winternl.h>

#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(ScopedHandleTest, ScopedHandle) {
  // Any illegal error code will do. We just need to test that it is preserved
  // by ScopedHandle to avoid bug 528394.
  const DWORD magic_error = 0x12345678;

  HANDLE handle = ::CreateMutex(nullptr, FALSE, nullptr);
  // Call SetLastError after creating the handle.
  ::SetLastError(magic_error);
  base::win::ScopedHandle handle_holder(handle);
  EXPECT_EQ(magic_error, ::GetLastError());

  // Create a new handle and then set LastError again.
  handle = ::CreateMutex(nullptr, FALSE, nullptr);
  ::SetLastError(magic_error);
  handle_holder.Set(handle);
  EXPECT_EQ(magic_error, ::GetLastError());

  // Create a new handle and then set LastError again.
  handle = ::CreateMutex(nullptr, FALSE, nullptr);
  base::win::ScopedHandle handle_source(handle);
  ::SetLastError(magic_error);
  handle_holder = handle_source.Pass();
  EXPECT_EQ(magic_error, ::GetLastError());
}

// This test requires that the CloseHandle hook be in place.
#if !defined(DISABLE_HANDLE_VERIFIER_HOOKS)
TEST(ScopedHandleTest, ActiveVerifierCloseTracked) {
#if defined(_DEBUG)
  // Handle hooks cause shutdown asserts in Debug on Windows 7. crbug.com/571304
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return;
#endif
  HANDLE handle = ::CreateMutex(nullptr, FALSE, nullptr);
  ASSERT_NE(HANDLE(NULL), handle);
  ASSERT_DEATH({
    base::win::ScopedHandle handle_holder(handle);
    // Calling CloseHandle on a tracked handle should crash.
    ::CloseHandle(handle);
  }, "CloseHandle called on tracked handle.");
}
#endif

TEST(ScopedHandleTest, ActiveVerifierTrackedHasBeenClosed) {
  HANDLE handle = ::CreateMutex(nullptr, FALSE, nullptr);
  ASSERT_NE(HANDLE(NULL), handle);
  typedef NTSTATUS(WINAPI * NtCloseFunc)(HANDLE);
  NtCloseFunc ntclose = reinterpret_cast<NtCloseFunc>(
      GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtClose"));
  ASSERT_NE(nullptr, ntclose);

  ASSERT_DEATH({
    base::win::ScopedHandle handle_holder(handle);
    ntclose(handle);
    // Destructing a ScopedHandle with an illegally closed handle should fail.
  }, "CloseHandle failed.");
}

TEST(ScopedHandleTest, ActiveVerifierDoubleTracking) {
  HANDLE handle = ::CreateMutex(nullptr, FALSE, nullptr);
  ASSERT_NE(HANDLE(NULL), handle);

  base::win::ScopedHandle handle_holder(handle);

  ASSERT_DEATH({
    base::win::ScopedHandle handle_holder2(handle);
  }, "Attempt to start tracking already tracked handle.");
}

TEST(ScopedHandleTest, ActiveVerifierWrongOwner) {
  HANDLE handle = ::CreateMutex(nullptr, FALSE, nullptr);
  ASSERT_NE(HANDLE(NULL), handle);

  base::win::ScopedHandle handle_holder(handle);
  ASSERT_DEATH({
    base::win::ScopedHandle handle_holder2;
    handle_holder2.handle_ = handle;
  }, "Attempting to close a handle not owned by opener.");
  ASSERT_TRUE(handle_holder.IsValid());
  handle_holder.Close();
}

TEST(ScopedHandleTest, ActiveVerifierUntrackedHandle) {
  HANDLE handle = ::CreateMutex(nullptr, FALSE, nullptr);
  ASSERT_NE(HANDLE(NULL), handle);

  ASSERT_DEATH({
    base::win::ScopedHandle handle_holder;
    handle_holder.handle_ = handle;
  }, "Attempting to close an untracked handle.");

  ASSERT_TRUE(::CloseHandle(handle));
}

}  // namespace win
}  // namespace base
