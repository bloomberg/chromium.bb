// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_TRAITS_H_
#define BASE_TASK_TASK_TRAITS_H_

#include <stdint.h>

#include <iosfwd>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/base_export.h"
#include "base/logging.h"
#include "base/task/task_traits_extension.h"
#include "base/traits_bag.h"
#include "build/build_config.h"

namespace base {

class PostTaskAndroid;

// Valid priorities supported by the task scheduling infrastructure.
// Note: internal algorithms depend on priorities being expressed as a
// continuous zero-based list from lowest to highest priority. Users of this API
// shouldn't otherwise care about nor use the underlying values.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.base.task
enum class TaskPriority : uint8_t {
  // This will always be equal to the lowest priority available.
  LOWEST = 0,
  // This task will only be scheduled when machine resources are available. Once
  // running, it may be descheduled if higher priority work arrives (in this
  // process or another) and its running on a non-critical thread.
  BEST_EFFORT = LOWEST,
  // This task affects UI or responsiveness of future user interactions. It is
  // not an immediate response to a user interaction.
  // Examples:
  // - Updating the UI to reflect progress on a long task.
  // - Loading data that might be shown in the UI after a future user
  //   interaction.
  USER_VISIBLE,
  // This task affects UI immediately after a user interaction.
  // Example: Generating data shown in the UI immediately after a click.
  USER_BLOCKING,
  // This will always be equal to the highest priority available.
  HIGHEST = USER_BLOCKING,
};

// Valid shutdown behaviors supported by the thread pool.
enum class TaskShutdownBehavior : uint8_t {
  // Tasks posted with this mode which have not started executing before
  // shutdown is initiated will never run. Tasks with this mode running at
  // shutdown will be ignored (the worker will not be joined).
  //
  // This option provides a nice way to post stuff you don't want blocking
  // shutdown. For example, you might be doing a slow DNS lookup and if it's
  // blocked on the OS, you may not want to stop shutdown, since the result
  // doesn't really matter at that point.
  //
  // However, you need to be very careful what you do in your callback when you
  // use this option. Since the thread will continue to run until the OS
  // terminates the process, the app can be in the process of tearing down when
  // you're running. This means any singletons or global objects you use may
  // suddenly become invalid out from under you. For this reason, it's best to
  // use this only for slow but simple operations like the DNS example.
  CONTINUE_ON_SHUTDOWN,

  // Tasks posted with this mode that have not started executing at
  // shutdown will never run. However, any task that has already begun
  // executing when shutdown is invoked will be allowed to continue and
  // will block shutdown until completion.
  //
  // Note: Because ThreadPoolInstance::Shutdown() may block while these tasks
  // are executing, care must be taken to ensure that they do not block on the
  // thread that called ThreadPoolInstance::Shutdown(), as this may lead to
  // deadlock.
  SKIP_ON_SHUTDOWN,

  // Tasks posted with this mode before shutdown is complete will block shutdown
  // until they're executed. Generally, this should be used only to save
  // critical user data.
  //
  // Note: Background threads will be promoted to normal threads at shutdown
  // (i.e. TaskPriority::BEST_EFFORT + TaskShutdownBehavior::BLOCK_SHUTDOWN will
  // resolve without a priority inversion).
  BLOCK_SHUTDOWN,
};

// Determines at which thread priority a task may run.
enum class ThreadPolicy : uint8_t {
  // The task runs at background thread priority if its TaskPriority is
  // BEST_EFFORT and this is supported (no background thread priority when using
  // a BrowserThread trait or on unsupported platforms - refer to
  // environment_config_unittest.cc). Otherwise, it runs at normal thread
  // priority.
  PREFER_BACKGROUND,

