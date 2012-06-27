// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_
#define CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_

#include "base/logging.h"
#include "chrome_frame/crash_reporting/vectored_handler.h"
#include "chrome_frame/crash_reporting/nt_loader.h"

#if !defined(_M_IX86)
#error only x86 is supported for now.
#endif

#ifndef EXCEPTION_CHAIN_END
#define EXCEPTION_CHAIN_END ((struct _EXCEPTION_REGISTRATION_RECORD*)-1)
#if !defined(_WIN32_WINNT_WIN8)
typedef struct _EXCEPTION_REGISTRATION_RECORD {
  struct _EXCEPTION_REGISTRATION_RECORD* Next;
  PVOID Handler;
} EXCEPTION_REGISTRATION_RECORD;
// VEH handler flags settings.
// These are grabbed from winnt.h for PocketPC.
// Only EXCEPTION_NONCONTINUABLE in defined in "regular" winnt.h
// #define EXCEPTION_NONCONTINUABLE 0x1    // Noncontinuable exception
#define EXCEPTION_UNWINDING 0x2         // Unwind is in progress
#define EXCEPTION_EXIT_UNWIND 0x4       // Exit unwind is in progress
#define EXCEPTION_STACK_INVALID 0x8     // Stack out of limits or unaligned
#define EXCEPTION_NESTED_CALL 0x10      // Nested exception handler call
#define EXCEPTION_TARGET_UNWIND 0x20    // Target unwind in progress
#define EXCEPTION_COLLIDED_UNWIND 0x40  // Collided exception handler call

#define EXCEPTION_UNWIND (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND | \
    EXCEPTION_TARGET_UNWIND | EXCEPTION_COLLIDED_UNWIND)

#define IS_UNWINDING(Flag)  (((Flag) & EXCEPTION_UNWIND) != 0)
#define IS_DISPATCHING(Flag)  (((Flag) & EXCEPTION_UNWIND) == 0)
#define IS_TARGET_UNWIND(Flag)  ((Flag) & EXCEPTION_TARGET_UNWIND)
#endif  // !defined(_WIN32_WINNT_WIN8)
#endif  // EXCEPTION_CHAIN_END

template <typename E>
VectoredHandlerT<E>::VectoredHandlerT(E* api) : exceptions_seen_(0), api_(api) {
}

template <typename E>
VectoredHandlerT<E>::~VectoredHandlerT() {
}

template <typename E>
LONG VectoredHandlerT<E>::Handler(EXCEPTION_POINTERS* exceptionInfo) {
  // TODO(stoyan): Consider reentrancy
  const DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;

  // Not interested in non-error exceptions. In this category falls exceptions
  // like:
  // 0x40010006 - OutputDebugStringA. Seen when no debugger is attached
  //              (otherwise debugger swallows the exception and prints
  //              the string).
  // 0x406D1388 - DebuggerProbe. Used by debug CRT - for example see source
  //              code of isatty(). Used to name a thread as well.
  // RPC_E_DISCONNECTED and Co. - COM IPC non-fatal warnings
  // STATUS_BREAKPOINT and Co. - Debugger related breakpoints
  if ((exceptionCode & ERROR_SEVERITY_ERROR) != ERROR_SEVERITY_ERROR) {
    return ExceptionContinueSearch;
  }

  // Ignore custom exception codes.
  // MSXML likes to raise 0xE0000001 while parsing.
  // Note the C++ SEH (0xE06D7363) also fails in that range.
  if (exceptionCode & APPLICATION_ERROR_MASK) {
    return ExceptionContinueSearch;
  }

  exceptions_seen_++;

  // If the exception code is STATUS_STACK_OVERFLOW then proceed as usual -
  // we want to report it. Otherwise check whether guard page in stack is gone -
  // i.e. stack overflow has been already observed and most probably we are
  // seeing the follow-up STATUS_ACCESS_VIOLATION(s). See bug 32441.
  if (exceptionCode != STATUS_STACK_OVERFLOW && api_->CheckForStackOverflow()) {
    return ExceptionContinueSearch;
  }

  // Check whether exception will be handled by the module that cuased it.
  // This should automatically handle the case of IsBadReadPtr and family.
  const EXCEPTION_REGISTRATION_RECORD* seh = api_->RtlpGetExceptionList();
  if (api_->ShouldIgnoreException(exceptionInfo, seh)) {
    return ExceptionContinueSearch;
  }

  const DWORD exceptionFlags = exceptionInfo->ExceptionRecord->ExceptionFlags;
  // I don't think VEH is called on unwind. Just to be safe.
  if (IS_DISPATCHING(exceptionFlags)) {
    if (ModuleHasInstalledSEHFilter())
      return ExceptionContinueSearch;

    if (api_->IsOurModule(exceptionInfo->ExceptionRecord->ExceptionAddress)) {
      api_->WriteDump(exceptionInfo);
      return ExceptionContinueSearch;
    }

    // See whether our module is somewhere in the call stack.
    void* back_trace[E::max_back_trace] = {0};
    DWORD captured = api_->RtlCaptureStackBackTrace(0, api_->max_back_trace,
                                                    &back_trace[0], NULL);
    for (DWORD i = 0; i < captured; ++i) {
      if (api_->IsOurModule(back_trace[i])) {
        api_->WriteDump(exceptionInfo);
        return ExceptionContinueSearch;
      }
    }
  }

  return ExceptionContinueSearch;
}

template <typename E>
bool VectoredHandlerT<E>::ModuleHasInstalledSEHFilter() {
  const EXCEPTION_REGISTRATION_RECORD* RegistrationFrame =
      api_->RtlpGetExceptionList();
  // TODO(stoyan): Add the stack limits check and some sanity checks like
  // decreasing addresses of registration records
  while (RegistrationFrame != EXCEPTION_CHAIN_END) {
    if (api_->IsOurModule(RegistrationFrame->Handler)) {
      return true;
    }

    RegistrationFrame = RegistrationFrame->Next;
  }

  return false;
}


