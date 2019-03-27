// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/profiler/profile_builder.h"
#include "base/profiler/stack_sampler_impl.h"
#include "base/profiler/thread_delegate.h"
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
                     // Vector to fill in with the copied stack.
                     std::vector<uintptr_t>* stack_copy = nullptr,
                     // Variable to fill in with the bottom address of the
                     // copied stack. This will be different than
                     // &(*stack_copy)[0] because |stack_copy| is a copy of the
                     // copy so does not share memory with the actual copy.
                     uintptr_t* stack_copy_bottom = nullptr,
                     // The register context will be initialized to
                     // *|thread_context| if non-null.
                     RegisterContext* thread_context = nullptr)
      : fake_stack_(fake_stack),
        stack_copy_(stack_copy),
        stack_copy_bottom_(stack_copy_bottom),
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

  UnwindResult WalkNativeFrames(
      RegisterContext* thread_context,
      uintptr_t stack_top,
      ModuleCache* module_cache,
      std::vector<ProfileBuilder::Frame>* stack) override {
    if (stack_copy_) {
      auto* bottom = reinterpret_cast<uintptr_t*>(
          RegisterContextStackPointer(thread_context));
      auto* top = bottom + fake_stack_.size();
      *stack_copy_ = std::vector<uintptr_t>(bottom, top);
    }
    if (stack_copy_bottom_)
      *stack_copy_bottom_ = RegisterContextStackPointer(thread_context);
    return UnwindResult::COMPLETED;
  }

 private:
  // Must be a reference to retain the underlying allocation from the vector
  // passed to the constructor.
  const std::vector<uintptr_t>& fake_stack_;
  std::vector<uintptr_t>* stack_copy_;
  uintptr_t* stack_copy_bottom_;
  RegisterContext* thread_context_;
};

}  // namespace

TEST(StackSamplerImplTest, CopyStack) {
  ModuleCache module_cache;
  const std::vector<uintptr_t> stack = {0, 1, 2, 3, 4};
  std::vector<uintptr_t> stack_copy;
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestThreadDelegate>(stack, &stack_copy), &module_cache);

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
  std::vector<uintptr_t> stack_copy;
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestThreadDelegate>(stack, &stack_copy), &module_cache);

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
  std::vector<uintptr_t> stack_copy;
  uintptr_t stack_copy_bottom;
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestThreadDelegate>(stack, &stack_copy,
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
  std::vector<uintptr_t> stack(3);
  uintptr_t stack_copy_bottom;
  RegisterContext thread_context;
  RegisterContextFramePointer(&thread_context) =
      reinterpret_cast<uintptr_t>(&stack[1]);
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestThreadDelegate>(stack, nullptr, &stack_copy_bottom,
                                           &thread_context),
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
