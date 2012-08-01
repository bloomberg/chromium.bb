// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.util.HashSet;
import java.util.Set;

/**
 * Handles running cleanup tasks after an object has been GC'd. Cleanup tasks
 * are always executed on the main thread. In general, classes should not have
 * finalizers and likewise should not use this class for the same reasons. The
 * exception is where public APIs exist that require native side resources to be
 * cleaned up in response to java side GC of API objects. (Private/internal
 * interfaces should always favor explicit resource releases / destroy()
 * protocol for this rather than depend on GC to trigger native cleanup).
 */
public class CleanupReference extends PhantomReference<Object> {

    static {
        // Bootstrap the cleanup trigger.
        createGarbageCollectionDetector();
    }

    /**
     * Create a scratch object with a finalizer that triggers cleanup.
     */
    private static void createGarbageCollectionDetector() {
        new Object() {
            @Override
            protected void finalize() throws Throwable {
                try {
                    Message.obtain(sHandler).sendToTarget();
                    // Create a new detector since this one has now been GC'd.
                    createGarbageCollectionDetector();
                } finally {
                    super.finalize();
                }
            }
        };
    }

    /**
     * This {@link Handler} polls {@link #sRefs}, looking for cleanup tasks that
     * are ready to run.
     */
    private static Handler sHandler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(android.os.Message msg) {
            TraceEvent.begin();
            CleanupReference ref = null;
            while ((ref = (CleanupReference) sQueue.poll()) != null) {
                ref.cleanupNow();
                ref.clear();
            }
            TraceEvent.end();
        }
    };

    private static ReferenceQueue<Object> sQueue = new ReferenceQueue<Object>();

    /**
     * Keep a strong reference to {@link CleanupReference} so that it will
     * actually get enqueued.
     */
    private static Set<CleanupReference> sRefs = new HashSet<CleanupReference>();

    private Runnable mCleanupTask;

    /**
     * @param obj the object whose loss of reachability should trigger the
     *            cleanup task.
     * @param cleanupTask the task to run once obj loses reachability.
     */
    public CleanupReference(Object obj, Runnable cleanupTask) {
        super(obj, sQueue);
        mCleanupTask = cleanupTask;
        sRefs.add(this);
        // Proactively force a cleanup to catch anything that has been enqueued
        // but is still waiting for the GC detector's finalizer to run.
        // Note this could still run behind (if the main thread is consistently too
        // busy to service messages) but we could investigate synchronously flushing the
        // reference queue if this method is itself being called on the main thread.
        Message.obtain(sHandler).sendToTarget();
    }

    /**
     * Clear the cleanup task {@link Runnable} so that nothing will be done
     * after garbage collection.
     */
    public void cleanupNow() {
        if (mCleanupTask != null) {
            mCleanupTask.run();
            mCleanupTask = null;
        }
        sRefs.remove(this);
    }
}
