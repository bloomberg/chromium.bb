// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.Log;
import android.util.SparseArray;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;

/**
 * Manages oom bindings used to bound child services. "Oom binding" is a binding that raises the
 * process oom priority so that it shouldn't be killed by the OS out-of-memory killer under
 * normal conditions (it can still be killed under drastic memory pressure). ChildProcessConnections
 * have two oom bindings: initial binding and strong binding.
 *
 * This class receives calls that signal visibility of each service (setInForeground()) and the
 * entire embedding application (onSentToBackground(), onBroughtToForeground()) and manipulates
 * child process bindings accordingly.
 *
 * In particular, the class is responsible for:
 * - removing the initial binding of a service when its visibility is determined for the first time
 * - addition and (possibly delayed) removal of a strong binding when service visibility changes
 * - dropping the current oom bindings when a new connection is started on a low-memory device
 * - keeping a strong binding on the foreground service while the entire application is in
 *   background
 *
 * Thread-safety: most of the methods will be called only on the main thread, exceptions are
 * explicitly noted.
 */
class BindingManager {
    private static final String TAG = "BindingManager";

    // Delay of 1 second used when removing the initial oom binding of a process.
    private static final long REMOVE_INITIAL_BINDING_DELAY_MILLIS = 1 * 1000;

    // Delay of 1 second used when removing temporary strong binding of a process (only on
    // non-low-memory devices).
    private static final long DETACH_AS_ACTIVE_HIGH_END_DELAY_MILLIS = 1 * 1000;

    // These fields allow to override the parameters for testing - see
    // createBindingManagerForTesting().
    private final long mRemoveInitialBindingDelay;
    private final long mRemoveStrongBindingDelay;
    private final boolean mIsLowMemoryDevice;

    /**
     * Wraps ChildProcessConnection keeping track of additional information needed to manage the
     * bindings of the connection. The reference to ChildProcessConnection is cleared when the
     * connection goes away, but ManagedConnection itself is kept (until overwritten by a new entry
     * for the same pid).
     */
    private class ManagedConnection {
        // Set in constructor, cleared in clearConnection().
        private ChildProcessConnection mConnection;

        // True iff there is a strong binding kept on the service because it is working in
        // foreground.
        private boolean mInForeground;

        // True iff there is a strong binding kept on the service because it was bound for the
        // application background period.
        private boolean mBoundForBackgroundPeriod;

        // When mConnection is cleared, oom binding status is stashed here.
        private boolean mWasOomProtected;

        /** Removes the initial service binding. */
        private void removeInitialBinding() {
            final ChildProcessConnection connection = mConnection;
            if (connection == null || !connection.isInitialBindingBound()) return;

            ThreadUtils.postOnUiThreadDelayed(new Runnable() {
                @Override
                public void run() {
                    if (connection.isInitialBindingBound()) {
                        connection.removeInitialBinding();
                    }
                }
            }, mRemoveInitialBindingDelay);
        }

        /** Adds a strong service binding. */
        private void addStrongBinding() {
            ChildProcessConnection connection = mConnection;
            if (connection == null) return;

            connection.addStrongBinding();
        }

        /** Removes a strong service binding. */
        private void removeStrongBinding() {
            final ChildProcessConnection connection = mConnection;
            // We have to fail gracefully if the strong binding is not present, as on low-end the
            // binding could have been removed by dropOomBindings() when a new service was started.
            if (connection == null || !connection.isStrongBindingBound()) return;

            // This runnable performs the actual unbinding. It will be executed synchronously when
            // on low-end devices and posted with a delay otherwise.
            Runnable doUnbind = new Runnable() {
                @Override
                public void run() {
                    if (connection.isStrongBindingBound()) {
                        connection.removeStrongBinding();
                    }
                }
            };

            if (mIsLowMemoryDevice) {
                doUnbind.run();
            } else {
                ThreadUtils.postOnUiThreadDelayed(doUnbind, mRemoveStrongBindingDelay);
            }
        }

