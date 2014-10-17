// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.util;

import android.content.Context;
import android.os.Handler;

import org.chromium.components.devtools_bridge.SessionBase;

/**
 * Implementation of SessionBase.Executor on top of android's handler.
 */
public class LooperExecutor implements SessionBase.Executor {
    private final Handler mHandler;

    public LooperExecutor(Handler handler) {
        mHandler = handler;
    }

    public static LooperExecutor newInstanceForMainLooper(Context context) {
        return new LooperExecutor(new Handler(context.getMainLooper()));
    }

    @Override
    public SessionBase.Cancellable postOnSessionThread(int delayMs, Runnable runnable) {
        CancellableTask task = new CancellableTask(runnable);
        mHandler.postDelayed(task, delayMs);
        return task;
    }

    @Override
    public boolean isCalledOnSessionThread() {
        return mHandler.getLooper().getThread() == Thread.currentThread();
    }

    private final class CancellableTask implements SessionBase.Cancellable, Runnable {
        private Runnable mTask;

        public CancellableTask(Runnable task) {
            mTask = task;
        }

        @Override
        public void run() {
            if (mTask != null) mTask.run();
        }

        @Override
        public void cancel() {
            mHandler.removeCallbacks(this);
            mTask = null;
        }
    }
}
