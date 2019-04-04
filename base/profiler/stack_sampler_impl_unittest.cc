// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/profiler/profile_builder.h"
#include "base/profiler/stack_sampler_impl.h"
#include "base/profiler/thread_delegate.h"
#include "base/profiler/unwinder.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::ElementsAre;

class TestProfileBuilder : public ProfileBuilder {
 public:
  TestProfileBuilder(ModuleCache* module_cache) : module_cache_(module_cache) {}

  TestProfileBuilder(const TestProfileBuilder&) = delete;
  TestProfileBuilder& operator=(const TestProfileBuilder&) = delete;

  // ProfileBuilder
  ModuleCache* GetModuleCache() override { return module_cache_; }
  void RecordMetadata() override {}
  void OnSampleCompleted(std::vector<Frame> frames) override {}
  void OnProfileCompleted(TimeDelta profile_duration,
                          TimeDelta sampling_period) override {}

 private:
  ModuleCache* module_cache_;
};

// A thread delegate for use in tests that provides the expected behavior when
// operating on the supplied fake stack.
class TestThreadDelegate : public ThreadDelegate {
 public:
  class TestScopedSuspendThread : public ThreadDelegate::ScopedSuspendThread {
   public:
    TestScopedSuspendThread() = default;

    TestScopedSuspendThread(const TestScopedSuspendThread&) = delete;
    TestScopedSuspendThread& operator=(const TestScopedSuspendThread&) = delete;

    bool WasSuccessful() const override { return true; }
  };

  TestThreadDelegate(const std::vector<uintptr_t>& fake_stack,
                     // The register context will be initialized to
                     // *|thread_context| if non-null.
                     RegisterContext* thread_context = nullptr)
      : fake_stack_(fake_stack),
        thread_context_(thread_context) {}

  TestThreadDelegate(const TestThreadDelegate&) = delete;
  TestThreadDelegate& operator=(const TestThreadDelegate&) = delete;

  std::unique_ptr<ScopedSuspendThread> CreateScopedSuspendThread() override {
    return std::make_unique<TestScopedSuspendThread>();
  }

  bool GetThreadContext(RegisterContext* thread_context) override {
    if (thread_context_)
      *thread_context = *thread_context_;
    // Set the stack pointer to be consistent with the provided fake stack.
    RegisterContextStackPointer(thread_context) =
        reinterpret_cast<uintptr_t>(&fake_stack_[0]);
    RegisterContextInstructionPointer(thread_context) =
        reinterpret_cast<uintptr_t>(fake_stack_[0]);
    return true;
  }

  uintptr_t GetStackBaseAddress() const override {
    return reinterpret_cast<uintptr_t>(&fake_stack_[0] + fake_stack_.size());
  }

  bool CanCopyStack(uintptr_t stack_pointer) override { return true; }

  std::vector<uintptr_t*> GetRegistersToRewrite(
      RegisterContext* thread_context) override {
    return {&RegisterContextFramePointer(thread_context)};
  }

 private:
  // Must be a reference to retain the underlying allocation from the vector
  // passed to the constructor.
  const std::vector<uintptr_t>& fake_stack_;
  RegisterContext* thread_context_;
};

// Trivial unwinder implementation for testing.
class TestUnwinder : public Unwinder {
 public:
  TestUnwinder(size_t stack_size = 0,
               std::vector<uintptr_t>* stack_copy = nullptr,
               // Variable to fill in with the bottom address of the
               // copied stack. This will be different than
               // &(*stack_copy)[0] because |stack_copy| is a copy of the
               // copy so does not share memory with the actual copy.
               uintptr_t* stack_copy_bottom = nullptr)
      : stack_size_(stack_size),
        stack_copy_(stack_copy),
        stack_copy_bottom_(stack_copy_bottom) {}

  bool CanUnwindFrom(const Frame* current_frame) const override { return true; }

  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         ModuleCache* module_cache,
                         std::vector<Frame>* stack) const override {
    if (stack_copy_) {
      auto* bottom = reinterpret_cast<uintptr_t*>(
          RegisterContextStackPointer(thread_context));
      auto* top = bottom + stack_size_;
      *stack_copy_ = std::vector<uintptr_t>(bottom, top);
    }
    if (stack_copy_bottom_)
      *stack_copy_bottom_ = RegisterContextStackPointer(thread_context);
    return UnwindResult::COMPLETED;
  }

 private:
  size_t stack_size_;
  std::vector<uintptr_t>* stack_copy_;
  uintptr_t* stack_copy_bottom_;
};

class TestModule : public ModuleCache::Module {
 public:
  TestModule(uintptr_t base_address, size_t size)
      : base_address_(base_address), size_(size) {}

