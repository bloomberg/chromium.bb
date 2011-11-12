// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/crash_reporting/nt_loader.h"

#include <tlhelp32.h>
#include <winnt.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "chrome_frame/crash_reporting/crash_dll.h"
#include "gtest/gtest.h"

namespace {
void AssertIsCriticalSection(CRITICAL_SECTION* critsec) {
  // Assert on some of the internals of the debug info if it has one.
  RTL_CRITICAL_SECTION_DEBUG* debug = critsec->DebugInfo;
  if (debug) {
    ASSERT_EQ(RTL_CRITSECT_TYPE, debug->Type);
    ASSERT_EQ(critsec, debug->CriticalSection);
  }

  // TODO(siggi): assert on the semaphore handle & object type?
}

class ScopedEnterCriticalSection {
 public:
  explicit ScopedEnterCriticalSection(CRITICAL_SECTION* critsec)
      : critsec_(critsec) {
    ::EnterCriticalSection(critsec_);
  }

  ~ScopedEnterCriticalSection() {
    ::LeaveCriticalSection(critsec_);
  }

 private:
  CRITICAL_SECTION* critsec_;
};

std::wstring FromUnicodeString(const UNICODE_STRING& str) {
  return std::wstring(str.Buffer, str.Length / sizeof(str.Buffer[0]));
}

}  // namespace

using namespace nt_loader;

TEST(NtLoader, OwnsCriticalSection) {
  // Use of Thread requires an atexit manager.
  base::AtExitManager at_exit;

  CRITICAL_SECTION cs = {};
  ::InitializeCriticalSection(&cs);
  EXPECT_FALSE(OwnsCriticalSection(&cs));

  // Enter the critsec and assert we own it.
  {
    ScopedEnterCriticalSection lock1(&cs);

    EXPECT_TRUE(OwnsCriticalSection(&cs));

    // Re-enter the critsec and assert we own it.
    ScopedEnterCriticalSection lock2(&cs);

    EXPECT_TRUE(OwnsCriticalSection(&cs));
  }

  // Should no longer own it.
  EXPECT_FALSE(OwnsCriticalSection(&cs));

  // Make another thread grab it.
  base::Thread other("Other threads");
  ASSERT_TRUE(other.Start());
  other.message_loop()->PostTask(
      FROM_HERE, base::Bind(::EnterCriticalSection, &cs));

  base::win::ScopedHandle event(::CreateEvent(NULL, FALSE, FALSE, NULL));
  other.message_loop()->PostTask(
      FROM_HERE, base::IgnoreReturn<BOOL>(base::Bind(::SetEvent, event.Get())));

  ASSERT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(event.Get(), INFINITE));

  // We still shouldn't own it - the other thread does.
  EXPECT_FALSE(OwnsCriticalSection(&cs));
  // And we shouldn't be able to enter it.
  EXPECT_EQ(0, ::TryEnterCriticalSection(&cs));

  // Make the other thread release it.
  other.message_loop()->PostTask(
      FROM_HERE, base::Bind(::LeaveCriticalSection, &cs));

  other.Stop();

  ::DeleteCriticalSection(&cs);
}

TEST(NtLoader, GetLoaderLock) {
  CRITICAL_SECTION* loader_lock = GetLoaderLock();

  AssertIsCriticalSection(loader_lock);

  // We should be able to enter and leave the loader's lock without trouble.
  EnterCriticalSection(loader_lock);
  LeaveCriticalSection(loader_lock);
}

TEST(NtLoader, OwnsLoaderLock) {
  CRITICAL_SECTION* loader_lock = GetLoaderLock();

  EXPECT_FALSE(OwnsLoaderLock());
  EnterCriticalSection(loader_lock);
  EXPECT_TRUE(OwnsLoaderLock());
  LeaveCriticalSection(loader_lock);
  EXPECT_FALSE(OwnsLoaderLock());
}

