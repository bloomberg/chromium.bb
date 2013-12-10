// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

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
 * This class serves as a proxy between external calls that manipulate the bindings and the
 * connections, allowing to enforce policies:
 * - delayed removal of the oom bindings
 * - dropping the current oom bindings when a new connection is started on a low-memory device
 *
 * Thread-safety: most of the methods will be called only on the main thread, exceptions are
 * explicitly noted.
 */
class BindingManager {
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
    private static class ManagedConnection {
        private ChildProcessConnection mConnection;

        // When mConnection is cleared, oom binding status is stashed here.
        private boolean mWasOomProtected;

        ManagedConnection(ChildProcessConnection connection) {
            mConnection = connection;
        }

        ChildProcessConnection getConnection() {
            return mConnection;
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
    }

    // This can be manipulated on different threads, synchronize access on mManagedConnections.
    private final SparseArray<ManagedConnection> mManagedConnections =
            new SparseArray<ManagedConnection>();

    /**
     * A helper method that returns the bare ChildProcessConnection for the given pid, returning
     * null with a log message if there is no entry for the pid.
     *
     * This is visible for testing, so that we can verify that we are not holding onto the reference
     * when we shouldn't.
     */
    @VisibleForTesting
    ChildProcessConnection getConnectionForPid(int pid) {
        ManagedConnection managedConnection;
        synchronized (mManagedConnections) {
            managedConnection = mManagedConnections.get(pid);
        }
        if (managedConnection == null) {
            ChildProcessLauncher.logPidWarning(pid,
                    "BindingManager never saw a connection for the pid: " + Integer.toString(pid));
            return null;
        }

        return managedConnection.getConnection();
    }

    // Pid of the renderer that was most recently bound using bindAsHighPriority(). This is used on
    // low-memory devices to drop oom bindings of a process when another one acquires them, making
    // sure that only one renderer process at a time is oom bound.
    private int mLastOomPid = -1;

    // Pid of the renderer that is bound with a strong binding for the background period. Equals
    // -1 when there is no such renderer.
    private int mBoundForBackgroundPeriodPid = -1;

    // Synchronizes operations that access mLastOomPid: addNewConnection() and bindAsHighPriority().
    private final Object mLastOomPidLock = new Object();

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
     * @param pid handle of the process.
     */
    void addNewConnection(int pid, ChildProcessConnection connection) {
        synchronized (mLastOomPidLock) {
            if (mIsLowMemoryDevice && mLastOomPid >= 0) {
                ChildProcessConnection lastOomConnection = getConnectionForPid(mLastOomPid);
                if (lastOomConnection != null) lastOomConnection.dropOomBindings();
            }
        }
        // This will reset the previous entry for the pid in the unlikely event of the OS
        // reusing renderer pids.
        synchronized (mManagedConnections) {
            mManagedConnections.put(pid, new ManagedConnection(connection));
        }
    }

    /**
     * Remove the initial binding of the child process. Child processes are bound with initial
     * binding to protect them from getting killed before they are put to use. This method
     * allows to remove the binding once it is no longer needed. The binding is removed after a
     * fixed delay period so that the renderer will not be killed immediately after the call.
     */
    void removeInitialBinding(final int pid) {
        final ChildProcessConnection connection = getConnectionForPid(pid);
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

    /**
     * Binds a child process as a high priority process so that it has the same priority as the main
     * process. This can be used for the foreground renderer process to distinguish it from the
     * background renderer process.
     */
    void bindAsHighPriority(final int pid) {
        ChildProcessConnection connection = getConnectionForPid(pid);
        if (connection == null) return;

        synchronized (mLastOomPidLock) {
            connection.attachAsActive();
            mLastOomPid = pid;
        }
    }

    /**
     * Unbinds a high priority process which was previous bound with bindAsHighPriority.
     */
    void unbindAsHighPriority(final int pid) {
        final ChildProcessConnection connection = getConnectionForPid(pid);
        if (connection == null || !connection.isStrongBindingBound()) return;

        // This runnable performs the actual unbinding. It will be executed synchronously when
        // on low-end devices and posted with a delay otherwise.
        Runnable doUnbind = new Runnable() {
            @Override
            public void run() {
                if (connection.isStrongBindingBound()) {
                    connection.detachAsActive();
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
     * Called when the embedding application is sent to background. We want to maintain a strong
     * binding on the most recently used renderer while the embedder is in background, to indicate
     * the relative importance of the renderer to system oom killer.
     *
     * The embedder needs to ensure that:
     *  - every onBroughtToForeground() is followed by onSentToBackground()
     *  - pairs of consecutive onBroughtToForeground() / onSentToBackground() calls do not overlap
     */
    void onSentToBackground() {
        assert mBoundForBackgroundPeriodPid == -1;
        synchronized (mLastOomPidLock) {
            // mLastOomPid can be -1 at this point as the embedding application could be used in
            // foreground without spawning any renderers.
            if (mLastOomPid >= 0) {
                bindAsHighPriority(mLastOomPid);
                mBoundForBackgroundPeriodPid = mLastOomPid;
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
        if (mBoundForBackgroundPeriodPid >= 0) {
            unbindAsHighPriority(mBoundForBackgroundPeriodPid);
            mBoundForBackgroundPeriodPid = -1;
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
}
