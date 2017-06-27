// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.support.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.process_launcher.ChildProcessCreationParams;

/**
 * This class is used to create a single spare ChildProcessConnection (usually early on during
 * start-up) that can then later be retrieved when a connection to a service is needed.
 */
public class SpareChildConnection {
    private static final String TAG = "SpareChildConn";

    // Factory interface used to create the actual connection.
    interface ConnectionFactory {
        ChildProcessConnection allocateBoundConnection(Context context,
                ChildProcessCreationParams creationParams,
                ChildProcessConnection.ServiceCallback serviceCallback);
    }

    // The parameters passed to the Connectionfactory when creating the connection.
    // Also used to identify the connection when the connection is retrieved.
    private final ChildProcessCreationParams mCreationParams;

    // Whether the connection is sandboxed.
    private final boolean mSandoxed;

    // The actual spare connection.
    private ChildProcessConnection mConnection;

    // True when there is a spare connection and it is bound.
    private boolean mConnectionReady;

    // The callback that should be called when the connection becomes bound. Set when the connection
    // is retrieved.
    private ChildProcessConnection.ServiceCallback mConnectionServiceCallback;

    /** Creates and binds a ChildProcessConnection using the specified parameters. */
    public SpareChildConnection(Context context, ConnectionFactory connectionFactory,
            ChildProcessCreationParams creationParams, boolean sandboxed) {
        assert LauncherThread.runningOnLauncherThread();

        mCreationParams = creationParams;
        mSandoxed = sandboxed;

        ChildProcessConnection.ServiceCallback serviceCallback =
                new ChildProcessConnection.ServiceCallback() {
                    @Override
                    public void onChildStarted() {
                        assert LauncherThread.runningOnLauncherThread();
                        mConnectionReady = true;
                        if (mConnectionServiceCallback != null) {
                            mConnectionServiceCallback.onChildStarted();
                            clearConnection();
                        }
                        // If there is no chained callback, that means the spare connection has not
                        // been used yet. It will be cleared when used.
                    }

                    @Override
                    public void onChildStartFailed() {
                        assert LauncherThread.runningOnLauncherThread();
                        Log.e(TAG, "Failed to warm up the spare sandbox service");
                        if (mConnectionServiceCallback != null) {
                            mConnectionServiceCallback.onChildStartFailed();
                        }
                        clearConnection();
                    }

                    @Override
                    public void onChildProcessDied(ChildProcessConnection connection) {
                        if (mConnectionServiceCallback != null) {
                            mConnectionServiceCallback.onChildProcessDied(connection);
                        }
                        if (mConnection != null) {
                            assert connection == mConnection;
                            clearConnection();
                        }
                    }
                };

        mConnection = connectionFactory.allocateBoundConnection(
                context, mCreationParams, serviceCallback);
    }

    /**
     * @return a connection that has been bound or is being bound matching the given paramters, null
     * otherwise.
     */
    public ChildProcessConnection getConnection(ChildProcessCreationParams creationParams,
            boolean sandboxed,
            @NonNull final ChildProcessConnection.ServiceCallback serviceCallback) {
        assert LauncherThread.runningOnLauncherThread();
        if (mConnection == null || creationParams != mCreationParams || sandboxed != mSandoxed
                || mConnectionServiceCallback != null) {
            return null;
        }

        mConnectionServiceCallback = serviceCallback;

        ChildProcessConnection connection = mConnection;
        if (mConnectionReady) {
            // onChildStarted was already run. Call it explicitly on the passed serviceCallback.
            if (serviceCallback != null) {
                // Post a task so the callback happens after the caller has retrieved the
                // connection.
                LauncherThread.post(new Runnable() {
                    @Override
                    public void run() {
                        serviceCallback.onChildStarted();
                    }
                });
            }
            clearConnection();
        }
        return connection;
    }

    /** Returns true if no connection is available (so getConnection will always return null), */
    public boolean isEmpty() {
        return mConnection == null;
    }

    private void clearConnection() {
        assert LauncherThread.runningOnLauncherThread();
        mConnection = null;
        mConnectionReady = false;
    }

    @VisibleForTesting
    public ChildProcessConnection getConnection() {
        return mConnection;
    }
}