TEST(NtLoader, GetLoaderEntry) {
  // Get all modules in the current process.
  base::win::ScopedHandle snap(
      ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ::GetCurrentProcessId()));
  EXPECT_TRUE(snap.Get() != NULL);

  // Walk them, while checking we get an entry for each, and that it
  // contains sane information.
  MODULEENTRY32 module = { sizeof(module) };
  ASSERT_TRUE(::Module32First(snap.Get(), &module));
  do {
    ScopedEnterCriticalSection lock(GetLoaderLock());

    nt_loader::LDR_DATA_TABLE_ENTRY* entry =
        nt_loader::GetLoaderEntry(module.hModule);
    ASSERT_TRUE(entry != NULL);
    EXPECT_EQ(module.hModule, reinterpret_cast<HMODULE>(entry->DllBase));
    EXPECT_STREQ(module.szModule,
                 FromUnicodeString(entry->BaseDllName).c_str());
    EXPECT_STREQ(module.szExePath,
                 FromUnicodeString(entry->FullDllName).c_str());

    ULONG flags = entry->Flags;

    // All entries should have this flag set.
    EXPECT_TRUE(flags & LDRP_ENTRY_PROCESSED);

    if (0 == (flags & LDRP_IMAGE_DLL)) {
      // TODO(siggi): write a test to assert this holds true for loading
      //    non-DLL, e.g. exe image files.
      // Dlls have the LDRP_IMAGE_DLL flag set, any module that doesn't
      // have that flag has to be the main executable.
      EXPECT_TRUE(module.hModule == ::GetModuleHandle(NULL));
    } else {
      // Since we're not currently loading any modules, all loaded
      // modules should either have the LDRP_PROCESS_ATTACH_CALLED,
      // or a NULL entrypoint.
      if (entry->EntryPoint == NULL) {
        EXPECT_FALSE(flags & LDRP_PROCESS_ATTACH_CALLED);
      } else {
        // Shimeng.dll is an exception to the above, it's loaded
        // in a special way, see e.g. http://www.alex-ionescu.com/?p=41
        // for details.
        bool is_shimeng = LowerCaseEqualsASCII(
            FromUnicodeString(entry->BaseDllName), "shimeng.dll");

        EXPECT_TRUE(is_shimeng || (flags & LDRP_PROCESS_ATTACH_CALLED));
      }
    }
  } while (::Module32Next(snap.Get(), &module));
}

namespace {

typedef void (*ExceptionFunction)(EXCEPTION_POINTERS* ex_ptrs);

class NtLoaderTest: public testing::Test {
 public:
  NtLoaderTest() : veh_id_(NULL), exception_function_(NULL) {
    EXPECT_EQ(NULL, current_);
    current_ = this;
  }

  ~NtLoaderTest() {
    EXPECT_TRUE(this == current_);
    current_ = NULL;
  }

  void SetUp() {
    veh_id_ = ::AddVectoredExceptionHandler(FALSE, &ExceptionHandler);
    EXPECT_TRUE(veh_id_ != NULL);

    // Clear the crash DLL environment.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->UnSetVar(WideToASCII(kCrashOnLoadMode).c_str());
    env->UnSetVar(WideToASCII(kCrashOnUnloadMode).c_str());
  }

  void TearDown() {
    if (veh_id_ != NULL)
      EXPECT_NE(0, ::RemoveVectoredExceptionHandler(veh_id_));

    // Clear the crash DLL environment.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->UnSetVar(WideToASCII(kCrashOnLoadMode).c_str());
    env->UnSetVar(WideToASCII(kCrashOnUnloadMode).c_str());
  }

  void set_exception_function(ExceptionFunction func) {
    exception_function_ = func;
  }

 private:
  static LONG NTAPI ExceptionHandler(EXCEPTION_POINTERS* ex_ptrs){
    // Dispatch the exception to any exception function,
    // but only on the main thread.
    if (main_thread_ == ::GetCurrentThreadId() &&
        current_ != NULL &&
        current_->exception_function_ != NULL)
      current_->exception_function_(ex_ptrs);

    return ExceptionContinueSearch;
  }

