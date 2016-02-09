// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.content.app.ChildProcessService;
import org.chromium.content.app.ChromiumLinkerParams;
import org.chromium.content.common.IChildProcessCallback;
import org.chromium.content.common.IChildProcessService;

import java.io.IOException;

/**
 * Manages a connection between the browser activity and a child service.
 */
public class ChildProcessConnectionImpl implements ChildProcessConnection {
    private final Context mContext;
    private final int mServiceNumber;
    private final boolean mInSandbox;
    private final ChildProcessConnection.DeathCallback mDeathCallback;
    private final Class<? extends ChildProcessService> mServiceClass;

    // Synchronization: While most internal flow occurs on the UI thread, the public API
    // (specifically start and stop) may be called from any thread, hence all entry point methods
    // into the class are synchronized on the lock to protect access to these members.
    private final Object mLock = new Object();
    private IChildProcessService mService = null;
    // Set to true when the service connected successfully.
    private boolean mServiceConnectComplete = false;
    // Set to true when the service disconnects, as opposed to being properly closed. This happens
    // when the process crashes or gets killed by the system out-of-memory killer.
    private boolean mServiceDisconnected = false;
    // When the service disconnects (i.e. mServiceDisconnected is set to true), the status of the
    // oom bindings is stashed here for future inspection.
    private boolean mWasOomProtected = false;
    private int mPid = 0;  // Process ID of the corresponding child process.
    // Initial binding protects the newly spawned process from being killed before it is put to use,
    // it is maintained between calls to start() and removeInitialBinding().
    private ChildServiceConnection mInitialBinding = null;
    // Strong binding will make the service priority equal to the priority of the activity. We want
    // the OS to be able to kill background renderers as it kills other background apps, so strong
    // bindings are maintained only for services that are active at the moment (between
    // addStrongBinding() and removeStrongBinding()).
    private ChildServiceConnection mStrongBinding = null;
    // Low priority binding maintained in the entire lifetime of the connection, i.e. between calls
    // to start() and stop().
    private ChildServiceConnection mWaivedBinding = null;
    // Incremented on addStrongBinding(), decremented on removeStrongBinding().
    private int mStrongBindingCount = 0;
    // Moderate binding will make the service priority equal to the priority of a visible process
    // while the app is in the foreground. It will stay bound only while the app is in the
    // foreground to protect a background process from the system out-of-memory killer.
    private ChildServiceConnection mModerateBinding = null;

    // Linker-related parameters.
    private ChromiumLinkerParams mLinkerParams = null;

    private final boolean mAlwaysInForeground;

    private static final String TAG = "cr.ChildProcessConnect";

    private static class ConnectionParams {
        final String[] mCommandLine;
        final FileDescriptorInfo[] mFilesToBeMapped;
        final IChildProcessCallback mCallback;
        final Bundle mSharedRelros;

        ConnectionParams(String[] commandLine, FileDescriptorInfo[] filesToBeMapped,
                IChildProcessCallback callback, Bundle sharedRelros) {
            mCommandLine = commandLine;
            mFilesToBeMapped = filesToBeMapped;
            mCallback = callback;
            mSharedRelros = sharedRelros;
        }
    }

    // This is set in setupConnection() and is later used in doConnectionSetupLocked(), after which
    // the variable is cleared. Therefore this is only valid while the connection is being set up.
    private ConnectionParams mConnectionParams;

    // Callback provided in setupConnection() that will communicate the result to the caller. This
    // has to be called exactly once after setupConnection(), even if setup fails, so that the
    // caller can free up resources associated with the setup attempt. This is set to null after the
    // call.
    private ChildProcessConnection.ConnectionCallback mConnectionCallback;

    private class ChildServiceConnection implements ServiceConnection {
        private boolean mBound = false;

        private final int mBindFlags;
        private final ChildProcessLauncher.ChildProcessCreationParams mCreationParams;

        private Intent createServiceBindIntent() {
            final String packageName = mCreationParams != null
                    ? mCreationParams.getPackageName() : mContext.getPackageName();
            Intent intent = new Intent();
            intent.setComponent(
                    new ComponentName(packageName, mServiceClass.getName() + mServiceNumber));
            return intent;
        }

        public ChildServiceConnection(int bindFlags,
                ChildProcessLauncher.ChildProcessCreationParams creationParams) {
            if (creationParams != null) {
                bindFlags = creationParams.addExtraBindFlags(bindFlags);
            }
            mBindFlags = bindFlags;
            mCreationParams = creationParams;
        }

        boolean bind(String[] commandLine) {
            if (!mBound) {
                try {
                    TraceEvent.begin("ChildProcessConnectionImpl.ChildServiceConnection.bind");
                    final Intent intent = createServiceBindIntent();
                    if (commandLine != null) {
                        intent.putExtra(ChildProcessConstants.EXTRA_COMMAND_LINE, commandLine);
                    }
                    if (mLinkerParams != null) {
                        mLinkerParams.addIntentExtras(intent);
                    }
                    mBound = mContext.bindService(intent, this, mBindFlags);
                } finally {
                    TraceEvent.end("ChildProcessConnectionImpl.ChildServiceConnection.bind");
                }
            }
            return mBound;
        }

