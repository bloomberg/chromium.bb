// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampling_profiler.h"

#include <dbghelp.h>
#include <map>
#include <utility>
#include <windows.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/win/pe_image.h"
#include "base/win/scoped_handle.h"

namespace base {

namespace {

class NativeStackSamplerWin : public StackSamplingProfiler::NativeStackSampler {
 public:
  explicit NativeStackSamplerWin(win::ScopedHandle thread_handle);
  ~NativeStackSamplerWin() override;

  // StackSamplingProfiler::NativeStackSampler:
  void ProfileRecordingStarting(
      StackSamplingProfiler::Profile* profile) override;
  void RecordStackSample(StackSamplingProfiler::Sample* sample) override;
  void ProfileRecordingStopped() override;

 private:
  static bool GetModuleInfo(HMODULE module,
                            StackSamplingProfiler::Module* module_info);

  void CopyToSample(const void* const instruction_pointers[],
                    const HMODULE modules[],
                    int stack_depth,
                    StackSamplingProfiler::Sample* sample,
                    std::vector<StackSamplingProfiler::Module>* module_infos);

  win::ScopedHandle thread_handle_;
  // Weak. Points to the profile being recorded between
  // ProfileRecordingStarting() and ProfileRecordingStopped().
  StackSamplingProfiler::Profile* current_profile_;
  // Maps a module to the module's index within current_profile_->modules.
  std::map<HMODULE, int> profile_module_index_;

  DISALLOW_COPY_AND_ASSIGN(NativeStackSamplerWin);
};

// Walk the stack represented by |context| from the current frame downwards,
// recording the instruction pointers for each frame in |instruction_pointers|.
int RecordStack(CONTEXT* context,
                int max_stack_size,
                const void* instruction_pointers[],
                bool* last_frame_is_unknown_function) {
#ifdef _WIN64
  *last_frame_is_unknown_function = false;

  IMAGEHLP_SYMBOL64 sym;
  sym.SizeOfStruct = sizeof(sym);
  sym.MaxNameLength = 0;

  for (int i = 0; i < max_stack_size; ++i) {
    // Try to look up unwind metadata for the current function.
    ULONG64 image_base;
    PRUNTIME_FUNCTION runtime_function =
        RtlLookupFunctionEntry(context->Rip, &image_base, nullptr);

    instruction_pointers[i] = reinterpret_cast<void*>(context->Rip);

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

// Fills in |modules| corresponding to the pointers to code in |addresses|. The
// modules are returned with reference counts incremented should be freed with
// FreeModules.
void FindModulesForAddresses(const void* const addresses[], HMODULE modules[],
                             int stack_depth,
                             bool last_frame_is_unknown_function) {
  const int module_frames = last_frame_is_unknown_function ? stack_depth - 1 :
      stack_depth;
  for (int i = 0; i < module_frames; ++i) {
    HMODULE module = NULL;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          reinterpret_cast<LPCTSTR>(addresses[i]),
                          &module)) {
      // HMODULE is the base address of the module.
      DCHECK_LT(reinterpret_cast<const void*>(module), addresses[i]);
      modules[i] = module;
    }
  }
}

// Free the modules returned by FindModulesForAddresses.
void FreeModules(int stack_depth, HMODULE modules[]) {
  for (int i = 0; i < stack_depth; ++i) {
    if (modules[i])
      ::FreeLibrary(modules[i]);
  }
}

// Disables priority boost on a thread for the lifetime of the object.
class ScopedDisablePriorityBoost {
 public:
  ScopedDisablePriorityBoost(HANDLE thread_handle);
  ~ScopedDisablePriorityBoost();

 private:
  HANDLE thread_handle_;
  BOOL got_previous_boost_state_;
  BOOL boost_state_was_disabled_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisablePriorityBoost);
};

ScopedDisablePriorityBoost::ScopedDisablePriorityBoost(HANDLE thread_handle)
    : thread_handle_(thread_handle),
      got_previous_boost_state_(false),
      boost_state_was_disabled_(false) {
  got_previous_boost_state_ =
      ::GetThreadPriorityBoost(thread_handle_, &boost_state_was_disabled_);
  if (got_previous_boost_state_ && !boost_state_was_disabled_) {
    // Confusingly, TRUE disables priority boost ...
    ::SetThreadPriorityBoost(thread_handle_, TRUE);
  }
}

ScopedDisablePriorityBoost::~ScopedDisablePriorityBoost() {
  if (got_previous_boost_state_ && !boost_state_was_disabled_) {
    // ... and FALSE enables priority boost.
    ::SetThreadPriorityBoost(thread_handle_, FALSE);
  }
}

// Suspends the thread with |thread_handle|, records the stack into
// |instruction_pointers|, then resumes the thread. Returns the size of the
// stack.
int SuspendThreadAndRecordStack(HANDLE thread_handle, int max_stack_size,
                                const void* instruction_pointers[],
                                bool* last_frame_is_unknown_function) {
#if defined(_WIN64)
  if (RtlVirtualUnwind == nullptr || RtlLookupFunctionEntry == nullptr)
    return 0;
#endif

  if (::SuspendThread(thread_handle) == -1) {
    LOG(ERROR) << "SuspendThread failed: " << GetLastError();
    return 0;
  }

  CONTEXT thread_context = {0};
  thread_context.ContextFlags = CONTEXT_FULL;
  if (!::GetThreadContext(thread_handle, &thread_context)) {
    LOG(ERROR) << "GetThreadContext failed: " << GetLastError();
  }

  int stack_depth = RecordStack(&thread_context, max_stack_size,
                                instruction_pointers,
                                last_frame_is_unknown_function);

  {
    ScopedDisablePriorityBoost disable_priority_boost(thread_handle);
    if (::ResumeThread(thread_handle) == -1)
      LOG(ERROR) << "ResumeThread failed: " << GetLastError();
  }

  return stack_depth;
}

}  // namespace