  // The task runs at normal thread priority, irrespective of its TaskPriority.
  //
  // A priority inversion occurs when low priority thread is descheduled while
  // holding a resource needed by a thread of higher priority.
  // ThreadPolicy::MUST_USE_FOREGROUND can be combined with
  // TaskPriority::BEST_EFFORT to indicate that a task:
  // - Can stay in the queue indefinitely when resources are needed for other
  //   tasks.
  // - Once out of the queue, must run at normal thread priority to prevent
  //   priority inversions.
  // Please consult with //base/task/OWNERS if you suspect a priority inversion.
  MUST_USE_FOREGROUND,
};

// Tasks with this trait may block. This includes but is not limited to tasks
// that wait on synchronous file I/O operations: read or write a file from disk,
// interact with a pipe or a socket, rename or delete a file, enumerate files in
// a directory, etc. This trait isn't required for the mere use of locks. For
// tasks that block on base/ synchronization primitives, see the
// WithBaseSyncPrimitives trait.
struct MayBlock {};

// DEPRECATED. Use base::ScopedAllowBaseSyncPrimitives(ForTesting) instead.
//
// Tasks with this trait will pass base::AssertBaseSyncPrimitivesAllowed(), i.e.
// will be allowed on the following methods :
// - base::WaitableEvent::Wait
// - base::ConditionVariable::Wait
// - base::PlatformThread::Join
// - base::PlatformThread::Sleep
// - base::Process::WaitForExit
// - base::Process::WaitForExitWithTimeout
//
// Tasks should generally not use these methods.
//
// Instead of waiting on a WaitableEvent or a ConditionVariable, put the work
// that should happen after the wait in a callback and post that callback from
// where the WaitableEvent or ConditionVariable would have been signaled. If
// something needs to be scheduled after many tasks have executed, use
// base::BarrierClosure.
//
// On Windows, join processes asynchronously using base::win::ObjectWatcher.
//
// MayBlock() must be specified in conjunction with this trait if and only if
// removing usage of methods listed above in the labeled tasks would still
// result in tasks that may block (per MayBlock()'s definition).
//
// In doubt, consult with //base/task/OWNERS.
struct WithBaseSyncPrimitives {};

// Tasks and task runners with this trait will run in the thread pool,
// concurrently with tasks on other task runners. If you need mutual exclusion
// between tasks, see base::PostTask::CreateSequencedTaskRunner.
struct ThreadPool {};

// Describes metadata for a single task or a group of tasks.
class BASE_EXPORT TaskTraits {
 public:
  // ValidTrait ensures TaskTraits' constructor only accepts appropriate types.
  struct ValidTrait {
    ValidTrait(TaskPriority);
    ValidTrait(TaskShutdownBehavior);
    ValidTrait(ThreadPolicy);
    ValidTrait(MayBlock);
    ValidTrait(WithBaseSyncPrimitives);
    ValidTrait(ThreadPool);
  };

  // Invoking this constructor without arguments produces TaskTraits that are
  // appropriate for tasks that
  //     (1) don't block (ref. MayBlock() and WithBaseSyncPrimitives()),
  //     (2) prefer inheriting the current priority to specifying their own, and
  //     (3) can either block shutdown or be skipped on shutdown
  //         (ThreadPoolInstance implementation is free to choose a fitting
  //         default).
  //
  // To get TaskTraits for tasks that require stricter guarantees and/or know
  // the specific TaskPriority appropriate for them, provide arguments of type
  // TaskPriority, TaskShutdownBehavior, ThreadPolicy, MayBlock and/or
  // WithBaseSyncPrimitives in any order to the constructor.
  //
  // E.g.
  // constexpr base::TaskTraits default_traits = {};
  // constexpr base::TaskTraits user_visible_traits =
  //     {base::TaskPriority::USER_VISIBLE};
  // constexpr base::TaskTraits user_visible_may_block_traits = {
  //     base::TaskPriority::USER_VISIBLE, base::MayBlock()};
  // constexpr base::TaskTraits other_user_visible_may_block_traits = {
  //     base::MayBlock(), base::TaskPriority::USER_VISIBLE};
  template <class... ArgTypes,
            class CheckArgumentsAreValid = std::enable_if_t<
                trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>::value ||
                trait_helpers::AreValidTraitsForExtension<ArgTypes...>::value>>
  constexpr TaskTraits(ArgTypes... args)
      : extension_(trait_helpers::GetTaskTraitsExtension(
            trait_helpers::AreValidTraits<ValidTrait, ArgTypes...>{},
            args...)),
        priority_(
            trait_helpers::GetEnum<TaskPriority, TaskPriority::USER_VISIBLE>(
                args...)),
        shutdown_behavior_(
            trait_helpers::GetEnum<TaskShutdownBehavior,
                                   TaskShutdownBehavior::SKIP_ON_SHUTDOWN>(
                args...)),
        thread_policy_(
            trait_helpers::GetEnum<ThreadPolicy,
                                   ThreadPolicy::PREFER_BACKGROUND>(args...)),
        priority_set_explicitly_(
            trait_helpers::HasTrait<TaskPriority>(args...)),
        shutdown_behavior_set_explicitly_(
            trait_helpers::HasTrait<TaskShutdownBehavior>(args...)),
        may_block_(trait_helpers::HasTrait<MayBlock>(args...)),
        with_base_sync_primitives_(
            trait_helpers::HasTrait<WithBaseSyncPrimitives>(args...)),
        use_thread_pool_(trait_helpers::HasTrait<ThreadPool>(args...)) {}

  constexpr TaskTraits(const TaskTraits& other) = default;
  TaskTraits& operator=(const TaskTraits& other) = default;