        void unbind() {
            if (mBound) {
                mContext.unbindService(this);
                mBound = false;
            }
        }

        boolean isBound() {
            return mBound;
        }

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            synchronized (mLock) {
                // A flag from the parent class ensures we run the post-connection logic only once
                // (instead of once per each ChildServiceConnection).
                if (mServiceConnectComplete) {
                    return;
                }
                try {
                    TraceEvent.begin(
                            "ChildProcessConnectionImpl.ChildServiceConnection.onServiceConnected");
                    mServiceConnectComplete = true;
                    mService = IChildProcessService.Stub.asInterface(service);
                    // Run the setup if the connection parameters have already been provided. If
                    // not, doConnectionSetupLocked() will be called from setupConnection().
                    if (mConnectionParams != null) {
                        doConnectionSetupLocked();
                    }
                } finally {
                    TraceEvent.end(
                            "ChildProcessConnectionImpl.ChildServiceConnection.onServiceConnected");
                }
            }
        }


        // Called on the main thread to notify that the child service did not disconnect gracefully.
        @Override
        public void onServiceDisconnected(ComponentName className) {
            synchronized (mLock) {
                // Ensure that the disconnection logic runs only once (instead of once per each
                // ChildServiceConnection).
                if (mServiceDisconnected) {
                    return;
                }
                // Stash the status of the oom bindings, since stop() will release all bindings.
                mWasOomProtected = isCurrentlyOomProtected();
                mServiceDisconnected = true;
                Log.w(TAG, "onServiceDisconnected (crash or killed by oom): pid=%d", mPid);
                stop();  // We don't want to auto-restart on crash. Let the browser do that.
                mDeathCallback.onChildProcessDied(ChildProcessConnectionImpl.this);
                // If we have a pending connection callback, we need to communicate the failure to
                // the caller.
                if (mConnectionCallback != null) {
                    mConnectionCallback.onConnected(0);
                }
                mConnectionCallback = null;
            }
        }
    }

    ChildProcessConnectionImpl(Context context, int number, boolean inSandbox,
            ChildProcessConnection.DeathCallback deathCallback,
            Class<? extends ChildProcessService> serviceClass,
            ChromiumLinkerParams chromiumLinkerParams,
            boolean alwaysInForeground,
            ChildProcessLauncher.ChildProcessCreationParams creationParams) {
        mContext = context;
        mServiceNumber = number;
        mInSandbox = inSandbox;
        mDeathCallback = deathCallback;
        mServiceClass = serviceClass;
        mLinkerParams = chromiumLinkerParams;
        mAlwaysInForeground = alwaysInForeground;
        int initialFlags = Context.BIND_AUTO_CREATE;
        if (mAlwaysInForeground) initialFlags |= Context.BIND_IMPORTANT;
        mInitialBinding = new ChildServiceConnection(initialFlags, creationParams);
        mStrongBinding = new ChildServiceConnection(
                Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT, creationParams);
        mWaivedBinding = new ChildServiceConnection(
                Context.BIND_AUTO_CREATE | Context.BIND_WAIVE_PRIORITY, creationParams);
        mModerateBinding = new ChildServiceConnection(Context.BIND_AUTO_CREATE, creationParams);
    }

    @Override
    public int getServiceNumber() {
        return mServiceNumber;
    }

    @Override
    public boolean isInSandbox() {
        return mInSandbox;
    }

    @Override
    public IChildProcessService getService() {
        synchronized (mLock) {
            return mService;
        }
    }

    @Override
    public int getPid() {
        synchronized (mLock) {
            return mPid;
        }
    }

    @Override
    public void start(String[] commandLine) {
        try {
            TraceEvent.begin("ChildProcessConnectionImpl.start");
            synchronized (mLock) {
                assert !ThreadUtils.runningOnUiThread();
                assert mConnectionParams == null :
                        "setupConnection() called before start() in ChildProcessConnectionImpl.";

                if (!mInitialBinding.bind(commandLine)) {
                    Log.e(TAG, "Failed to establish the service connection.");
                    // We have to notify the caller so that they can free-up associated resources.
                    // TODO(ppi): Can we hard-fail here?
                    mDeathCallback.onChildProcessDied(ChildProcessConnectionImpl.this);
                } else {
                    mWaivedBinding.bind(null);
                }
            }
        } finally {
            TraceEvent.end("ChildProcessConnectionImpl.start");
        }
    }

    @Override
    public void setupConnection(
            String[] commandLine,
            FileDescriptorInfo[] filesToBeMapped,
            IChildProcessCallback processCallback,
            ConnectionCallback connectionCallback,
            Bundle sharedRelros) {
        synchronized (mLock) {
            assert mConnectionParams == null;
            if (mServiceDisconnected) {
                Log.w(TAG, "Tried to setup a connection that already disconnected.");
                connectionCallback.onConnected(0);
                return;
            }
            try {
                TraceEvent.begin("ChildProcessConnectionImpl.setupConnection");
                mConnectionCallback = connectionCallback;
                mConnectionParams = new ConnectionParams(
                        commandLine, filesToBeMapped, processCallback, sharedRelros);
                // Run the setup if the service is already connected. If not,
                // doConnectionSetupLocked() will be called from onServiceConnected().
                if (mServiceConnectComplete) {
                    doConnectionSetupLocked();
                }
            } finally {
                TraceEvent.end("ChildProcessConnectionImpl.setupConnection");
            }
        }
    }

    @Override
    public void stop() {
        synchronized (mLock) {
            mInitialBinding.unbind();
            mStrongBinding.unbind();
            mWaivedBinding.unbind();
            mModerateBinding.unbind();
            mStrongBindingCount = 0;
            if (mService != null) {
                mService = null;
            }
            mConnectionParams = null;
        }
    }

    /**
     * Called after the connection parameters have been set (in setupConnection()) *and* a
     * connection has been established (as signaled by onServiceConnected()). These two events can
     * happen in any order. Has to be called with mLock.
     */
    private void doConnectionSetupLocked() {
        try {
            TraceEvent.begin("ChildProcessConnectionImpl.doConnectionSetupLocked");
            assert mServiceConnectComplete && mService != null;
            assert mConnectionParams != null;

            Bundle bundle =
                    ChildProcessLauncher.createsServiceBundle(mConnectionParams.mCommandLine,
                            mConnectionParams.mFilesToBeMapped, mConnectionParams.mSharedRelros);
            try {
                mPid = mService.setupConnection(bundle, mConnectionParams.mCallback);
                assert mPid != 0 : "Child service claims to be run by a process of pid=0.";
            } catch (android.os.RemoteException re) {
                Log.e(TAG, "Failed to setup connection.", re);
            }
            // We proactively close the FDs rather than wait for GC & finalizer.
            try {
                for (FileDescriptorInfo fileInfo : mConnectionParams.mFilesToBeMapped) {
                    fileInfo.mFd.close();
                }
            } catch (IOException ioe) {
                Log.w(TAG, "Failed to close FD.", ioe);
            }
            mConnectionParams = null;

            if (mConnectionCallback != null) {
                mConnectionCallback.onConnected(mPid);
            }
            mConnectionCallback = null;
        } finally {
            TraceEvent.end("ChildProcessConnectionImpl.doConnectionSetupLocked");
        }
    }

    @Override
    public boolean isInitialBindingBound() {
        synchronized (mLock) {
            return mInitialBinding.isBound();
        }
    }

    @Override
    public boolean isStrongBindingBound() {
        synchronized (mLock) {
            return mStrongBinding.isBound();
        }
    }

    @Override
    public void removeInitialBinding() {
        synchronized (mLock) {
            assert !mAlwaysInForeground;
            mInitialBinding.unbind();
        }
    }

    @Override
    public boolean isOomProtectedOrWasWhenDied() {
        synchronized (mLock) {
            if (mServiceDisconnected) {
                return mWasOomProtected;
            } else {
                return isCurrentlyOomProtected();
            }
        }
    }

    private boolean isCurrentlyOomProtected() {
        synchronized (mLock) {
            assert !mServiceDisconnected;
            if (mAlwaysInForeground) return ChildProcessLauncher.isApplicationInForeground();
            return mInitialBinding.isBound() || mStrongBinding.isBound();
        }
    }

    @Override
    public void dropOomBindings() {
        synchronized (mLock) {
            assert !mAlwaysInForeground;
            mInitialBinding.unbind();

            mStrongBindingCount = 0;
            mStrongBinding.unbind();

            mModerateBinding.unbind();
        }
    }

    @Override
    public void addStrongBinding() {
        synchronized (mLock) {
            if (mService == null) {
                Log.w(TAG, "The connection is not bound for %d", mPid);
                return;
            }
            if (mStrongBindingCount == 0) {
                mStrongBinding.bind(null);
            }
            mStrongBindingCount++;
        }
    }

    @Override
    public void removeStrongBinding() {
        synchronized (mLock) {
            if (mService == null) {
                Log.w(TAG, "The connection is not bound for %d", mPid);
                return;
            }
            assert mStrongBindingCount > 0;
            mStrongBindingCount--;
            if (mStrongBindingCount == 0) {
                mStrongBinding.unbind();
            }
        }
    }

    @Override
    public boolean isModerateBindingBound() {
        synchronized (mLock) {
            return mModerateBinding.isBound();
        }
    }

    @Override
    public void addModerateBinding() {
        synchronized (mLock) {
            if (mService == null) {
                Log.w(TAG, "The connection is not bound for %d", mPid);
                return;
            }
            mModerateBinding.bind(null);
        }
    }

    @Override
    public void removeModerateBinding() {
        synchronized (mLock) {
            if (mService == null) {
                Log.w(TAG, "The connection is not bound for %d", mPid);
                return;
            }
            mModerateBinding.unbind();
        }
    }

    @VisibleForTesting
    public boolean crashServiceForTesting() throws RemoteException {
        try {
            mService.crashIntentionallyForTesting();
        } catch (DeadObjectException e) {
            return true;
        }
        return false;
    }

    @VisibleForTesting
    public boolean isConnected() {
        return mService != null;
    }
}
