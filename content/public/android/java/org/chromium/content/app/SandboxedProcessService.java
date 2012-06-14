// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import org.chromium.base.CalledByNative;
import org.chromium.content.app.ContentMain;
import org.chromium.content.browser.SandboxedProcessConnection;
import org.chromium.content.common.ISandboxedProcessCallback;
import org.chromium.content.common.ISandboxedProcessService;
import org.chromium.content.common.SurfaceCallback;

/**
 * This is the base class for sandboxed services; the SandboxedProcessService0, 1.. etc
 * subclasses provide the concrete service entry points, to enable the browser to connect
 * to more than one distinct process (i.e. one process per service number, up to limit of N).
 * The embedding application must declare these service instances in the application section
 * of its AndroidManifest.xml, for example with N entries of the form:-
 *     <service android:name="org.chromium.content.app.SandboxedProcessServiceX"
 *              android:process=":sandboxed_processX" />
 * for X in 0...N-1 (where N is {@link SandboxedProcessLauncher#MAX_REGISTERED_SERVICES})
 */
public class SandboxedProcessService extends Service {
    private static final String MAIN_THREAD_NAME = "SandboxedProcessMain";
    private static final String TAG = "SandboxedProcessService";
    private ISandboxedProcessCallback mCallback;

    // This is the native "Main" thread for the renderer / utility process.
    private Thread mSandboxMainThread;
    // Parameters received via IPC, only accessed while holding the mSandboxMainThread monitor.
    private String[] mCommandLineParams;
    private ParcelFileDescriptor mIPCFd;
    private ParcelFileDescriptor mCrashFd;

    private static Context sContext = null;
    private boolean mLibraryInitialized = false;

    // Binder object used by clients for this service.
    private final ISandboxedProcessService.Stub mBinder = new ISandboxedProcessService.Stub() {
        // NOTE: Implement any ISandboxedProcessService methods here.
        @Override
        public int setupConnection(Bundle args, ISandboxedProcessCallback callback) {
            mCallback = callback;
            synchronized (mSandboxMainThread) {
                // Allow the command line to be set via bind() intent or setupConnection, but
                // the FD can only be transferred here.
                if (mCommandLineParams == null) {
                    mCommandLineParams = args.getStringArray(
                            SandboxedProcessConnection.EXTRA_COMMAND_LINE);
                }
                // We must have received the command line by now
                assert mCommandLineParams != null;
                mIPCFd = args.getParcelable(SandboxedProcessConnection.EXTRA_IPC_FD);
                // mCrashFd may be null if native crash reporting is disabled.
                if (args.containsKey(SandboxedProcessConnection.EXTRA_CRASH_FD)) {
                    mCrashFd = args.getParcelable(SandboxedProcessConnection.EXTRA_CRASH_FD);
                }
                mSandboxMainThread.notifyAll();
            }
            return Process.myPid();
        }

        @Override
        public void setSurface(int type, Surface surface, int primaryID, int secondaryID) {
            // This gives up ownership of the Surface.
            SurfaceCallback.setSurface(type, surface, primaryID, secondaryID);
        }
    };

    /* package */ static Context getContext() {
        return sContext;
    }