        /**
         * Drops the service bindings. This is used on low-end to drop bindings of the current
         * service when a new one is created.
         */
        private void dropBindings() {
            assert mIsLowMemoryDevice;
            ChildProcessConnection connection = mConnection;
            if (connection == null) return;

            connection.dropOomBindings();
        }

        ManagedConnection(ChildProcessConnection connection) {
            mConnection = connection;
        }

        /**
         * Sets the visibility of the service, adding or removing the strong binding as needed. This
         * also removes the initial binding, as the service visibility is now known.
         */
        void setInForeground(boolean nextInForeground) {
            if (!mInForeground && nextInForeground) {
                addStrongBinding();
            } else if (mInForeground && !nextInForeground) {
                removeStrongBinding();
            }

            removeInitialBinding();
            mInForeground = nextInForeground;
        }

        /**
         * Sets or removes additional binding when the service is main service during the embedder
         * background period.
         */
        void setBoundForBackgroundPeriod(boolean nextBound) {
            if (!mBoundForBackgroundPeriod && nextBound) {
                addStrongBinding();
            } else if (mBoundForBackgroundPeriod && !nextBound) {
                removeStrongBinding();
            }

            mBoundForBackgroundPeriod = nextBound;
        }

        boolean isOomProtected() {
            // When a process crashes, we can be queried about its oom status before or after the
            // connection is cleared. For the latter case, the oom status is stashed in
            // mWasOomProtected.
            return mConnection != null ?
                    mConnection.isOomProtectedOrWasWhenDied() : mWasOomProtected;
        }

        void clearConnection() {
            mWasOomProtected = mConnection.isOomProtectedOrWasWhenDied();
            mConnection = null;
        }

        /** @return true iff the reference to the connection is no longer held */
        @VisibleForTesting
        boolean isConnectionCleared() {
            return mConnection == null;
        }
    }

    // This can be manipulated on different threads, synchronize access on mManagedConnections.
    private final SparseArray<ManagedConnection> mManagedConnections =
            new SparseArray<ManagedConnection>();

    // The connection that was most recently set as foreground (using setInForeground()). This is
    // used to add additional binding on it when the embedder goes to background. On low-end, this
    // is also used to drop process bidnings when a new one is created, making sure that only one
    // renderer process at a time is protected from oom killing.
    private ManagedConnection mLastInForeground;

    // Synchronizes operations that access mLastInForeground: setInForeground() and
    // addNewConnection().
    private final Object mLastInForegroundLock = new Object();

    // The connection bound with additional binding in onSentToBackground().
    private ManagedConnection mBoundForBackgroundPeriod;

    /**
     * The constructor is private to hide parameters exposed for testing from the regular consumer.
     * Use factory methods to create an instance.
     */
    private BindingManager(boolean isLowMemoryDevice, long removeInitialBindingDelay,
            long removeStrongBindingDelay) {
        mIsLowMemoryDevice = isLowMemoryDevice;
        mRemoveInitialBindingDelay = removeInitialBindingDelay;
        mRemoveStrongBindingDelay = removeStrongBindingDelay;
    }

    public static BindingManager createBindingManager() {
        return new BindingManager(SysUtils.isLowEndDevice(), REMOVE_INITIAL_BINDING_DELAY_MILLIS,
                DETACH_AS_ACTIVE_HIGH_END_DELAY_MILLIS);
    }

    /**
     * Creates a testing instance of BindingManager. Testing instance will have the unbinding delays
     * set to 0, so that the tests don't need to deal with actual waiting.
     * @param isLowEndDevice true iff the created instance should apply low-end binding policies
     */
    public static BindingManager createBindingManagerForTesting(boolean isLowEndDevice) {
        return new BindingManager(isLowEndDevice, 0, 0);
    }

