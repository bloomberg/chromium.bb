// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import java.util.HashMap;
import java.util.Map;

/**
 * The default {@link TaskExecutor} which maps directly to base::TaskScheduler.
 */
class DefaultTaskExecutor implements TaskExecutor {
    Map<TaskTraits, TaskRunner> mTraitsToRunnerMap = new HashMap<>();

    @Override
    public TaskRunner createTaskRunner(TaskTraits taskTraits) {
        return new TaskRunnerImpl(taskTraits);
    }

    @Override
    public SequencedTaskRunner createSequencedTaskRunner(TaskTraits taskTraits) {
        return new SequencedTaskRunnerImpl(taskTraits);
    }

    /**
     * This maps to a single thread within the native thread pool. Due to that contract we
     * can't run tasks posted on it until native has started.
     */
    @Override
    public SingleThreadTaskRunner createSingleThreadTaskRunner(TaskTraits taskTraits) {
        // Tasks posted via this API will not execute until after native has started.
        return new SingleThreadTaskRunnerImpl(null, taskTraits);
    }

    @Override
    public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
        if (taskTraits.hasExtension()) {
            TaskRunner runner = createTaskRunner(taskTraits);
            runner.postDelayedTask(task, delay);
            runner.destroy();
        } else {
            // Caching TaskRunners only for common TaskTraits.
            TaskRunner runner = mTraitsToRunnerMap.get(taskTraits);
            if (runner == null) {
                TaskRunnerImpl runnerImpl = new TaskRunnerImpl(taskTraits);
                // Disable destroy() check since object will live forever.
                runnerImpl.disableLifetimeCheck();
                mTraitsToRunnerMap.put(taskTraits, runnerImpl);
                runner = runnerImpl;
            }
            runner.postDelayedTask(task, delay);
        }
    }

    @Override
    public boolean canRunTaskImmediately(TaskTraits traits) {
        return false;
    }
}
