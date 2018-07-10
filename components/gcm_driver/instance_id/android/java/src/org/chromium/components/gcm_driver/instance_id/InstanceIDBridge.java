// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import android.os.Bundle;

import org.chromium.base.AsyncTask;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/**
 * Wraps InstanceIDWithSubtype so it can be used over JNI.
 * Performs disk/network operations on a background thread and replies asynchronously.
 */
@JNINamespace("instance_id")
public class InstanceIDBridge {
    private final String mSubtype;
    private long mNativeInstanceIDAndroid;
    /**
     * Underlying InstanceIDWithSubtype. May be shared by multiple InstanceIDBridges. Must be
     * initialized on a background thread.
     */
    private InstanceIDWithSubtype mInstanceID;

    private static boolean sBlockOnAsyncTasksForTesting;

    private InstanceIDBridge(long nativeInstanceIDAndroid, String subtype) {
        mSubtype = subtype;
        mNativeInstanceIDAndroid = nativeInstanceIDAndroid;
    }

    /**
     * Returns a wrapped {@link InstanceIDWithSubtype}. Multiple InstanceIDBridge instances may
     * share an underlying InstanceIDWithSubtype.
     */
    @CalledByNative
    public static InstanceIDBridge create(long nativeInstanceIDAndroid, String subtype) {
        return new InstanceIDBridge(nativeInstanceIDAndroid, subtype);
    }

    /**
     * Called when our C++ counterpart is destroyed. Clears the handle to our native C++ object,
     * ensuring it's not called by pending async tasks.
     */
    @CalledByNative
    private void destroy() {
        mNativeInstanceIDAndroid = 0;
    }

    /**
     * If enabled, methods that are usually asynchronous will block returning the control flow to
     * C++ until the asynchronous Java operation has completed. Used this in tests where the Java
     * message loop is not nested. The caller is expected to reset this to false when tearing down.
     */
    @CalledByNative
    private static boolean setBlockOnAsyncTasksForTesting(boolean block) {
        boolean wasBlocked = sBlockOnAsyncTasksForTesting;
        sBlockOnAsyncTasksForTesting = block;
        return wasBlocked;
    }

    /** Async wrapper for {@link InstanceID#getId}. */
    @CalledByNative
    public void getId(final int requestId) {
        new BridgeAsyncTask<String>() {
            @Override
            protected String doBackgroundWork() {
                return mInstanceID.getId();
            }
            @Override
            protected void sendResultToNative(String id) {
                nativeDidGetID(mNativeInstanceIDAndroid, requestId, id);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#getCreationTime}. */
    @CalledByNative
    public void getCreationTime(final int requestId) {
        new BridgeAsyncTask<Long>() {
            @Override
            protected Long doBackgroundWork() {
                return mInstanceID.getCreationTime();
            }
            @Override
            protected void sendResultToNative(Long creationTime) {
                nativeDidGetCreationTime(mNativeInstanceIDAndroid, requestId, creationTime);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#getToken(String, String, Bundle)}. */
    @CalledByNative
    private void getToken(final int requestId, final String authorizedEntity, final String scope,
            String[] extrasStrings) {
        final Bundle extras = new Bundle();
        assert extrasStrings.length % 2 == 0;
        for (int i = 0; i < extrasStrings.length; i += 2) {
            extras.putString(extrasStrings[i], extrasStrings[i + 1]);
        }
        new BridgeAsyncTask<String>() {
            @Override
            protected String doBackgroundWork() {
                try {
                    return mInstanceID.getToken(authorizedEntity, scope, extras);
                } catch (IOException ex) {
                    return "";
                }
            }
            @Override
            protected void sendResultToNative(String token) {
                nativeDidGetToken(mNativeInstanceIDAndroid, requestId, token);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteToken(String, String)}. */
    @CalledByNative
    private void deleteToken(
            final int requestId, final String authorizedEntity, final String scope) {
        new BridgeAsyncTask<Boolean>() {
            @Override
            protected Boolean doBackgroundWork() {
                try {
                    mInstanceID.deleteToken(authorizedEntity, scope);
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }
            @Override
            protected void sendResultToNative(Boolean success) {
                nativeDidDeleteToken(mNativeInstanceIDAndroid, requestId, success);
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteInstanceID}. */
    @CalledByNative
    private void deleteInstanceID(final int requestId) {
        new BridgeAsyncTask<Boolean>() {
            @Override
            protected Boolean doBackgroundWork() {
                try {
                    mInstanceID.deleteInstanceID();
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }
            @Override
            protected void sendResultToNative(Boolean success) {
                nativeDidDeleteID(mNativeInstanceIDAndroid, requestId, success);
            }
        }.execute();
    }

    private native void nativeDidGetID(long nativeInstanceIDAndroid, int requestId, String id);
    private native void nativeDidGetCreationTime(
            long nativeInstanceIDAndroid, int requestId, long creationTime);
    private native void nativeDidGetToken(
            long nativeInstanceIDAndroid, int requestId, String token);
    private native void nativeDidDeleteToken(
            long nativeInstanceIDAndroid, int requestId, boolean success);
    private native void nativeDidDeleteID(
            long nativeInstanceIDAndroid, int requestId, boolean success);

    /**
     * Custom {@link AsyncTask} wrapper. As usual, this performs work on a background thread, then
     * sends the result back on the UI thread. Key differences:
     * 1. Lazily initializes mInstanceID before running doBackgroundWork.
     * 2. sendResultToNative will be skipped if the C++ counterpart has been destroyed.
     * 3. Tasks run in parallel (using THREAD_POOL_EXECUTOR) to avoid blocking other Chrome tasks.
     * 4. If setBlockOnAsyncTasksForTesting has been enabled, executing the task will block the UI
     *    thread, then directly call sendResultToNative. This is a workaround for use in tests
     *    that lack a nested Java message loop (which prevents onPostExecute from running).
     */
    private abstract class BridgeAsyncTask<Result> {
        protected abstract Result doBackgroundWork();

        protected abstract void sendResultToNative(Result result);

        public void execute() {
            AsyncTask<Void, Void, Result> task = new AsyncTask<Void, Void, Result>() {
                @Override
                @SuppressWarnings("NoSynchronizedThisCheck") // Only used/accessible by native.
                protected Result doInBackground(Void... params) {
                    synchronized (InstanceIDBridge.this) {
                        if (mInstanceID == null) {
                            mInstanceID = InstanceIDWithSubtype.getInstance(mSubtype);
                        }
                    }
                    return doBackgroundWork();
                }
                @Override
                protected void onPostExecute(Result result) {
                    if (!sBlockOnAsyncTasksForTesting && mNativeInstanceIDAndroid != 0) {
                        sendResultToNative(result);
                    }
                }
            };
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            if (sBlockOnAsyncTasksForTesting) {
                Result result;
                try {
                    // Synchronously block the UI thread until doInBackground returns result.
                    result = task.get();
                } catch (InterruptedException | ExecutionException e) {
                    throw new IllegalStateException(e); // Shouldn't happen in tests.
                }
                sendResultToNative(result);
            }
        }
    }
}
