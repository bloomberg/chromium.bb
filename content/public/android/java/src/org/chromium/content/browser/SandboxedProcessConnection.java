// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;

import org.chromium.base.CalledByNative;
import org.chromium.base.CpuFeatures;
import org.chromium.base.ThreadUtils;
import org.chromium.content.common.CommandLine;
import org.chromium.content.common.ISandboxedProcessCallback;
import org.chromium.content.common.ISandboxedProcessService;
import org.chromium.content.common.TraceEvent;

public class SandboxedProcessConnection implements ServiceConnection {
    interface DeathCallback {
        void onSandboxedProcessDied(int pid);
    }

    // Names of items placed in the bind intent or connection bundle.
    public static final String EXTRA_COMMAND_LINE =
            "com.google.android.apps.chrome.extra.sandbox_command_line";
    public static final String EXTRA_NATIVE_LIBRARY_NAME =
            "com.google.android.apps.chrome.extra.sandbox_native_library_name";
    // Note the FDs may only be passed in the connection bundle.
    public static final String EXTRA_FILES_PREFIX =
            "com.google.android.apps.chrome.extra.sandbox_extraFile_";
    public static final String EXTRA_FILES_ID_SUFFIX = "_id";
    public static final String EXTRA_FILES_FD_SUFFIX = "_fd";

    // Used to pass the CPU core count to sandboxed processes.
    public static final String EXTRA_CPU_COUNT =
            "com.google.android.apps.chrome.extra.cpu_count";
    // Used to pass the CPU features mask to sandboxed processes.
    public static final String EXTRA_CPU_FEATURES =
            "com.google.android.apps.chrome.extra.cpu_features";

    private final Context mContext;
    private final int mServiceNumber;
    private final SandboxedProcessConnection.DeathCallback mDeathCallback;

    // Synchronization: While most internal flow occurs on the UI thread, the public API
    // (specifically bind and unbind) may be called from any thread, hence all entry point methods
    // into the class are synchronized on the SandboxedProcessConnection instance to protect access
    // to these members. But see also the TODO where AsyncBoundServiceConnection is created.
    private ISandboxedProcessService mService = null;
    private boolean mServiceConnectComplete = false;
    private int mPID = 0;  // Process ID of the corresponding sandboxed process.
    private HighPriorityConnection mHighPriorityConnection = null;
    private int mHighPriorityConnectionCount = 0;

    private static final String TAG = "SandboxedProcessConnection";

    private static class ConnectionParams {
        final String[] mCommandLine;
        final FileDescriptorInfo[] mFilesToBeMapped;
        final ISandboxedProcessCallback mCallback;
        final Runnable mOnConnectionCallback;

        ConnectionParams(
                String[] commandLine,
                FileDescriptorInfo[] filesToBeMapped,
                ISandboxedProcessCallback callback,
                Runnable onConnectionCallback) {
            mCommandLine = commandLine;
            mFilesToBeMapped = filesToBeMapped;
            mCallback = callback;
            mOnConnectionCallback = onConnectionCallback;
        }
    }

    // This is only valid while the connection is being established.
    private ConnectionParams mConnectionParams;
    private boolean mIsBound;

    SandboxedProcessConnection(Context context, int number,
            SandboxedProcessConnection.DeathCallback deathCallback) {
        mContext = context;
        mServiceNumber = number;
        mDeathCallback = deathCallback;
    }

    int getServiceNumber() {
        return mServiceNumber;
    }

    synchronized ISandboxedProcessService getService() {
        return mService;
    }

    private Intent createServiceBindIntent() {
        Intent intent = new Intent();
        String n = org.chromium.content.app.SandboxedProcessService.class.getName();
        intent.setClassName(mContext, n + mServiceNumber);
        intent.setPackage(mContext.getPackageName());
        return intent;
    }