  uintptr_t GetBaseAddress() const override { return base_address_; }
  std::string GetId() const override { return ""; }
  FilePath GetDebugBasename() const override { return FilePath(); }
  size_t GetSize() const override { return size_; }

 private:
  const uintptr_t base_address_;
  const size_t size_;
};

// Injects a fake module covering the initial instruction pointer value, to
// avoid asking the OS to look it up. Windows doesn't return a consistent error
// code when doing so, and we DCHECK_EQ the expected error code.
void InjectModuleForContextInstructionPointer(
    const std::vector<uintptr_t>& stack,
    ModuleCache* module_cache) {
  module_cache->InjectModuleForTesting(
      std::make_unique<TestModule>(stack[0], sizeof(uintptr_t)));
}

}  // namespace

TEST(StackSamplerImplTest, CopyStack) {
  ModuleCache module_cache;
  const std::vector<uintptr_t> stack = {0, 1, 2, 3, 4};
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  std::vector<uintptr_t> stack_copy;
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestThreadDelegate>(stack),
      std::make_unique<TestUnwinder>(stack.size(), &stack_copy), &module_cache);

  std::unique_ptr<StackSampler::StackBuffer> stack_buffer =
      std::make_unique<StackSampler::StackBuffer>(stack.size() *
                                                  sizeof(uintptr_t));
  TestProfileBuilder profile_builder(&module_cache);
  stack_sampler_impl.RecordStackFrames(stack_buffer.get(), &profile_builder);

  EXPECT_EQ(stack, stack_copy);
}

TEST(StackSamplerImplTest, CopyStackBufferTooSmall) {
  ModuleCache module_cache;
  std::vector<uintptr_t> stack = {0, 1, 2, 3, 4};
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  std::vector<uintptr_t> stack_copy;
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestThreadDelegate>(stack),
      std::make_unique<TestUnwinder>(stack.size(), &stack_copy), &module_cache);

  std::unique_ptr<StackSampler::StackBuffer> stack_buffer =
      std::make_unique<StackSampler::StackBuffer>((stack.size() - 1) *
                                                  sizeof(uintptr_t));
  // Make the buffer different than the input stack.
  stack_buffer->buffer()[0] = 100;
  TestProfileBuilder profile_builder(&module_cache);

  stack_sampler_impl.RecordStackFrames(stack_buffer.get(), &profile_builder);

  // Use the buffer not being overwritten as a proxy for the unwind being
  // aborted.
  EXPECT_NE(stack, stack_copy);
}

TEST(StackSamplerImplTest, CopyStackAndRewritePointers) {
  ModuleCache module_cache;
  // Allocate space for the stack, then make its elements point to themselves.
  std::vector<uintptr_t> stack(2);
  stack[0] = reinterpret_cast<uintptr_t>(&stack[0]);
  stack[1] = reinterpret_cast<uintptr_t>(&stack[1]);
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  std::vector<uintptr_t> stack_copy;
  uintptr_t stack_copy_bottom;
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestThreadDelegate>(stack),
      std::make_unique<TestUnwinder>(stack.size(), &stack_copy,
                                     &stack_copy_bottom),
      &module_cache);

  std::unique_ptr<StackSampler::StackBuffer> stack_buffer =
      std::make_unique<StackSampler::StackBuffer>(stack.size() *
                                                  sizeof(uintptr_t));
  TestProfileBuilder profile_builder(&module_cache);

  stack_sampler_impl.RecordStackFrames(stack_buffer.get(), &profile_builder);

  EXPECT_THAT(stack_copy, ElementsAre(stack_copy_bottom,
                                      stack_copy_bottom + sizeof(uintptr_t)));
}

TEST(StackSamplerImplTest, RewriteRegisters) {
  ModuleCache module_cache;
  std::vector<uintptr_t> stack = {0, 1, 2};
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  uintptr_t stack_copy_bottom;
  RegisterContext thread_context;
  RegisterContextFramePointer(&thread_context) =
      reinterpret_cast<uintptr_t>(&stack[1]);
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestThreadDelegate>(stack, &thread_context),
      std::make_unique<TestUnwinder>(stack.size(), nullptr, &stack_copy_bottom),
      &module_cache);

  std::unique_ptr<StackSampler::StackBuffer> stack_buffer =
      std::make_unique<StackSampler::StackBuffer>(stack.size() *
                                                  sizeof(uintptr_t));
  TestProfileBuilder profile_builder(&module_cache);
  stack_sampler_impl.RecordStackFrames(stack_buffer.get(), &profile_builder);

  EXPECT_EQ(stack_copy_bottom + sizeof(uintptr_t),
            RegisterContextFramePointer(&thread_context));
}

}  // namespace base
