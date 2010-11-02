// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities and declarations to do with GFlags.
#ifndef CEEE_TESTING_UTILS_GFLAG_UTILS_H_
#define CEEE_TESTING_UTILS_GFLAG_UTILS_H_

#include <windows.h>

namespace testing {

// Offset to the NtGlobalFlags from observation of running processes.
#ifdef _WIN64
// For 64 bit processes.
const ptrdiff_t kGFlagsPEBOffset = 0xBC;
#else
// For 32 bit processes.
const ptrdiff_t kGFlagsPEBOffset = 0x68;
#endif

// Nt global flags values gleaned from MSDN documentation
// at e.g. http://msdn.microsoft.com/en-us/library/cc265893.aspx.

// Stop on exception
#define FLG_STOP_ON_EXCEPTION           0x00000001
// Show loader snaps
#define FLG_SHOW_LDR_SNAPS              0x00000002
// Debug initial command
#define FLG_DEBUG_INITIAL_COMMAND       0x00000004
// Stop on hung GUI
#define FLG_STOP_ON_HUNG_GUI            0x00000008
// Enable heap tail checking
#define FLG_HEAP_ENABLE_TAIL_CHECK      0x00000010
// Enable heap free checking
#define FLG_HEAP_ENABLE_FREE_CHECK      0x00000020
// Enable heap parameter checking
#define FLG_HEAP_VALIDATE_PARAMETERS    0x00000040
// Enable heap validation on call
#define FLG_HEAP_VALIDATE_ALL           0x00000080
// Enable application verifier
#define FLG_APPLICATION_VERIFIER        0x00000100
// Enable pool tagging (Windows 2000 and Windows XP only)
#define FLG_POOL_ENABLE_TAGGING         0x00000400
// Enable heap tagging
#define FLG_HEAP_ENABLE_TAGGING         0x00000800
// Create user mode stack trace database
#define FLG_USER_STACK_TRACE_DB         0x00001000
// Create kernel mode stack trace database
#define FLG_KERNEL_STACK_TRACE_DB       0x00002000
// Maintain a list of objects for each type
#define FLG_MAINTAIN_OBJECT_TYPELIST    0x00004000
// Enable heap tagging by DLL
#define FLG_HEAP_ENABLE_TAG_BY_DLL      0x00008000
// Disable stack extension
#define FLG_DISABLE_STACK_EXTENSION     0x00010000
// Enable debugging of Win32 subsystem
#define FLG_ENABLE_CSRDEBUG             0x00020000
// Enable loading of kernel debugger symbols
#define FLG_ENABLE_KDEBUG_SYMBOL_LOAD   0x00040000
// Disable paging of kernel stacks
#define FLG_DISABLE_PAGE_KERNEL_STACKS  0x00080000
// Enable system critical breaks
#define FLG_ENABLE_SYSTEM_CRIT_BREAKS   0x00100000
// Disable heap coalesce on free
#define FLG_HEAP_DISABLE_COALESCING     0x00200000
// Enable close exception
#define FLG_ENABLE_CLOSE_EXCEPTIONS     0x00400000
// Enable exception logging
#define FLG_ENABLE_EXCEPTION_LOGGING    0x00800000
// Enable object handle type tagging
#define FLG_ENABLE_HANDLE_TYPE_TAGGING  0x01000000
// Enable page heap
#define FLG_HEAP_PAGE_ALLOCS            0x02000000
// Debug WinLogon
#define FLG_DEBUG_INITIAL_COMMAND_EX    0x04000000
// Buffer DbgPrint output
#define FLG_DISABLE_DBGPRINT            0x08000000
// Early critical section event creation
#define FLG_CRITSEC_EVENT_CREATION      0x10000000
// Load DLLs top-down (Win64 only)
#define FLG_LDR_TOP_DOWN                0x20000000
// Enable bad handles detection
#define FLG_ENABLE_HANDLE_EXCEPTIONS    0x40000000
// Disable protected DLL verification
#define FLG_DISABLE_PROTDLLS            0x80000000
#define FLG_VALID_BITS                  0x7FFFFFFF


// Writes gflags to process' PEB.
// @param process a handle the the process, must grant
//      PROCESS_QUERY_INFORMATION, PROCESS_VM_WRITE and
//      PROCESS_VM_OPERATION.
// @param gflags the set of nt global flags to write.
HRESULT WriteProcessGFlags(HANDLE process, DWORD gflags);

// Creates a new process with the supplied gflags.
// Parameters are the same as for ::CreateProcess.
HRESULT CreateProcessWithGFlags(LPCWSTR application_name,
                                LPWSTR command_line,
                                LPSECURITY_ATTRIBUTES process_attributes,
                                LPSECURITY_ATTRIBUTES thread_attributes,
                                BOOL inherit_handles,
                                DWORD creation_flags,
                                LPVOID environment,
                                LPCWSTR current_directory,
                                LPSTARTUPINFOW startup_info,
                                LPPROCESS_INFORMATION proc_info,
                                DWORD gflags);

// Relaunches the current executable with the same command
// line, environment, current directory, as this process has,
// but with the supplied gflags in addition to the current process'
// gflags.
// @param wanted_gflags the set of global flags requested.
// @return S_FALSE if the current process is already running with
//    all of the supplied flags, or an appropriate error otherwise.
// @note On success, this function will not return, but will
//    call ExitProcess to propagate the exit code of the
//    relaunched child process.
HRESULT RelaunchWithGFlags(DWORD wanted_gflags);

}  // namespace testing

#endif  // CEEE_TESTING_UTILS_GFLAG_UTILS_H_
