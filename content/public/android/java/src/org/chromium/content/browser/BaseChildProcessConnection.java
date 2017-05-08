// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.process_launcher.ChildProcessCreationParams;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.base.process_launcher.ICallbackInt;
import org.chromium.base.process_launcher.IChildProcessService;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import javax.annotation.Nullable;

/**
 * Manages a connection between the browser activity and a child service.
 */
public abstract class BaseChildProcessConnection {
    private static final String TAG = "BaseChildProcessConn";

    /**
     * Used to notify the consumer about disconnection of the service. This callback is provided
     * earlier than ConnectionCallbacks below, as a child process might die before the connection is
     * fully set up.
     */
    interface DeathCallback {
        // Called on Launcher thread.
        void onChildProcessDied(BaseChildProcessConnection connection);
    }

    /**
     * Used to notify the consumer about the process start. These callbacks will be invoked before
     * the ConnectionCallbacks.
     */
    interface StartCallback {
        /**
         * Called when the child process has successfully started and is ready for connection
         * setup.
         */
        void onChildStarted();

        /**
         * Called when the child process failed to start. This can happen if the process is already
         * in use by another client.
         */
        void onChildStartFailed();
    }

    /**
     * Used to notify the consumer about the connection being established.
     */
    interface ConnectionCallback {
        /**
         * Called when the connection to the service is established.
         * @param connecion the connection object to the child process
         */
        void onConnected(BaseChildProcessConnection connection);
    }

    /** Used to create specialization connection instances. */
    interface Factory {
        BaseChildProcessConnection create(Context context, DeathCallback deathCallback,
                String serviceClassName, Bundle childProcessCommonParameters,
                ChildProcessCreationParams creationParams);
    }

    /** Interface representing a connection to the Android service. Can be mocked in unit-tests. */
    protected interface ChildServiceConnection {
        boolean bind();
        void unbind();
        boolean isBound();
    }

    /** Implementation of ChildServiceConnection that does connect to a service. */
    protected class ChildServiceConnectionImpl
            implements ChildServiceConnection, ServiceConnection {
        private final int mBindFlags;
        private boolean mBound;

        private Intent createServiceBindIntent() {
            Intent intent = new Intent();
            if (mCreationParams != null) {
                mCreationParams.addIntentExtras(intent);
            }
            intent.setComponent(mServiceName);
            return intent;
        }

        private ChildServiceConnectionImpl(int bindFlags) {
            mBindFlags = bindFlags;
        }

        @Override
        public boolean bind() {
            if (!mBound) {
                try {
                    TraceEvent.begin("BaseChildProcessConnection.ChildServiceConnection.bind");
                    Intent intent = createServiceBindIntent();
                    if (mChildProcessCommonParameters != null) {
                        intent.putExtras(mChildProcessCommonParameters);
                    }
                    mBound = mContext.bindService(intent, this, mBindFlags);
                } finally {
                    TraceEvent.end("BaseChildProcessConnection.ChildServiceConnection.bind");
                }
            }
            return mBound;
        }

        @Override
        public void unbind() {
            if (mBound) {
                mContext.unbindService(this);
                mBound = false;
            }
        }

        @Override
        public boolean isBound() {
            return mBound;
        }

        @Override
        public void onServiceConnected(ComponentName className, final IBinder service) {
            LauncherThread.post(new Runnable() {
                @Override
                public void run() {
                    BaseChildProcessConnection.this.onServiceConnectedOnLauncherThread(service);
                }
            });
        }

        // Called on the main thread to notify that the child service did not disconnect gracefully.
        @Override
        public void onServiceDisconnected(ComponentName className) {
            LauncherThread.post(new Runnable() {
                @Override
                public void run() {
                    BaseChildProcessConnection.this.onServiceDisconnectedOnLauncherThread();
                }
            });
        }
    }

    // binding flag provided via ChildProcessCreationParams.
    // TODO(mnaganov): Get rid of it after the release of the next Android SDK.
    private static final Map<ComponentName, Boolean> sNeedsExtrabindFlagsMap = new HashMap<>();

