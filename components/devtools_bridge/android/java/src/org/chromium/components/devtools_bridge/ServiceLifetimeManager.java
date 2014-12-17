// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.app.IntentService;
import android.app.Service;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;

/**
 * Tracks count of running tasks and stops the service when all tasks completed. Supposed to be
 * a part of a service.
 *
 * Usage:
 * class MyService extends Service {
 *     public int onStartCommand(Intent intent, int flags, int startId) {
 *         Runnable intentHandlingTask = mLifetimeManager.startTask(startId);
 *         ...
 *         intentHandlingTask.run(); // Stops self if no other task started.
 *         return START_NOT_STICKY;
 *     }
 * }
 */
class ServiceLifetimeManager {
    private final Service mService;
    private final String mWakeLockKey;
    private int mLastStartId = -1;
    private int mActiveTasks = 0;
    private PowerManager.WakeLock mWakeLock;

    public ServiceLifetimeManager(Service service, String wakeLockKey) {
        // IntentService is incompatible with this class because it manages lifetime on its own.
        assert !(service instanceof IntentService);

        mService = service;
        mWakeLockKey = wakeLockKey;
    }

    public Runnable startTask(int startId) {
        mLastStartId = startId;
        return startTask();
    }

    public Runnable startTask() {
        assert isCalledOnServiceLooper();
        if (mActiveTasks == 0) {
            if (mWakeLock == null) {
                PowerManager pm = (PowerManager) mService.getSystemService(Context.POWER_SERVICE);
                mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, mWakeLockKey);
            }
            mWakeLock.acquire();
        }
        mActiveTasks++;
        return new TaskCompletionHandler();
    }

    private boolean isCalledOnServiceLooper() {
        return mService.getMainLooper() == Looper.myLooper();
    }

    private class TaskCompletionHandler implements Runnable {
        private boolean mCompleted = false;

        @Override
        public void run() {
            if (!isCalledOnServiceLooper()) {
                new Handler(mService.getMainLooper()).post(this);
                return;
            }
            if (mCompleted) return;

            if (--mActiveTasks == 0) {
                mWakeLock.release();
                mService.stopSelf(mLastStartId);
            }
            mCompleted = true;
        }
    }
}
