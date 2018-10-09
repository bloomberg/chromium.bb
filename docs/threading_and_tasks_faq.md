# Threading and Tasks in Chrome - FAQ

[TOC]

## General

### On what thread will a task run?

A task is posted through the `base/task/post_task.h` API with `TaskTraits`.

* If `TaskTraits` contain `BrowserThread::UI`:
    * The task runs on the main thread.

* If `TaskTraits` contain `BrowserThread::IO`:
    * The task runs on the IO thread.

* If `TaskTraits` don't contain `BrowserThread::UI/IO`:
    * If the task is posted through a `SingleThreadTaskRunner` obtained from
      `CreateSingleThreadTaskRunnerWithTraits(..., mode)`:
        * Where `mode` is `SingleThreadTaskRunnerThreadMode::DEDICATED`:
              * The task runs on a thread that only runs tasks from that
                SingleThreadTaskRunner. This is not the main thread or the IO
                thread.

        * Where `mode` is `SingleThreadTaskRunnerThreadMode::SHARED`:
              * The task runs on a thread that runs tasks from one or many
                unrelated SingleThreadTaskRunners. This is not the main thread
                or the IO thread.

    * Otherwise:
        * The task runs in a thread pool.

As explained in [Prefer Sequences to Threads](threading_and_tasks.md#Prefer-Sequences-to-Threads),
tasks should generally run on a sequence in a thread pool rather than on a
dedicated thread.

## Blocking off-CPU

### How to make a call that may block off-CPU and never return?

If you can't avoid making a call to a third-party library that may block off-CPU
and never return, you must take some steps to ensure that it doesn't prevent
other tasks from running. The steps depend on where the task runs (see [Where
will a task run?](#On-what-thread-will-a-task-run_)).

If the task runs in a thread pool:

* Annotate the scope that may block off-CPU with
  `ScopedBlockingCall(BlockingType::MAY_BLOCK/WILL_BLOCK)`. A few milliseconds
  after the annotated scope is entered, the capacity of the thread pool is
  incremented. This ensures that your task doesn't reduce the number of tasks
  that can run concurrently on the CPU. If the scope exits, the thread pool
  capacity goes back to normal. Since tasks posted to the same sequence can't
  run concurrently, it is advisable to run tasks that may block indefinitely in
  [parallel](threading_and_tasks.md#posting-a-parallel-task) rather than in
  [sequence](threading_and_tasks.md#posting-a-sequenced-task).

If the task runs on the main thread, the IO thread or a `SHARED
SingleThreadTaskRunner`:

* Blocking on one of these threads will cause breakages. Move your task to a
  thread pool (or to a `DEDICATED SingleThreadTaskRunner` if necessary - see
  [Prefer Sequences to Threads](threading_and_tasks.md#Prefer-Sequences-to-Threads)).

If the task runs on a `DEDICATED SingleThreadTaskRunner`:

* Annotate the scope that may block off-CPU with
  `ScopedBlockingCall(BlockingType::MAY_BLOCK/WILL_BLOCK)`. The annotation is a
  no-op that documents the blocking behavior. Tasks posted to the same
  `DEDICATED SingleThreadTaskRunner` won't run until your blocking task returns
  (they will never run if the blocking task never returns).

`[base/threading/scoped_blocking_call.h](https://cs.chromium.org/chromium/src/base/threading/scoped_blocking_call.h)`
explains the difference between `MAY_BLOCK ` and  `WILL_BLOCK` and gives
examples of off-CPU blocking operations.

## Sequences

### How to migrate from SingleThreadTaskRunner to SequencedTaskRunner?

The following mappings can be useful when migrating code from a
`SingleThreadTaskRunner` to a `SequencedTaskRunner`:

* base::SingleThreadTaskRunner -> base::SequencedTaskRunner
    * SingleThreadTaskRunner::BelongsToCurrentThread() -> SequencedTaskRunner::RunsTasksInCurrentSequence()
* base::ThreadTaskRunnerHandle -> base::SequencedTaskRunnerHandle
* THREAD_CHECKER -> SEQUENCE_CHECKER
* base::ThreadLocalStorage::Slot -> base::SequenceLocalStorageSlot
* BrowserThread::DeleteOnThread -> base::OnTaskRunnerDeleter / base::RefCountedDeleteOnSequence
* BrowserMessageFilter::OverrideThreadForMessage() -> BrowserMessageFilter::OverrideTaskRunnerForMessage()
* CreateSingleThreadTaskRunnerWithTraits() -> CreateSequencedTaskRunnerWithTraits()
     * Every CreateSingleThreadTaskRunnerWithTraits() usage should be accompanied
       with a comment and ideally a bug to make it sequence when the sequence-unfriendly
       dependency is addressed.

### How to ensure mutual exclusion between tasks posted by a component?

Create a `SequencedTaskRunner` using `CreateSequencedTaskRunnerWithTraits()` and
store it on an object that can be accessed from all the PostTask() call sites
that require mutual exclusion. If there isn't a shared object that can own
common `SequencedTaskRunner`, use
`Lazy(Sequenced|SingleThread|COMSTA)TaskRunner` in an anonymous namespace.

## Tests

### How to test code that posts tasks?

If the test uses `BrowserThread::UI/IO`, instantiate a
`content::TestBrowserThreadBundle` for the scope of the test. Call
`content::RunAllTasksUntilIdle()` to wait until all tasks have run.

If the test doesn't use `BrowserThread::UI/IO`, instantiate a
`base::test::ScopedTaskEnvironment` for the scope of the test. Call
`base::test::ScopedTaskEnvironment::RunUntilIdle()` to wait until all tasks have
run.

In both cases, you can run tasks until a condition is met. A test that waits for
a condition to be met is easier to understand and debug than a test that waits
for all tasks to run.

```cpp
int g_condition = false;

base::RunLoop run_loop;
base::PostTaskWithTraits(FROM_HERE, {}, base::BindOnce(
    [] (base::OnceClosure closure) {
        g_condition = true;
        std::move(quit_closure).Run();
    }, run_loop.QuitClosure()));

// Runs tasks until the quit closure is invoked.
run_loop.Run();

EXPECT_TRUE(g_condition);
```

## Your question hasn't been answered?

Ping
[scheduler-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/scheduler-dev).
