// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.ThreadUtils;

import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Queue used for running tasks, initiated through WebView APIs, on the UI thread.
 * The queue won't start running tasks until WebView has been initialized properly.
 */
public class WebViewChromiumRunQueue {
    private final Queue<Runnable> mQueue;
    private final ShouldDrainQueueCallable mShouldDrainQueueCallable;

    /**
     * Callable representing whether WebView has been initialized, and we should start running
     * tasks.
     */
    public static interface ShouldDrainQueueCallable { public boolean shouldDrainQueue(); }

    public WebViewChromiumRunQueue(ShouldDrainQueueCallable shouldDrainQueueCallable) {
        mQueue = new ConcurrentLinkedQueue<Runnable>();
        mShouldDrainQueueCallable = shouldDrainQueueCallable;
    }

    /**
     * Add a new task to the queue. If WebView has already been initialized the task will be run
     * ASAP.
     */
    public void addTask(Runnable task) {
        mQueue.add(task);
        if (mShouldDrainQueueCallable.shouldDrainQueue()) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    drainQueue();
                }
            });
        }
    }

    /**
     * Drain the queue, i.e. perform all the tasks in the queue.
     */
    public void drainQueue() {
        if (mQueue == null || mQueue.isEmpty()) {
            return;
        }

        Runnable task = mQueue.poll();
        while (task != null) {
            task.run();
            task = mQueue.poll();
        }
    }
}