  void* veh_id_;
  ExceptionFunction exception_function_;

  static NtLoaderTest* current_;
  static DWORD main_thread_;
};

NtLoaderTest* NtLoaderTest::current_ = NULL;
DWORD NtLoaderTest::main_thread_ = ::GetCurrentThreadId();

}  // namespace

static int exceptions_handled = 0;
static void OnCrashDuringLoadLibrary(EXCEPTION_POINTERS* ex_ptrs) {
  ASSERT_EQ(STATUS_ACCESS_VIOLATION, ex_ptrs->ExceptionRecord->ExceptionCode);
  ASSERT_EQ(2, ex_ptrs->ExceptionRecord->NumberParameters);
  ASSERT_EQ(EXCEPTION_WRITE_FAULT,
            ex_ptrs->ExceptionRecord->ExceptionInformation[0]);
  ASSERT_EQ(kCrashAddress,
            ex_ptrs->ExceptionRecord->ExceptionInformation[1]);

  // Bump the exceptions count.
  exceptions_handled++;

  EXPECT_TRUE(OwnsLoaderLock());

  HMODULE crash_dll = ::GetModuleHandle(kCrashDllName);
  ASSERT_TRUE(crash_dll != NULL);

  nt_loader::LDR_DATA_TABLE_ENTRY* entry = GetLoaderEntry(crash_dll);
  ASSERT_TRUE(entry != NULL);
  ASSERT_EQ(0, entry->Flags & LDRP_PROCESS_ATTACH_CALLED);
}

TEST_F(NtLoaderTest, CrashOnLoadLibrary) {
  exceptions_handled = 0;
  set_exception_function(OnCrashDuringLoadLibrary);

  // Setup to crash on load.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(WideToASCII(kCrashOnLoadMode).c_str(), "1");

  // And load it.
  HMODULE module = ::LoadLibrary(kCrashDllName);
  DWORD err = ::GetLastError();
  EXPECT_EQ(NULL, module);
  EXPECT_EQ(ERROR_NOACCESS, err);
  EXPECT_EQ(1, exceptions_handled);

  if (module != NULL)
    ::FreeLibrary(module);
}

static void OnCrashDuringUnloadLibrary(EXCEPTION_POINTERS* ex_ptrs) {
  ASSERT_EQ(STATUS_ACCESS_VIOLATION, ex_ptrs->ExceptionRecord->ExceptionCode);
  ASSERT_EQ(2, ex_ptrs->ExceptionRecord->NumberParameters);
  ASSERT_EQ(EXCEPTION_WRITE_FAULT,
            ex_ptrs->ExceptionRecord->ExceptionInformation[0]);
  ASSERT_EQ(kCrashAddress,
            ex_ptrs->ExceptionRecord->ExceptionInformation[1]);

  // Bump the exceptions count.
  exceptions_handled++;

  EXPECT_TRUE(OwnsLoaderLock());

  HMODULE crash_dll = ::GetModuleHandle(kCrashDllName);
  ASSERT_TRUE(crash_dll == NULL);

  nt_loader::LDR_DATA_TABLE_ENTRY* entry = GetLoaderEntry(crash_dll);
  ASSERT_TRUE(entry == NULL);
}

TEST_F(NtLoaderTest, CrashOnUnloadLibrary) {
  // Setup to crash on unload.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(WideToASCII(kCrashOnUnloadMode).c_str(), "1");

  // And load it.
  HMODULE module = ::LoadLibrary(kCrashDllName);
  EXPECT_TRUE(module != NULL);

  exceptions_handled = 0;
  set_exception_function(OnCrashDuringUnloadLibrary);

  // We should crash during unload.
  if (module != NULL)
    ::FreeLibrary(module);

  EXPECT_EQ(1, exceptions_handled);
}