    /**
     * Registers a freshly started child process. On low-memory devices this will also drop the
     * oom bindings of the last process that was oom-bound. We can do that, because every time a
     * connection is created on the low-end, it is used in foreground (no prerendering, no
     * loading of tabs opened in background). This can be called on any thread.
     * @param pid handle of the service process
     */
    void addNewConnection(int pid, ChildProcessConnection connection) {
        synchronized (mLastInForegroundLock) {
            if (mIsLowMemoryDevice && mLastInForeground != null) mLastInForeground.dropBindings();
        }

        // This will reset the previous entry for the pid in the unlikely event of the OS
        // reusing renderer pids.
        synchronized (mManagedConnections) {
            mManagedConnections.put(pid, new ManagedConnection(connection));
        }
    }

    /**
     * Called when the service visibility changes or is determined for the first time.
     * @param pid handle of the service process
     * @param inForeground true iff the service is visibile to the user
     */
    void setInForeground(int pid, boolean inForeground) {
        ManagedConnection managedConnection;
        synchronized (mManagedConnections) {
            managedConnection = mManagedConnections.get(pid);
        }

        if (managedConnection == null) {
            Log.w(TAG, "Cannot setInForeground() - never saw a connection for the pid: " +
                    Integer.toString(pid));
            return;
        }

        synchronized (mLastInForegroundLock) {
            managedConnection.setInForeground(inForeground);
            if (inForeground) mLastInForeground = managedConnection;
        }
    }

    /**
     * Called when the embedding application is sent to background. We want to maintain a strong
     * binding on the most recently used renderer while the embedder is in background, to indicate
     * the relative importance of the renderer to system oom killer.
     *
     * The embedder needs to ensure that:
     *  - every onBroughtToForeground() is followed by onSentToBackground()
     *  - pairs of consecutive onBroughtToForeground() / onSentToBackground() calls do not overlap
     */
    void onSentToBackground() {
        assert mBoundForBackgroundPeriod == null;
        synchronized (mLastInForegroundLock) {
            // mLastInForeground can be null at this point as the embedding application could be
            // used in foreground without spawning any renderers.
            if (mLastInForeground != null) {
                mLastInForeground.setBoundForBackgroundPeriod(true);
                mBoundForBackgroundPeriod = mLastInForeground;
            }
        }
    }

    /**
     * Called when the embedding application is brought to foreground. This will drop the strong
     * binding kept on the main renderer during the background period, so the embedder should make
     * sure that this is called after the regular strong binding is attached for the foreground
     * session.
     */
    void onBroughtToForeground() {
        if (mBoundForBackgroundPeriod != null) {
            mBoundForBackgroundPeriod.setBoundForBackgroundPeriod(false);
            mBoundForBackgroundPeriod = null;
        }
    }

    /**
     * @return True iff the given service process is protected from the out-of-memory killing, or it
     * was protected when it died unexpectedly. This can be used to decide if a disconnection of a
     * renderer was a crash or a probable out-of-memory kill. This can be called on any thread.
     */
    boolean isOomProtected(int pid) {
        // In the unlikely event of the OS reusing renderer pid, the call will refer to the most
        // recent renderer of the given pid. The binding state for a pid is being reset in
        // addNewConnection().
        ManagedConnection managedConnection;
        synchronized (mManagedConnections) {
            managedConnection = mManagedConnections.get(pid);
        }
        return managedConnection != null ? managedConnection.isOomProtected() : false;
    }

    /**
     * Should be called when the connection to the child process goes away (either after a clean
     * exit or an unexpected crash). At this point we let go of the reference to the
     * ChildProcessConnection. This can be called on any thread.
     */
    void clearConnection(int pid) {
        ManagedConnection managedConnection;
        synchronized (mManagedConnections) {
            managedConnection = mManagedConnections.get(pid);
        }
        if (managedConnection != null) managedConnection.clearConnection();
    }

    /** @return true iff the connection reference is no longer held */
    @VisibleForTesting
    boolean isConnectionCleared(int pid) {
        synchronized (mManagedConnections) {
            return mManagedConnections.get(pid).isConnectionCleared();
        }
    }
}