// Here comes the default Windows 32-bit implementation.
namespace {
  __declspec(naked)
    static EXCEPTION_REGISTRATION_RECORD* InternalRtlpGetExceptionList() {
      __asm {
        mov eax, fs:0
        ret
      }
  }

  __declspec(naked)
  static char* GetStackTopLimit() {
    __asm {
      mov eax, fs:8
      ret
    }
  }
}  // end of namespace

// Class which methods simply forwards to Win32 API.
// Used as template (external interface) of VectoredHandlerT<E>.
class Win32VEHTraits {
 public:
  enum {max_back_trace = 62};

  static inline
  EXCEPTION_REGISTRATION_RECORD* RtlpGetExceptionList() {
    return InternalRtlpGetExceptionList();
  }

  static inline WORD RtlCaptureStackBackTrace(DWORD FramesToSkip,
      DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash) {
    return ::RtlCaptureStackBackTrace(FramesToSkip, FramesToCapture,
                                      BackTrace, BackTraceHash);
  }

  static bool ShouldIgnoreException(const EXCEPTION_POINTERS* exceptionInfo,
      const EXCEPTION_REGISTRATION_RECORD* seh_record) {
    const void* address = exceptionInfo->ExceptionRecord->ExceptionAddress;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    HMODULE crashing_module = GetModuleHandleFromAddress(address);
    if (!crashing_module)
      return false;

    HMODULE top_seh_module = NULL;
    if (EXCEPTION_CHAIN_END != seh_record)
      top_seh_module = GetModuleHandleFromAddress(seh_record->Handler);

    // check if the crash address belongs in a module that's expecting
    // and handling it. This should address cases like kernel32!IsBadXXX
    // and urlmon!IsValidInterface
    if (crashing_module == top_seh_module)
      return true;

    // We don't want to report exceptions that occur during DLL loading,
    // as those are captured and ignored by the NT loader. If this thread
    // is holding the loader's lock, there's a possiblity that the crash
    // is occurring in a loading DLL, in which case we resolve the
    // crash address to a module and check on the module's status.
    if (nt_loader::OwnsLoaderLock()) {
      nt_loader::LDR_DATA_TABLE_ENTRY* entry =
          nt_loader::GetLoaderEntry(crashing_module);

      // If:
      //  1. we found the entry in question, and
      //  2. the entry is a DLL (has the IMAGE_DLL flag set), and
      //  3. the DLL has a non-null entrypoint, and
      //  4. the loader has not tagged it with the process attached called flag
      // we conclude that the crash is most likely during the loading of the
      // module and bail on reporting the crash to avoid false positives
      // during crashes that occur during module loads, such as e.g. when
      // the hook manager attempts to load buggy window hook DLLs.
      if (entry &&
          (entry->Flags & LDRP_IMAGE_DLL) != 0 &&
          (entry->EntryPoint != NULL) &&
          (entry->Flags & LDRP_PROCESS_ATTACH_CALLED) == 0)
        return true;
    }

    return false;
  }

  static bool CheckForStackOverflow() {
    MEMORY_BASIC_INFORMATION mi = {0};
    const DWORD kPageSize = 0x1000;
    void* stack_top = GetStackTopLimit() - kPageSize;
    ::VirtualQuery(stack_top, &mi, sizeof(mi));
    // The above call may result in moving the top of the stack.
    // Check once more.
    void* stack_top2 = GetStackTopLimit() - kPageSize;
    if (stack_top2 != stack_top)
      ::VirtualQuery(stack_top2, &mi, sizeof(mi));
    return !(mi.Protect & PAGE_GUARD);
  }

 private:
  static inline const void* CodeOffset(const void* code, int offset) {
    return reinterpret_cast<const char*>(code) + offset;
  }

  static HMODULE GetModuleHandleFromAddress(const void* p) {
    HMODULE module_at_address = NULL;
    MEMORY_BASIC_INFORMATION mi = {0};
    if (::VirtualQuery(p, &mi, sizeof(mi)) && (mi.Type & MEM_IMAGE)) {
      module_at_address = reinterpret_cast<HMODULE>(mi.AllocationBase);
    }

    return module_at_address;
  }
};


// Use Win32 API; checks for single (current) module. Will call a specified
// CrashHandlerTraits::DumpHandler when taking a dump.
class CrashHandlerTraits : public Win32VEHTraits,
                           public ModuleOfInterestWithExcludedRegion {
 public:

  typedef bool (*DumpHandler)(EXCEPTION_POINTERS* p);

  CrashHandlerTraits() : dump_handler_(NULL) {}

  // Note that breakpad_lock must be held when this is called.
  void Init(const void* veh_segment_start, const void* veh_segment_end,
            DumpHandler dump_handler) {
    DCHECK(dump_handler);
    dump_handler_ = dump_handler;
    ModuleOfInterestWithExcludedRegion::SetCurrentModule();
    // Pointers to static (non-extern) functions take the address of the
    // function's first byte, as opposed to an entry in the compiler generated
    // JMP table. In release builds /OPT:REF wipes away the JMP table, but debug
    // builds are not so lucky.
    ModuleOfInterestWithExcludedRegion::SetExcludedRegion(veh_segment_start,
                                                          veh_segment_end);
  }

  void Shutdown() {
  }

  inline bool WriteDump(EXCEPTION_POINTERS* p) {
    if (dump_handler_) {
      return dump_handler_(p);
    } else {
      return false;
    }
  }

 private:
  DumpHandler dump_handler_;
};

#endif  // CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_
