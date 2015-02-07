// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/cpu_profiler.h"

#include <Tlhelp32.h>
#include <dbghelp.h>
#include <stddef.h>
#include <windows.h>

#include "base/debug/stack_trace.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/time/time.h"

namespace base {

namespace {

const DWORD kSuspendFailed = static_cast<DWORD>(-1);

int StackTrace64(CONTEXT* context, int max_stack_size,
                 StackTraceEntry* stack_trace,
                 bool* last_frame_is_unknown_function) {
#ifdef _WIN64
  *last_frame_is_unknown_function = false;

  IMAGEHLP_SYMBOL64 sym;
  sym.SizeOfStruct  = sizeof(sym);
  sym.MaxNameLength = 0;

  for (int i = 0; i < max_stack_size; ++i, ++stack_trace) {
    // Try to look up unwind metadata for the current function.
    ULONG64 image_base;
    PRUNTIME_FUNCTION runtime_function =
        RtlLookupFunctionEntry(context->Rip, &image_base, nullptr);

    stack_trace->rsp = context->Rsp;
    stack_trace->rip = context->Rip;
    stack_trace->module = nullptr;

    if (runtime_function) {
      KNONVOLATILE_CONTEXT_POINTERS nvcontext = {0};
      void* handler_data;
      ULONG64 establisher_frame;
      RtlVirtualUnwind(0, image_base, context->Rip, runtime_function, context,
          &handler_data, &establisher_frame, &nvcontext);
    } else {
      // If we don't have a RUNTIME_FUNCTION, then we've encountered
      // a leaf function.  Adjust the stack appropriately.
      context->Rip = *reinterpret_cast<PDWORD64>(context->Rsp);
      context->Rsp += 8;
      *last_frame_is_unknown_function = true;
    }

    if (!context->Rip)
      return i;
  }
  return max_stack_size;
#else
  return 0;
#endif
}

// Look up modules from instruction pointers.
void FindModulesForIPs(StackTraceEntry* stack_trace, int stack_depth,
                       bool last_frame_is_unknown_function) {
  const int module_frames = last_frame_is_unknown_function ? stack_depth - 1 :
      stack_depth;
  for (int i = 0; i < module_frames; ++i) {
    HMODULE module = NULL;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCTSTR>(stack_trace[i].rip),
                          &module)) {
      stack_trace->module = module;
    }
  }
}

int SampleThread(HANDLE thread_handle, int max_stack_size,
                 StackTraceEntry* stack) {
  if (::SuspendThread(thread_handle) == kSuspendFailed) {
    LOG(ERROR) << "SuspendThread failed: " << GetLastError();
    return 0;
  }

  CONTEXT thread_context = {0};
  thread_context.ContextFlags = CONTEXT_ALL;
  if (!::GetThreadContext(thread_handle, &thread_context)) {
    LOG(ERROR) << "GetThreadContext failed: " << GetLastError();
  }

  bool last_frame_is_unknown_function = false;
  int stack_depth = StackTrace64(&thread_context, max_stack_size, stack,
                                 &last_frame_is_unknown_function);

  if (::ResumeThread(thread_handle) == kSuspendFailed) {
    LOG(ERROR) << "ResumeThread failed: " << GetLastError();
  }

  FindModulesForIPs(stack, stack_depth, last_frame_is_unknown_function);

  return stack_depth;
}

void GetNames(StackTraceEntry* stack_trace, int stack_depth) {
  HANDLE process = GetCurrentProcess();
  DWORD64 displacement = 0;
  DWORD line_displacement = 0;
  char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(wchar_t)];
  SYMBOL_INFO* symbol_info = reinterpret_cast<SYMBOL_INFO*>(buffer);
  symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol_info->MaxNameLen = MAX_SYM_NAME;
  std::string file_name;

  IMAGEHLP_LINE64 line;
  line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

  for (int i = 0; i < stack_depth; ++i, ++stack_trace) {
    DWORD64 address = stack_trace->rip;
    if (!SymFromAddr(process, address, &displacement, symbol_info)) {
      wcscpy_s(reinterpret_cast<wchar_t*>(symbol_info->Name),
               symbol_info->MaxNameLen, L"failed");
    }
    if (!SymGetLineFromAddr64(process, address, &line_displacement, &line)) {
      line.LineNumber = 0;
      file_name.clear();
    } else {
      file_name.assign(line.FileName);
    }
  }
}

}

CpuProfiler::CpuProfiler()
    : main_thread_(0),
      main_thread_stack_depth_(0) {
#ifdef _WIN64
  // Record the main thread's handle.
  BOOL result = ::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(),
                                  ::GetCurrentProcess(), &main_thread_, 0,
                                  FALSE, DUPLICATE_SAME_ACCESS);
  DCHECK(result) << "DuplicateHandle failed";

  // Initialization
  if (RtlVirtualUnwind == NULL && RtlLookupFunctionEntry == NULL) {
    const HMODULE nt_dll_handle = ::GetModuleHandle(L"ntdll.dll");
    reinterpret_cast<void*&>(RtlVirtualUnwind) =
        ::GetProcAddress(nt_dll_handle, "RtlVirtualUnwind");
    reinterpret_cast<void*&>(RtlLookupFunctionEntry) =
        ::GetProcAddress(nt_dll_handle, "RtlLookupFunctionEntry");
  }
#endif
}

CpuProfiler::~CpuProfiler() {
#ifdef _WIN64
  CloseHandle(main_thread_);
#endif
}

// static
bool CpuProfiler::IsPlatformSupported() {
#ifdef _WIN64
  return true;
#else
  return false;
#endif
}

void CpuProfiler::OnTimer() {
  main_thread_stack_depth_ = SampleThread(
      main_thread_, arraysize(main_thread_stack_), main_thread_stack_);
  ProcessStack(main_thread_stack_, main_thread_stack_depth_);

  modules_.clear();
  main_thread_stack_depth_ = 0;
}

void CpuProfiler::ProcessStack(StackTraceEntry* stack, int stack_depth) {
  for (int i = 0; i < stack_depth; ++i, ++stack) {
    HMODULE module = stack->module;
    if (!module || (modules_.find(module) != modules_.end()))
      continue;

    wchar_t module_name[MAX_PATH];
    if (GetModuleFileName(module, module_name, arraysize(module_name))) {
      modules_.insert(std::pair<HMODULE, base::string16>(module, module_name));
    }
  }
}

}  // namespace base
