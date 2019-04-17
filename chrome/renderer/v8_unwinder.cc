// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/v8_unwinder.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"

namespace {

// Synthetic build id to use for V8 modules.
const char kV8BuildId[] = "5555555517284E1E874EFA4EB754964B999";

class V8Module : public base::ModuleCache::Module {
 public:
  V8Module(const v8::MemoryRange& memory_range, const std::string& descriptor)
      : memory_range_(memory_range), descriptor_(descriptor) {}

  V8Module(const V8Module&) = delete;
  V8Module& operator=(const V8Module&) = delete;

  // ModuleCache::Module
  uintptr_t GetBaseAddress() const override {
    return reinterpret_cast<uintptr_t>(memory_range_.start);
  }

  std::string GetId() const override { return kV8BuildId; }

  base::FilePath GetDebugBasename() const override {
    return base::FilePath().AppendASCII(base::StrCat({"V8 ", descriptor_}));
  }

  size_t GetSize() const override { return memory_range_.length_in_bytes; }

  bool IsNative() const override { return false; }

 private:
  const v8::MemoryRange memory_range_;
  const std::string descriptor_;
};

}  // namespace

V8Unwinder::V8Unwinder(const v8::UnwindState& unwind_state)
    : unwind_state_(unwind_state) {}

V8Unwinder::~V8Unwinder() = default;

void V8Unwinder::AddNonNativeModules(base::ModuleCache* module_cache) {
  std::vector<std::unique_ptr<base::ModuleCache::Module>> modules;
  modules.emplace_back(
      std::make_unique<V8Module>(unwind_state_.code_range, "Code Range"));
  modules.emplace_back(std::make_unique<V8Module>(
      unwind_state_.embedded_code_range, "Embedded Code Range"));
  for (auto& module : modules) {
    v8_modules_.insert(module.get());
    module_cache->AddNonNativeModule(std::move(module));
  }
}

bool V8Unwinder::CanUnwindFrom(const base::Frame* current_frame) const {
  return v8_modules_.find(current_frame->module) != v8_modules_.end();
}

base::UnwindResult V8Unwinder::TryUnwind(
    base::RegisterContext* thread_context,
    uintptr_t stack_top,
    base::ModuleCache* module_cache,
    std::vector<base::Frame>* stack) const {
  v8::RegisterState register_state;
  register_state.pc = reinterpret_cast<void*>(
      base::RegisterContextInstructionPointer(thread_context));
  register_state.sp = reinterpret_cast<void*>(
      base::RegisterContextStackPointer(thread_context));
  register_state.fp = reinterpret_cast<void*>(
      base::RegisterContextFramePointer(thread_context));

  if (!v8::Unwinder::TryUnwindV8Frames(
          unwind_state_, &register_state,
          reinterpret_cast<const void*>(stack_top))) {
    return base::UnwindResult::ABORTED;
  }

  base::RegisterContextInstructionPointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.pc);
  base::RegisterContextStackPointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.sp);
  base::RegisterContextFramePointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.fp);

  stack->emplace_back(
      base::RegisterContextInstructionPointer(thread_context),
      module_cache->GetModuleForAddress(
          base::RegisterContextInstructionPointer(thread_context)));

  return base::UnwindResult::UNRECOGNIZED_FRAME;
}
