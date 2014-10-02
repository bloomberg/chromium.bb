// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/isolate_holder.h"

#include <stdlib.h>
#include <string.h>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "gin/array_buffer.h"
#include "gin/debug_impl.h"
#include "gin/function_template.h"
#include "gin/per_isolate_data.h"
#include "gin/public/v8_platform.h"
#include "gin/run_microtasks_observer.h"

namespace gin {

namespace {

v8::ArrayBuffer::Allocator* g_array_buffer_allocator = NULL;

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  base::RandBytes(buffer, amount);
  return true;
}

}  // namespace

IsolateHolder::IsolateHolder() {
  CHECK(g_array_buffer_allocator)
      << "You need to invoke gin::IsolateHolder::Initialize first";
  v8::Isolate::CreateParams params;
  params.entry_hook = DebugImpl::GetFunctionEntryHook();
  params.code_event_handler = DebugImpl::GetJitCodeEventHandler();
  params.constraints.ConfigureDefaults(base::SysInfo::AmountOfPhysicalMemory(),
                                       base::SysInfo::AmountOfVirtualMemory(),
                                       base::SysInfo::NumberOfProcessors());
  isolate_ = v8::Isolate::New(params);
  isolate_data_.reset(new PerIsolateData(isolate_, g_array_buffer_allocator));
#if defined(OS_WIN)
  {
    void* code_range;
    size_t size;
    isolate_->GetCodeRange(&code_range, &size);
    Debug::CodeRangeCreatedCallback callback =
        DebugImpl::GetCodeRangeCreatedCallback();
    if (code_range && size && callback)
      callback(code_range, size);
  }
#endif
}

IsolateHolder::~IsolateHolder() {
  if (task_observer_.get())
    base::MessageLoop::current()->RemoveTaskObserver(task_observer_.get());
#if defined(OS_WIN)
  {
    void* code_range;
    size_t size;
    isolate_->GetCodeRange(&code_range, &size);
    Debug::CodeRangeDeletedCallback callback =
        DebugImpl::GetCodeRangeDeletedCallback();
    if (code_range && callback)
      callback(code_range);
  }
#endif
  isolate_data_.reset();
  isolate_->Dispose();
}

// static
void IsolateHolder::Initialize(ScriptMode mode,
                               v8::ArrayBuffer::Allocator* allocator) {
  CHECK(allocator);
  static bool v8_is_initialized = false;
  if (v8_is_initialized)
    return;
  v8::V8::InitializePlatform(V8Platform::Get());
  v8::V8::SetArrayBufferAllocator(allocator);
  g_array_buffer_allocator = allocator;
  if (mode == gin::IsolateHolder::kStrictMode) {
    static const char v8_flags[] = "--use_strict";
    v8::V8::SetFlagsFromString(v8_flags, sizeof(v8_flags) - 1);
  }
  v8::V8::SetEntropySource(&GenerateEntropy);
  v8::V8::Initialize();
  v8_is_initialized = true;
}

void IsolateHolder::AddRunMicrotasksObserver() {
  DCHECK(!task_observer_.get());
  task_observer_.reset(new RunMicrotasksObserver(isolate_));;
  base::MessageLoop::current()->AddTaskObserver(task_observer_.get());
}

void IsolateHolder::RemoveRunMicrotasksObserver() {
  DCHECK(task_observer_.get());
  base::MessageLoop::current()->RemoveTaskObserver(task_observer_.get());
  task_observer_.reset();
}

}  // namespace gin