    private final Context mContext;
    private final BaseChildProcessConnection.DeathCallback mDeathCallback;
    private final ComponentName mServiceName;

    // Parameters passed to the child process through the service binding intent.
    // If the service gets recreated by the framework the intent will be reused, so these parameters
    // should be common to all processes of that type.
    private final Bundle mChildProcessCommonParameters;

    private final ChildProcessCreationParams mCreationParams;

    private static class ConnectionParams {
        final String[] mCommandLine;
        final FileDescriptorInfo[] mFilesToBeMapped;
        final IBinder mCallback;

        ConnectionParams(
                String[] commandLine, FileDescriptorInfo[] filesToBeMapped, IBinder callback) {
            mCommandLine = commandLine;
            mFilesToBeMapped = filesToBeMapped;
            mCallback = callback;
        }
    }

    // This is set in start() and is used in onServiceConnected().
    private StartCallback mStartCallback;

    // This is set in setupConnection() and is later used in doConnectionSetupLocked(), after which
    // the variable is cleared. Therefore this is only valid while the connection is being set up.
    private ConnectionParams mConnectionParams;

    // Callback provided in setupConnection() that will communicate the result to the caller. This
    // has to be called exactly once after setupConnection(), even if setup fails, so that the
    // caller can free up resources associated with the setup attempt. This is set to null after the
    // call.
    private ConnectionCallback mConnectionCallback;

    private IChildProcessService mService;

    // Set to true when the service connection callback runs. This differs from
    // mServiceConnectComplete, which tracks that the connection completed successfully.
    private boolean mDidOnServiceConnected;

    // Set to true when the service connected successfully.
    private boolean mServiceConnectComplete;

    // Set to true when the service disconnects, as opposed to being properly closed. This happens
    // when the process crashes or gets killed by the system out-of-memory killer.
    private boolean mServiceDisconnected;

    // Process ID of the corresponding child process.
    private int mPid;

    protected BaseChildProcessConnection(Context context, DeathCallback deathCallback,
            String serviceClassName, Bundle childProcessCommonParameters,
            ChildProcessCreationParams creationParams) {
        assert LauncherThread.runningOnLauncherThread();
        mContext = context;
        mDeathCallback = deathCallback;
        String packageName =
                creationParams != null ? creationParams.getPackageName() : context.getPackageName();
        mServiceName = new ComponentName(packageName, serviceClassName);
        mChildProcessCommonParameters = childProcessCommonParameters;
        mCreationParams = creationParams;
    }

    public final Context getContext() {
        assert LauncherThread.runningOnLauncherThread();
        return mContext;
    }

    public final String getPackageName() {
        assert LauncherThread.runningOnLauncherThread();
        return mCreationParams != null ? mCreationParams.getPackageName()
                                       : mContext.getPackageName();
    }

    public final ChildProcessCreationParams getCreationParams() {
        assert LauncherThread.runningOnLauncherThread();
        return mCreationParams;
    }

    public final IChildProcessService getService() {
        assert LauncherThread.runningOnLauncherThread();
        return mService;
    }

    public final ComponentName getServiceName() {
        assert LauncherThread.runningOnLauncherThread();
        return mServiceName;
    }

    /**
     * @return the connection pid, or 0 if not yet connected
     */
    public int getPid() {
        assert LauncherThread.runningOnLauncherThread();
        return mPid;
    }