    @Override
    public void onCreate() {
        sContext = this;
        super.onCreate();

        mSandboxMainThread = new Thread(new Runnable() {
            @Override
            public void run()  {
                try {
                    // TODO(michaelbai): Upstream LibraryLoader.java
                    // if (!LibraryLoader.loadNow()) return;
                    synchronized (mSandboxMainThread) {
                        while (mCommandLineParams == null) {
                            mSandboxMainThread.wait();
                        }
                    }
                    // LibraryLoader.initializeOnMainThread(mCommandLineParams);
                    synchronized (mSandboxMainThread) {
                        mLibraryInitialized = true;
                        mSandboxMainThread.notifyAll();
                        while (mIPCFd == null) {
                            mSandboxMainThread.wait();
                        }
                    }
                    int crashFd = (mCrashFd == null) ? -1 : mCrashFd.detachFd();
                    ContentMain.initApplicationContext(sContext.getApplicationContext());
                    nativeInitSandboxedProcess(sContext.getApplicationContext(),
                            SandboxedProcessService.this, mIPCFd.detachFd(), crashFd);
                    ContentMain.start();
                    nativeExitSandboxedProcess();
                } catch (InterruptedException e) {
                    Log.w(TAG, MAIN_THREAD_NAME + " startup failed: " + e);
                }
            }
        }, MAIN_THREAD_NAME);
        mSandboxMainThread.start();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mCommandLineParams == null) {
            // This process was destroyed before it even started. Nothing more to do.
            return;
        }
        synchronized (mSandboxMainThread) {
            try {
                while (!mLibraryInitialized) {
                    // Avoid a potential race in calling through to native code before the library
                    // has loaded.
                    mSandboxMainThread.wait();
                }
            } catch (InterruptedException e) {
            }
        }

        // This is not synchronized with the main thread in any way, but this is analogous
        // to how desktop chrome terminates processes using SIGTERM. The mSandboxMainThread
        // may run briefly before this is executed, but will eventually get a channel error
        // and similarly commit suicide via SuicideOnChannelErrorFilter().
        // TODO(tedbo): Why doesn't the activity manager SIGTERM/SIGKILL this service process?
        nativeExitSandboxedProcess();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // We call stopSelf() to request that this service be stopped as soon as the client
        // unbinds. Otherwise the system may keep it around and available for a reconnect. The
        // sandboxed processes do not currently support reconnect; they must be initialized from
        // scratch every time.
        stopSelf();

        synchronized (mSandboxMainThread) {
            mCommandLineParams = intent.getStringArrayExtra(
                    SandboxedProcessConnection.EXTRA_COMMAND_LINE);
            mSandboxMainThread.notifyAll();
        }

        return mBinder;
    }

    /**
     * Called from native code to share a surface texture with another child process.
     * Through using the callback object the browser is used as a proxy to route the
     * call to the correct process.
     *
     * @param pid Process handle of the sandboxed process to share the SurfaceTexture with.
     * @param type The type of process that the SurfaceTexture is for.
     * @param surfaceObject The Surface or SurfaceTexture to share with the other sandboxed process.
     * @param primaryID Used to route the call to the correct client instance.
     * @param secondaryID Used to route the call to the correct client instance.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void establishSurfaceTexturePeer(int pid, int type, Object surfaceObject, int primaryID,
                                             int secondaryID) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }

        Surface surface = null;
        boolean needRelease = false;
        if (surfaceObject instanceof Surface) {
            surface = (Surface)surfaceObject;
        } else if (surfaceObject instanceof SurfaceTexture) {
            surface = new Surface((SurfaceTexture)surfaceObject);
            needRelease = true;
        } else {
            Log.e(TAG, "Not a valid surfaceObject: " + surfaceObject);
            return;
        }
        try {
            mCallback.establishSurfacePeer(pid, type, surface, primaryID, secondaryID);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call establishSurfaceTexturePeer: " + e);
            return;
        } finally {
            if (needRelease) {
                surface.release();
            }
        }
    }

    /**
     * The main entry point for a sandboxed process. This should be called from a new thread since
     * it will not return until the sandboxed process exits. See sandboxed_process_service.{h,cc}
     *
     * @param applicationContext The Application Context of the current process.
     * @param service The current SandboxedProcessService object.
     * @param ipcFd File descriptor to use for ipc.
     * @param crashFd File descriptor for signaling crashes.
     */
    private static native void nativeInitSandboxedProcess(Context applicationContext,
            SandboxedProcessService service, int ipcFd, int crashFd);

    /**
     * Force the sandboxed process to exit.
     */
    private static native void nativeExitSandboxedProcess();
}