    /**
     * Bind to an ISandboxedProcessService. This must be followed by a call to setupConnection()
     * to setup the connection parameters. (These methods are separated to allow the client
     * to pass whatever parameters they have available here, and complete the remainder
     * later while reducing the connection setup latency).
     * @param nativeLibraryName The name of the shared native library to be loaded for the
     *                          sandboxed process.
     * @param commandLine (Optional) Command line for the sandboxed process. If omitted, then
     *                    the command line parameters must instead be passed to setupConnection().
     */
    synchronized void bind(String nativeLibraryName, String[] commandLine) {
        TraceEvent.begin();
        assert !ThreadUtils.runningOnUiThread();

        final Intent intent = createServiceBindIntent();

        intent.putExtra(EXTRA_NATIVE_LIBRARY_NAME, nativeLibraryName);
        if (commandLine != null) {
            intent.putExtra(EXTRA_COMMAND_LINE, commandLine);
        }

        mIsBound = mContext.bindService(intent, this, Context.BIND_AUTO_CREATE);
        if (!mIsBound) {
            onBindFailed();
        }
        TraceEvent.end();
    }

    /** Setup a connection previous bound via a call to bind().
     *
     * This establishes the parameters that were not already supplied in bind.
     * @param commandLine (Optional) will be ignored if the command line was already sent in bind()
     * @param fileToBeMapped a list of file descriptors that should be registered
     * @param callback Used for status updates regarding this process connection.
     * @param onConnectionCallback will be run when the connection is setup and ready to use.
     */
    synchronized void setupConnection(
            String[] commandLine,
            FileDescriptorInfo[] filesToBeMapped,
            ISandboxedProcessCallback callback,
            Runnable onConnectionCallback) {
        TraceEvent.begin();
        assert mConnectionParams == null;
        mConnectionParams = new ConnectionParams(commandLine, filesToBeMapped, callback,
                onConnectionCallback);
        if (mServiceConnectComplete) {
            doConnectionSetup();
        }
        TraceEvent.end();
    }

    /**
     * Unbind the ISandboxedProcessService. It is safe to call this multiple times.
     */
    synchronized void unbind() {
        if (mIsBound) {
            mContext.unbindService(this);
            mIsBound = false;
        }
        if (mService != null) {
            if (mHighPriorityConnection != null) {
                unbindHighPriority(true);
            }
            mService = null;
            mPID = 0;
        }
        mConnectionParams = null;
        mServiceConnectComplete = false;
    }

    // Called on the main thread to notify that the service is connected.
    @Override
    public void onServiceConnected(ComponentName className, IBinder service) {
        TraceEvent.begin();
        mServiceConnectComplete = true;
        mService = ISandboxedProcessService.Stub.asInterface(service);
        if (mConnectionParams != null) {
            doConnectionSetup();
        }
        TraceEvent.end();
    }

    // Called on the main thread to notify that the bindService() call failed (returned false).
    private void onBindFailed() {
        mServiceConnectComplete = true;
        if (mConnectionParams != null) {
            doConnectionSetup();
        }
    }