  // TODO(eseckler): Default the comparison operator once C++20 arrives.
  bool operator==(const TaskTraits& other) const {
    static_assert(sizeof(TaskTraits) == 17,
                  "Update comparison operator when TaskTraits change");
    return extension_ == other.extension_ && priority_ == other.priority_ &&
           shutdown_behavior_ == other.shutdown_behavior_ &&
           thread_policy_ == other.thread_policy_ &&
           priority_set_explicitly_ == other.priority_set_explicitly_ &&
           shutdown_behavior_set_explicitly_ ==
               other.shutdown_behavior_set_explicitly_ &&
           may_block_ == other.may_block_ &&
           with_base_sync_primitives_ == other.with_base_sync_primitives_ &&
           use_thread_pool_ == other.use_thread_pool_;
  }

  // Sets the priority of tasks with these traits to |priority|.
  void UpdatePriority(TaskPriority priority) {
    priority_ = priority;
    priority_set_explicitly_ = true;
  }

  // Sets the priority to |priority| if it wasn't explicitly set before.
  void InheritPriority(TaskPriority priority) {
    if (priority_set_explicitly_)
      return;
    priority_ = priority;
  }

  // Returns true if the priority was set explicitly.
  constexpr bool priority_set_explicitly() const {
    return priority_set_explicitly_;
  }

  // Returns the priority of tasks with these traits.
  constexpr TaskPriority priority() const { return priority_; }

  // Returns true if the shutdown behavior was set explicitly.
  constexpr bool shutdown_behavior_set_explicitly() const {
    return shutdown_behavior_set_explicitly_;
  }

  // Returns the shutdown behavior of tasks with these traits.
  constexpr TaskShutdownBehavior shutdown_behavior() const {
    return shutdown_behavior_;
  }

  // Returns the thread policy of tasks with these traits.
  constexpr ThreadPolicy thread_policy() const { return thread_policy_; }

  // Returns true if tasks with these traits may block.
  constexpr bool may_block() const { return may_block_; }

  // Returns true if tasks with these traits may use base/ sync primitives.
  constexpr bool with_base_sync_primitives() const {
    return with_base_sync_primitives_;
  }

  // Returns true if tasks with these traits execute on the thread pool.
  constexpr bool use_thread_pool() const { return use_thread_pool_; }

  uint8_t extension_id() const { return extension_.extension_id; }

  // Access the extension data by parsing it into the provided extension type.
  // See task_traits_extension.h for requirements on the extension type.
  template <class TaskTraitsExtension>
  const TaskTraitsExtension GetExtension() const {
    DCHECK_EQ(TaskTraitsExtension::kExtensionId, extension_.extension_id);
    return TaskTraitsExtension::Parse(extension_);
  }

 private:
  friend PostTaskAndroid;

  // For use by PostTaskAndroid.
  TaskTraits(bool priority_set_explicitly,
             TaskPriority priority,
             bool shutdown_behavior_set_explicitly,
             TaskShutdownBehavior shutdown_behavior,
             bool may_block,
             bool with_base_sync_primitives,
             bool use_thread_pool,
             TaskTraitsExtensionStorage extension)
      : extension_(extension),
        priority_(priority),
        shutdown_behavior_(shutdown_behavior),
        thread_policy_(ThreadPolicy::PREFER_BACKGROUND),
        priority_set_explicitly_(priority_set_explicitly),
        shutdown_behavior_set_explicitly_(shutdown_behavior_set_explicitly),
        may_block_(may_block),
        with_base_sync_primitives_(with_base_sync_primitives),
        use_thread_pool_(use_thread_pool) {
    static_assert(sizeof(TaskTraits) == 17, "Keep this constructor up to date");
  }

  // Ordered for packing.
  TaskTraitsExtensionStorage extension_;
  TaskPriority priority_;
  TaskShutdownBehavior shutdown_behavior_;
  ThreadPolicy thread_policy_;
  bool priority_set_explicitly_;
  bool shutdown_behavior_set_explicitly_;
  bool may_block_;
  bool with_base_sync_primitives_;
  bool use_thread_pool_;
};

// Returns string literals for the enums defined in this file. These methods
// should only be used for tracing and debugging.
BASE_EXPORT const char* TaskPriorityToString(TaskPriority task_priority);
BASE_EXPORT const char* TaskShutdownBehaviorToString(
    TaskShutdownBehavior task_priority);

// Stream operators so that the enums defined in this file can be used in
// DCHECK and EXPECT statements.
BASE_EXPORT std::ostream& operator<<(std::ostream& os,
                                     const TaskPriority& shutdown_behavior);
BASE_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const TaskShutdownBehavior& shutdown_behavior);

}  // namespace base

#endif  // BASE_TASK_TASK_TRAITS_H_
