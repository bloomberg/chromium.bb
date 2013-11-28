// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.SparseArray;
import android.util.SparseIntArray;

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

    // Map from pid to the connection to service hosting particular process. Each reference is held
    // between calls to addNewConnection() and clearConnection().
    private final SparseArray<ChildProcessConnection> mManagedConnections
            = new SparseArray<ChildProcessConnection>();

    // Map from pid to the count of oom bindings bound for the service. Should be accessed with
    // mCountLock.
    // TODO(ppi): The reason we keep track of this count here is to inspect it after
    //            ChildProcessConnectionImpl goes away. We probably could just keep a boolean
    //            instead and populate it when ChildProcessConnectionImpl goes away (in
    //            clearConnection()). In this way we would eliminate some redundancy, as
    //            ChildProcessConnection already keeps track of the bindings. We would probably also
    //            be able to drop mCountLock along the way.
    private final SparseIntArray mOomBindingCount = new SparseIntArray();

    // Pid of the renderer that was most recently oom bound. This is used on low-memory devices
    // to drop oom bindings of a process when another one acquires them, making sure that only
    // one renderer process at a time is oom bound. Should be accessed with mCountLock.
    private int mLastOomPid = -1;

    // Pid of the renderer that we bound with a strong binding for the background period. Equals
    // -1 when the embedder is in foreground.
    private int mBoundForBackgroundPeriodPid = -1;

    // Should be acquired before binding or unbinding the connections and modifying state
    // variables: mOomBindingCount and mLastOomPid.
    private final Object mCountLock = new Object();

    /**
     * Registers an oom binding bound for a child process. Should be called with mCountLock.
     * @param pid handle of the process.
     */
    private void incrementOomCount(int pid) {
        mOomBindingCount.put(pid, mOomBindingCount.get(pid) + 1);
        mLastOomPid = pid;
    }

    /**
     * Registers an oom binding unbound for a child process. Should be called with mCountLock.
     * @param pid handle of the process.
     */
    private void decrementOomCount(int pid) {
        int count = mOomBindingCount.get(pid, -1);
        assert count > 0;
        count--;
        if (count > 0) {
            mOomBindingCount.put(pid, count);
        } else {
            mOomBindingCount.delete(pid);
        }
    }

    /**
     * Drops all oom bindings for the given renderer.
     * @param pid handle of the process.
     */
    private void dropOomBindings(int pid) {
        ChildProcessConnection connection = mManagedConnections.get(pid);
        if (connection == null) {
            ChildProcessLauncher.logPidWarning(pid,
                    "Tried to drop oom bindings for a non-existent connection");
            return;
        }
        synchronized (mCountLock) {
            connection.dropOomBindings();
            mOomBindingCount.delete(pid);
        }
    }

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
     * loading of tabs opened in background).
     * @param pid handle of the process.
     */
    void addNewConnection(int pid, ChildProcessConnection connection) {
        synchronized (mCountLock) {
            if (mIsLowMemoryDevice && mLastOomPid >= 0) {
                dropOomBindings(mLastOomPid);
            }
            // This will reset the previous entries for the pid in the unlikely event of the OS
            // reusing renderer pids.
            mManagedConnections.put(pid, connection);
            mOomBindingCount.put(pid, 0);
            // Every new connection is bound with initial oom binding.
            incrementOomCount(pid);
        }
    }

    /**
     * Remove the initial binding of the child process. Child processes are bound with initial
     * binding to protect them from getting killed before they are put to use. This method
     * allows to remove the binding once it is no longer needed. The binding is removed after a
     * fixed delay period so that the renderer will not be killed immediately after the call.
     */
    void removeInitialBinding(final int pid) {
        final ChildProcessConnection connection = mManagedConnections.get(pid);
        if (connection == null) {
            ChildProcessLauncher.logPidWarning(pid,
                    "Tried to remove a binding for a non-existent connection");
            return;
        }
        if (!connection.isInitialBindingBound()) return;
        ThreadUtils.postOnUiThreadDelayed(new Runnable() {
            @Override
            public void run() {
                synchronized (mCountLock) {
                    if (connection.isInitialBindingBound()) {
                        decrementOomCount(pid);
                        connection.removeInitialBinding();
                    }
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
        ChildProcessConnection connection = mManagedConnections.get(pid);
        if (connection == null) {
            ChildProcessLauncher.logPidWarning(pid, "Tried to bind a non-existent connection");
            return;
        }
        synchronized (mCountLock) {
            connection.attachAsActive();
            incrementOomCount(pid);
        }
    }

    /**
     * Unbinds a high priority process which was previous bound with bindAsHighPriority.
     */
    void unbindAsHighPriority(final int pid) {
        final ChildProcessConnection connection = mManagedConnections.get(pid);
        if (connection == null) {
            ChildProcessLauncher.logPidWarning(pid, "Tried to unbind non-existent connection");
            return;
        }
        if (!connection.isStrongBindingBound()) return;

        // This runnable performs the actual unbinding. It will be executed synchronously when
        // on low-end devices and posted with a delay otherwise.
        Runnable doUnbind = new Runnable() {
            @Override
            public void run() {
                synchronized (mCountLock) {
                    if (connection.isStrongBindingBound()) {
                        decrementOomCount(pid);
                        connection.detachAsActive();
                    }
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
     * @return True iff the given service process is protected from the out-of-memory killing,
     * or it was protected when it died (either crashed or was closed). This can be used to
     * decide if a disconnection of a renderer was a crash or a probable out-of-memory kill. In
     * the unlikely event of the OS reusing renderer pid, the call will refer to the most recent
     * renderer of the given pid. The binding count is being reset in addNewConnection().
     */
    boolean isOomProtected(int pid) {
        synchronized (mCountLock) {
            return mOomBindingCount.get(pid) > 0;
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
        // mLastOomPid can be -1 at this point as the embedding application could be used in
        // foreground without spawning any renderers.
        if (mLastOomPid >= 0) {
            bindAsHighPriority(mLastOomPid);
            mBoundForBackgroundPeriodPid = mLastOomPid;
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
     * Should be called when the connection to the child process goes away (either after a clean
     * exit or an unexpected crash). At this point we let go of the reference to the
     * ChildProcessConnection.
     */
    void clearConnection(int pid) {
        mManagedConnections.remove(pid);
    }
}