    /**
     * Called when the connection parameters have been set, and a connection has been established
     * (as signaled by onServiceConnected), or if the connection failed (mService will be false).
     */
    private void doConnectionSetup() {
        TraceEvent.begin();
        assert mServiceConnectComplete && mConnectionParams != null;
        // Capture the callback before it is potentially nulled in unbind().
        Runnable onConnectionCallback =
            mConnectionParams != null ? mConnectionParams.mOnConnectionCallback : null;
        if (onConnectionCallback == null) {
            unbind();
        } else if (mService != null) {
            Bundle bundle = new Bundle();
            bundle.putStringArray(EXTRA_COMMAND_LINE, mConnectionParams.mCommandLine);

            FileDescriptorInfo[] fileInfos = mConnectionParams.mFilesToBeMapped;
            ParcelFileDescriptor[] parcelFiles = new ParcelFileDescriptor[fileInfos.length];
            for (int i = 0; i < fileInfos.length; i++) {
                if (fileInfos[i].mFd == -1) {
                    // If someone provided an invalid FD, they are doing something wrong.
                    Log.e(TAG, "Invalid FD (id=" + fileInfos[i].mId + ") for process connection, "
                          + "aborting connection.");
                    return;
                }
                String idName = EXTRA_FILES_PREFIX + i + EXTRA_FILES_ID_SUFFIX;
                String fdName = EXTRA_FILES_PREFIX + i + EXTRA_FILES_FD_SUFFIX;
                if (fileInfos[i].mAutoClose) {
                    // Adopt the FD, it will be closed when we close the ParcelFileDescriptor.
                    parcelFiles[i] = ParcelFileDescriptor.adoptFd(fileInfos[i].mFd);
                } else {
                    try {
                        parcelFiles[i] = ParcelFileDescriptor.fromFd(fileInfos[i].mFd);
                    } catch(IOException e) {
                        Log.e(TAG,
                              "Invalid FD provided for process connection, aborting connection.",
                              e);
                        return;
                    }

                }
                bundle.putParcelable(fdName, parcelFiles[i]);
                bundle.putInt(idName, fileInfos[i].mId);
            }
            // Add the CPU properties now.
            bundle.putInt(EXTRA_CPU_COUNT, CpuFeatures.getCount());
            bundle.putLong(EXTRA_CPU_FEATURES, CpuFeatures.getMask());

            try {
                mPID = mService.setupConnection(bundle, mConnectionParams.mCallback);
            } catch (android.os.RemoteException re) {
                Log.e(TAG, "Failed to setup connection.", re);
            }
            // We proactivley close the FDs rather than wait for GC & finalizer.
            try {
                for (ParcelFileDescriptor parcelFile : parcelFiles) {
                    if (parcelFile != null) parcelFile.close();
                }
            } catch (IOException ioe) {
                Log.w(TAG, "Failed to close FD.", ioe);
            }
        }
        mConnectionParams = null;
        if (onConnectionCallback != null) {
            onConnectionCallback.run();
        }
        TraceEvent.end();
    }

    // Called on the main thread to notify that the sandboxed service did not disconnect gracefully.
    @Override
    public void onServiceDisconnected(ComponentName className) {
        int pid = mPID;  // Stash pid & connection callback since unbind() will clear them.
        Runnable onConnectionCallback =
            mConnectionParams != null ? mConnectionParams.mOnConnectionCallback : null;
        Log.w(TAG, "onServiceDisconnected (crash?): pid=" + pid);
        unbind();  // We don't want to auto-restart on crash. Let the browser do that.
        if (pid != 0) {
            mDeathCallback.onSandboxedProcessDied(pid);
        }
        if (onConnectionCallback != null) {
            onConnectionCallback.run();
        }
    }

    /**
     * Bind the service with a new high priority connection. This will make the service
     * as important as the main process.
     */
    synchronized void bindHighPriority() {
        if (mService == null) {
            Log.w(TAG, "The connection is not bound for " + mPID);
            return;
        }
        if (mHighPriorityConnection == null) {
            mHighPriorityConnection = new HighPriorityConnection();
            mHighPriorityConnection.bind();
        }
        mHighPriorityConnectionCount++;
    }

    /**
     * Unbind the service as the high priority connection.
     */
    synchronized void unbindHighPriority(boolean force) {
        if (mService == null) {
            Log.w(TAG, "The connection is not bound for " + mPID);
            return;
        }
        mHighPriorityConnectionCount--;
        if (force || (mHighPriorityConnectionCount == 0 && mHighPriorityConnection != null)) {
            mHighPriorityConnection.unbind();
            mHighPriorityConnection = null;
        }
    }

    private class HighPriorityConnection implements ServiceConnection {

        private boolean mHBound = false;

        void bind() {
            final Intent intent = createServiceBindIntent();

            mHBound = mContext.bindService(intent, this,
                    Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT);
        }

        void unbind() {
            if (mHBound) {
                mContext.unbindService(this);
                mHBound = false;
            }
        }

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
        }

        @Override
        public void onServiceDisconnected(ComponentName className) {
        }
    }

    /**
     * @return The connection PID, or 0 if not yet connected.
     */
    synchronized public int getPid() {
        return mPID;
    }
}