    /**
     * Starts a connection to an IChildProcessService. This must be followed by a call to
     * setupConnection() to setup the connection parameters. start() and setupConnection() are
     * separate to allow to pass whatever parameters are available in start(), and complete the
     * remainder later while reducing the connection setup latency.
     * @param startCallback (optional) callback when the child process starts or fails to start.
     */
    public void start(StartCallback startCallback) {
        assert LauncherThread.runningOnLauncherThread();
        try {
            TraceEvent.begin("BaseChildProcessConnection.start");
            assert LauncherThread.runningOnLauncherThread();
            assert mConnectionParams
                    == null
                    : "setupConnection() called before start() in BaseChildProcessConnection.";

            mStartCallback = startCallback;

            if (!bind()) {
                Log.e(TAG, "Failed to establish the service connection.");
                // We have to notify the caller so that they can free-up associated resources.
                // TODO(ppi): Can we hard-fail here?
                mDeathCallback.onChildProcessDied(BaseChildProcessConnection.this);
            }
        } finally {
            TraceEvent.end("BaseChildProcessConnection.start");
        }
    }

    /**
     * Setups the connection after it was started with start().
     * @param commandLine (optional) will be ignored if the command line was already sent in start()
     * @param filesToBeMapped a list of file descriptors that should be registered
     * @param callback optional client specified callbacks that the child can use to communicate
     *                 with the parent process
     * @param connectionCallback will be called exactly once after the connection is set up or the
     *                           setup fails
     */
    public void setupConnection(String[] commandLine, FileDescriptorInfo[] filesToBeMapped,
            @Nullable IBinder callback, ConnectionCallback connectionCallback) {
        assert LauncherThread.runningOnLauncherThread();
        assert mConnectionParams == null;
        if (mServiceDisconnected) {
            Log.w(TAG, "Tried to setup a connection that already disconnected.");
            connectionCallback.onConnected(null);
            return;
        }
        try {
            TraceEvent.begin("BaseChildProcessConnection.setupConnection");
            mConnectionCallback = connectionCallback;
            mConnectionParams = new ConnectionParams(commandLine, filesToBeMapped, callback);
            // Run the setup if the service is already connected. If not,
            // doConnectionSetupLocked() will be called from onServiceConnected().
            if (mServiceConnectComplete) {
                doConnectionSetupLocked();
            }
        } finally {
            TraceEvent.end("BaseChildProcessConnection.setupConnection");
        }
    }

    /**
     * Terminates the connection to IChildProcessService, closing all bindings. It is safe to call
     * this multiple times.
     */
    public void stop() {
        assert LauncherThread.runningOnLauncherThread();
        unbind();
        mService = null;
        mConnectionParams = null;
    }

    private void onServiceConnectedOnLauncherThread(IBinder service) {
        assert LauncherThread.runningOnLauncherThread();
        // A flag from the parent class ensures we run the post-connection logic only once
        // (instead of once per each ChildServiceConnection).
        if (mDidOnServiceConnected) {
            return;
        }
        try {
            TraceEvent.begin(
                    "BaseChildProcessConnection.ChildServiceConnection.onServiceConnected");
            mDidOnServiceConnected = true;
            mService = IChildProcessService.Stub.asInterface(service);

            StartCallback startCallback = mStartCallback;
            mStartCallback = null;

            final boolean bindCheck =
                    mCreationParams != null && mCreationParams.getBindToCallerCheck();
            boolean boundToUs = false;
            try {
                boundToUs = bindCheck ? mService.bindToCaller() : true;
            } catch (RemoteException ex) {
                // Do not trigger the StartCallback here, since the service is already
                // dead and the DeathCallback will run from onServiceDisconnected().
                Log.e(TAG, "Failed to bind service to connection.", ex);
                return;
            }

            if (startCallback != null) {
                if (boundToUs) {
                    startCallback.onChildStarted();
                } else {
                    startCallback.onChildStartFailed();
                }
            }

            if (!boundToUs) {
                return;
            }

            mServiceConnectComplete = true;

            // Run the setup if the connection parameters have already been provided. If
            // not, doConnectionSetupLocked() will be called from setupConnection().
            if (mConnectionParams != null) {
                doConnectionSetupLocked();
            }
        } finally {
            TraceEvent.end("BaseChildProcessConnection.ChildServiceConnection.onServiceConnected");
        }
    }

