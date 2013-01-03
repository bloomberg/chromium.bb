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
                    Message.obtain(sHandler, CLEANUP_REFS).sendToTarget();
                    // Create a new detector since this one has now been GC'd.
                    createGarbageCollectionDetector();
                } finally {
                    super.finalize();
                }
            }
        };
    }

    // Message's sent in the |what| field to |sHandler|.

    // Add a new reference to sRefs. |msg.obj| is the CleanupReference to add.
    static final int ADD_REF = 1;
    // Remove reference from sRefs. |msg.obj| is the CleanupReference to remove.
    static final int REMOVE_REF = 2;
    // Flush out pending items on the reference queue (i.e. run the finalizer actions).
    static final int CLEANUP_REFS = 3;

    /**
     * This {@link Handler} polls {@link #sRefs}, looking for cleanup tasks that
     * are ready to run.
     */
    private static Handler sHandler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(android.os.Message msg) {
            TraceEvent.begin();
            CleanupReference ref = (CleanupReference) msg.obj;
            switch (msg.what) {
                case ADD_REF:
                    sRefs.add(ref);
                    break;
                case REMOVE_REF:
                    ref.runCleanupTaskInternal();
                    break;
                case CLEANUP_REFS:
                    break; // Handled below
            }

            // Always run the cleanup loop here even when adding or removing refs, to avoid
            // falling behind on rapid allocation/GC inner loops.
            while ((ref = (CleanupReference) sQueue.poll()) != null) {
                ref.runCleanupTaskInternal();
            }
            TraceEvent.end();
        }
    };

    private static ReferenceQueue<Object> sQueue = new ReferenceQueue<Object>();

    /**
     * Keep a strong reference to {@link CleanupReference} so that it will
     * actually get enqueued.
     * Only accessed on the UI thread.
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
        handleOnUiThread(ADD_REF);
    }

    /**
     * Clear the cleanup task {@link Runnable} so that nothing will be done
     * after garbage collection.
     */
    public void cleanupNow() {
        handleOnUiThread(REMOVE_REF);
    }

    private void handleOnUiThread(int what) {
        Message msg = Message.obtain(sHandler, what, this);
        if (Looper.myLooper() == sHandler.getLooper()) {
            sHandler.handleMessage(msg);
        } else {
            msg.sendToTarget();
        }
    }

    private void runCleanupTaskInternal() {
        sRefs.remove(this);
        if (mCleanupTask != null) {
            mCleanupTask.run();
            mCleanupTask = null;
        }
        clear();
    }
}