scoped_ptr<StackSamplingProfiler::NativeStackSampler>
StackSamplingProfiler::NativeStackSampler::Create(PlatformThreadId thread_id) {
#if _WIN64
  // Get the thread's handle.
  HANDLE thread_handle = ::OpenThread(
      THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
      FALSE,
      thread_id);
  DCHECK(thread_handle) << "OpenThread failed";

  return scoped_ptr<NativeStackSampler>(new NativeStackSamplerWin(
      win::ScopedHandle(thread_handle)));
#else
  return scoped_ptr<NativeStackSampler>();
#endif
}

NativeStackSamplerWin::NativeStackSamplerWin(win::ScopedHandle thread_handle)
    : thread_handle_(thread_handle.Take()) {
#ifdef _WIN64
  if (RtlVirtualUnwind == nullptr && RtlLookupFunctionEntry == nullptr) {
    const HMODULE nt_dll_handle = ::GetModuleHandle(L"ntdll.dll");
    // This should always be non-null, but handle just in case.
    if (nt_dll_handle) {
      reinterpret_cast<void*&>(RtlVirtualUnwind) =
          ::GetProcAddress(nt_dll_handle, "RtlVirtualUnwind");
      reinterpret_cast<void*&>(RtlLookupFunctionEntry) =
          ::GetProcAddress(nt_dll_handle, "RtlLookupFunctionEntry");
    }
  }
#endif
}

NativeStackSamplerWin::~NativeStackSamplerWin() {
}

void NativeStackSamplerWin::ProfileRecordingStarting(
    StackSamplingProfiler::Profile* profile) {
  current_profile_ = profile;
  profile_module_index_.clear();
}

void NativeStackSamplerWin::RecordStackSample(
    StackSamplingProfiler::Sample* sample) {
  DCHECK(current_profile_);

  const int max_stack_size = 64;
  const void* instruction_pointers[max_stack_size] = {0};
  HMODULE modules[max_stack_size] = {0};

  bool last_frame_is_unknown_function = false;
  int stack_depth = SuspendThreadAndRecordStack(
      thread_handle_.Get(), max_stack_size, instruction_pointers,
      &last_frame_is_unknown_function);
  FindModulesForAddresses(instruction_pointers, modules, stack_depth,
                          last_frame_is_unknown_function);
  CopyToSample(instruction_pointers, modules, stack_depth, sample,
               &current_profile_->modules);
  FreeModules(stack_depth, modules);
}

void NativeStackSamplerWin::ProfileRecordingStopped() {
  current_profile_ = nullptr;
}

// static
bool NativeStackSamplerWin::GetModuleInfo(
    HMODULE module,
    StackSamplingProfiler::Module* module_info) {
  wchar_t module_name[MAX_PATH];
  DWORD result_length =
      GetModuleFileName(module, module_name, arraysize(module_name));
  if (result_length == 0)
    return false;

  module_info->filename = base::FilePath(module_name);

  module_info->base_address = reinterpret_cast<const void*>(module);

  GUID guid;
  DWORD age;
  win::PEImage(module).GetDebugId(&guid, &age);
  module_info->id.insert(module_info->id.end(),
                         reinterpret_cast<char*>(&guid),
                         reinterpret_cast<char*>(&guid + 1));
  module_info->id.insert(module_info->id.end(),
                         reinterpret_cast<char*>(&age),
                         reinterpret_cast<char*>(&age + 1));

  return true;
}

void NativeStackSamplerWin::CopyToSample(
    const void* const instruction_pointers[],
    const HMODULE modules[],
    int stack_depth,
    StackSamplingProfiler::Sample* sample,
    std::vector<StackSamplingProfiler::Module>* module_infos) {
  sample->clear();
  sample->reserve(stack_depth);

  for (int i = 0; i < stack_depth; ++i) {
    sample->push_back(StackSamplingProfiler::Frame());
    StackSamplingProfiler::Frame& frame = sample->back();

    frame.instruction_pointer = instruction_pointers[i];

    // Record an invalid module index if we don't have a valid module.
    if (!modules[i]) {
      frame.module_index = -1;
      continue;
    }

    auto loc = profile_module_index_.find(modules[i]);
    if (loc == profile_module_index_.end()) {
      StackSamplingProfiler::Module module_info;
      // Record an invalid module index if we have a module but can't find
      // information on it.
      if (!GetModuleInfo(modules[i], &module_info)) {
        frame.module_index = -1;
        continue;
      }
      module_infos->push_back(module_info);
      loc = profile_module_index_.insert(std::make_pair(
          modules[i], static_cast<int>(module_infos->size() - 1))).first;
    }

    frame.module_index = loc->second;
  }
}

}  // namespace base