    private void onServiceDisconnectedOnLauncherThread() {
        assert LauncherThread.runningOnLauncherThread();
        // Ensure that the disconnection logic runs only once (instead of once per each
        // ChildServiceConnection).
        if (mServiceDisconnected) {
            return;
        }
        mServiceDisconnected = true;
        Log.w(TAG, "onServiceDisconnected (crash or killed by oom): pid=%d", mPid);
        stop(); // We don't want to auto-restart on crash. Let the browser do that.
        mDeathCallback.onChildProcessDied(BaseChildProcessConnection.this);
        // If we have a pending connection callback, we need to communicate the failure to
        // the caller.
        if (mConnectionCallback != null) {
            mConnectionCallback.onConnected(null);
        }
        mConnectionCallback = null;
    }

    private void onSetupConnectionResult(int pid) {
        mPid = pid;
        assert mPid != 0 : "Child service claims to be run by a process of pid=0.";

        if (mConnectionCallback != null) {
            mConnectionCallback.onConnected(this);
        }
        mConnectionCallback = null;
    }

    /**
     * Called after the connection parameters have been set (in setupConnection()) *and* a
     * connection has been established (as signaled by onServiceConnected()). These two events can
     * happen in any order. Has to be called with mLock.
     */
    private void doConnectionSetupLocked() {
        try {
            TraceEvent.begin("BaseChildProcessConnection.doConnectionSetupLocked");
            assert mServiceConnectComplete && mService != null;
            assert mConnectionParams != null;

            Bundle bundle = ChildProcessLauncher.createsServiceBundle(
                    mConnectionParams.mCommandLine, mConnectionParams.mFilesToBeMapped);
            ICallbackInt pidCallback = new ICallbackInt.Stub() {
                @Override
                public void call(final int pid) {
                    LauncherThread.post(new Runnable() {
                        @Override
                        public void run() {
                            onSetupConnectionResult(pid);
                        }
                    });
                }
            };
            try {
                mService.setupConnection(bundle, pidCallback, mConnectionParams.mCallback);
            } catch (RemoteException re) {
                Log.e(TAG, "Failed to setup connection.", re);
            }
            // We proactively close the FDs rather than wait for GC & finalizer.
            try {
                for (FileDescriptorInfo fileInfo : mConnectionParams.mFilesToBeMapped) {
                    fileInfo.fd.close();
                }
            } catch (IOException ioe) {
                Log.w(TAG, "Failed to close FD.", ioe);
            }
            mConnectionParams = null;
        } finally {
            TraceEvent.end("BaseChildProcessConnection.doConnectionSetupLocked");
        }
    }

    /** Subclasses should implement this method to bind/unbind to the actual service. */
    protected abstract boolean bind();
    protected abstract void unbind();

    protected ChildServiceConnection createServiceConnection(int bindFlags) {
        assert LauncherThread.runningOnLauncherThread();
        return new ChildServiceConnectionImpl(bindFlags);
    }

    protected boolean shouldBindAsExportedService() {
        assert LauncherThread.runningOnLauncherThread();
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && getCreationParams() != null
                && getCreationParams().getIsExternalService()
                && isExportedService(getContext(), getServiceName());
    }

    private static boolean isExportedService(Context context, ComponentName serviceName) {
        Boolean isExported = sNeedsExtrabindFlagsMap.get(serviceName);
        if (isExported != null) {
            return isExported;
        }
        boolean result = false;
        try {
            PackageManager packageManager = context.getPackageManager();
            ServiceInfo serviceInfo = packageManager.getServiceInfo(serviceName, 0);
            result = serviceInfo.exported;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Could not retrieve info about service %s", serviceName, e);
        }
        sNeedsExtrabindFlagsMap.put(serviceName, Boolean.valueOf(result));
        return result;
    }

    @VisibleForTesting
    public void crashServiceForTesting() throws RemoteException {
        mService.crashIntentionallyForTesting();
    }

    @VisibleForTesting
    boolean isConnected() {
        return mService != null;
    }
}
