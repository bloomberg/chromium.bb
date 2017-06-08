# TaskScheduler Migration

[TOC]

## Overview

[`base/task_scheduler/post_task.h`](https://cs.chromium.org/chromium/src/base/task_scheduler/post_task.h)
was introduced to Chrome in Q1. The API is fully documented under [Threading and
Tasks in Chrome](threading_and_tasks.md). This page will go into more details
about how to migrate callers of existing APIs to TaskScheduler.

Much of the migration has already been automated but the callers that remain
require manual intervention from the OWNERS.

## BlockingPool (and other SequencedWorkerPools)

The remaining callers of BrowserThread::GetBlockingPool() require manual
intervention because they're plumbing the SequencedWorkerPool multiple layers
into another component.

The TaskScheduler API explicitly discourages this paradigm. Instead exposing a
static API from post_task.h and encouraging that individual components grab the
TaskRunner/TaskTraits they need instead of bring injected one from their owner
and hoping for the right traits. This often allows cleaning up multiple layers
of plumbing without otherwise hurting testing as documented
[here](threading_and_tasks.md#TaskRunner-ownership).

## BrowserThreads

All BrowserThreads but UI/IO are being migrated to TaskScheduler
(i.e. FILE/FILE_USER_BLOCKING/DB/PROCESS_LAUNCHER/CACHE).

This migration requires manual intervention because:
 1. Everything on BrowserThread::FOO has to be assumed to depend on being
    sequenced with everything else on BrowserThread::FOO until decided otherwise
    by a developer.
 2. Everything on BrowserThread::FOO has to be assumed to be thread-affine until
    decided otherwise by a developer.

As a developer your goal is to get rid of all uses of BrowserThread::FOO in your assigned files by:
 1. Splitting things into their own execution sequence (i.e. post to a TaskRunner
    obtained from post_task.h -- see [Threading and Tasks in
    Chrome](threading_and_tasks.md) for details).
 2. Ideally migrating from a single-threaded context to a [much preferred]
    (threading_and_tasks.md#Prefer-Sequences-to-Threads) sequenced context.
    * Note: if your tasks use COM APIs (Component Object Model on Windows),
      you'll need to use CreateCOMSTATaskRunnerWithTraits() and sequencing will
      not be an option.

## Relevant single-thread -> sequence mappings

* base::SingleThreadTaskRunner -> base::SequencedTaskRunner
* base::ThreadTaskRunnerHandle -> base::SequencedTaskRunnerHandle
* base::ThreadChecker -> base::SequenceChecker
* BrowserThread::DeleteOnThread -> base::DeleteOnTaskRunner / base::RefCountedDeleteOnSequence
* CreateSingleThreadTaskRunnerWithTraits() -> CreateSequencedTaskRunnerWithTraits()
   * Every CreateSingleThreadTaskRunnerWithTraits() usage should be accompanied
     with a comment and ideally a bug to make it sequence when the sequence-unfriendly
     dependency is addressed (again [Prefer Sequences to
     Threads](threading_and_tasks.md#Prefer-Sequences-to-Threads)).

### Other relevant mappings for tests:

* base::MessageLoop -> base::test::ScopedTaskEnvironment
* content::TestBrowserThread -> content::TestBrowserThreadBundle (if you still
  need other BrowserThreads and ScopedTaskEnvironment if you don't)
* base::RunLoop().Run() -(maybe)> content::RunAllBlockingPoolTasksUntilIdle()
   * If test code was previously using RunLoop to execute things off the main
     thread (as TestBrowserThreadBundle grouped everything under a single
     MessageLoop), flushing tasks will now require asking for that explicitly.
   * Or ScopedTaskEnvironment::RunUntilIdle() if you're not using
     TestBrowserThreadBundle.
   * If you need to control the order of execution of main thread versus
     scheduler you can individually RunLoop.Run() and
     TaskScheduler::FlushForTesting()
   * If you need the TaskScheduler to not run anything until explicitly asked to
     use ScopedTaskEnvironment::ExecutionMode::QUEUED.
