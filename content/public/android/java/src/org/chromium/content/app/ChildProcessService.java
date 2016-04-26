// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcelable;
import android.os.Process;
import android.os.RemoteException;
import android.view.Surface;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.Linker;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.ChildProcessConstants;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.browser.FileDescriptorInfo;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content.common.IChildProcessCallback;
import org.chromium.content.common.IChildProcessService;
import org.chromium.content.common.SurfaceWrapper;

import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicReference;

/**
 * This is the base class for child services; the [Non]SandboxedProcessService0, 1.. etc
 * subclasses provide the concrete service entry points, to enable the browser to connect
 * to more than one distinct process (i.e. one process per service number, up to limit of N).
 * The embedding application must declare these service instances in the application section
 * of its AndroidManifest.xml, for example with N entries of the form:-
 *     <service android:name="org.chromium.content.app.[Non]SandboxedProcessServiceX"
 *              android:process=":[non]sandboxed_processX" />
 * for X in 0...N-1 (where N is {@link ChildProcessLauncher#MAX_REGISTERED_SERVICES})
 */
@JNINamespace("content")
@SuppressWarnings("SynchronizeOnNonFinalField")
public class ChildProcessService extends Service {
    private static final String MAIN_THREAD_NAME = "ChildProcessMain";
    private static final String TAG = "ChildProcessService";
    protected static final FileDescriptorInfo[] EMPTY_FILE_DESCRIPTOR_INFO = {};
    private IChildProcessCallback mCallback;

    // This is the native "Main" thread for the renderer / utility process.
    private Thread mMainThread;
    // Parameters received via IPC, only accessed while holding the mMainThread monitor.
    private String[] mCommandLineParams;
    private int mCpuCount;
    private long mCpuFeatures;
    // File descriptors that should be registered natively.
    private FileDescriptorInfo[] mFdInfos;
    // Linker-specific parameters for this child process service.
    private ChromiumLinkerParams mLinkerParams;
    // Child library process type.
    private int mLibraryProcessType;

    private static AtomicReference<Context> sContext = new AtomicReference<Context>(null);
    private boolean mLibraryInitialized = false;
    // Becomes true once the service is bound. Access must synchronize around mMainThread.
    private boolean mIsBound = false;

    private final Semaphore mActivitySemaphore = new Semaphore(1);

    // Return a Linker instance. If testing, the Linker needs special setup.
    private Linker getLinker() {
        if (Linker.areTestsEnabled()) {
            // For testing, set the Linker implementation and the test runner
            // class name to match those used by the parent.
            assert mLinkerParams != null;
            Linker.setupForTesting(
                    mLinkerParams.mLinkerImplementationForTesting,
                    mLinkerParams.mTestRunnerClassNameForTesting);
        }
        return Linker.getInstance();
    }

    // Binder object used by clients for this service.
    private final IChildProcessService.Stub mBinder = new IChildProcessService.Stub() {
        // NOTE: Implement any IChildProcessService methods here.
        @Override
        public int setupConnection(Bundle args, IChildProcessCallback callback) {
            mCallback = callback;
            getServiceInfo(args);
            return Process.myPid();
        }

        @Override
        public void crashIntentionallyForTesting() {
            Process.killProcess(Process.myPid());
        }
    };

    /* package */ static Context getContext() {
        return sContext.get();
    }

    @Override
    public void onCreate() {
        Log.i(TAG, "Creating new ChildProcessService pid=%d", Process.myPid());
        if (sContext.get() != null) {
            throw new RuntimeException("Illegal child process reuse.");
        }
        sContext.set(this);
        super.onCreate();

        ContextUtils.initApplicationContext(getApplicationContext());

        mMainThread = new Thread(new Runnable() {
            @Override
            @SuppressFBWarnings("DM_EXIT")
            public void run()  {
                try {
                    // CommandLine must be initialized before everything else.
                    synchronized (mMainThread) {
                        while (mCommandLineParams == null) {
                            mMainThread.wait();
                        }
                    }
                    CommandLine.init(mCommandLineParams);

                    Linker linker = null;
                    boolean requestedSharedRelro = false;
                    if (Linker.isUsed()) {
                        synchronized (mMainThread) {
                            while (!mIsBound) {
                                mMainThread.wait();
                            }
                        }
                        linker = getLinker();
                        if (mLinkerParams.mWaitForSharedRelro) {
                            requestedSharedRelro = true;
                            linker.initServiceProcess(mLinkerParams.mBaseLoadAddress);
                        } else {
                            linker.disableSharedRelros();
                        }
                    }
                    boolean isLoaded = false;
                    if (CommandLine.getInstance().hasSwitch(
                            BaseSwitches.RENDERER_WAIT_FOR_JAVA_DEBUGGER)) {
                        android.os.Debug.waitForDebugger();
                    }

                    boolean loadAtFixedAddressFailed = false;
                    try {
                        LibraryLoader.get(mLibraryProcessType).loadNow(getApplicationContext());
                        isLoaded = true;
                    } catch (ProcessInitException e) {
                        if (requestedSharedRelro) {
                            Log.w(TAG, "Failed to load native library with shared RELRO, "
                                    + "retrying without");
                            loadAtFixedAddressFailed = true;
                        } else {
                            Log.e(TAG, "Failed to load native library", e);
                        }
                    }
                    if (!isLoaded && requestedSharedRelro) {
                        linker.disableSharedRelros();
                        try {
                            LibraryLoader.get(mLibraryProcessType).loadNow(getApplicationContext());
                            isLoaded = true;
                        } catch (ProcessInitException e) {
                            Log.e(TAG, "Failed to load native library on retry", e);
                        }
                    }
                    if (!isLoaded) {
                        System.exit(-1);
                    }
                    LibraryLoader.get(mLibraryProcessType)
                            .registerRendererProcessHistogram(requestedSharedRelro,
                                    loadAtFixedAddressFailed);
                    LibraryLoader.get(mLibraryProcessType).initialize();
                    synchronized (mMainThread) {
                        mLibraryInitialized = true;
                        mMainThread.notifyAll();
                        while (mFdInfos == null) {
                            mMainThread.wait();
                        }
                    }
                    for (FileDescriptorInfo fdInfo : mFdInfos) {
                        nativeRegisterGlobalFileDescriptor(
                                fdInfo.mId, fdInfo.mFd.detachFd(), fdInfo.mOffset, fdInfo.mSize);
                    }
                    nativeInitChildProcess(ChildProcessService.this, mCpuCount, mCpuFeatures);
                    if (mActivitySemaphore.tryAcquire()) {
                        ContentMain.start();
                        nativeExitChildProcess();
                    }
                } catch (InterruptedException e) {
                    Log.w(TAG, "%s startup failed: %s", MAIN_THREAD_NAME, e);
                } catch (ProcessInitException e) {
                    Log.w(TAG, "%s startup failed: %s", MAIN_THREAD_NAME, e);
                }
            }
        }, MAIN_THREAD_NAME);
        mMainThread.start();
    }

    @Override
    @SuppressFBWarnings("DM_EXIT")
    public void onDestroy() {
        Log.i(TAG, "Destroying ChildProcessService pid=%d", Process.myPid());
        super.onDestroy();
        if (mActivitySemaphore.tryAcquire()) {
            // TODO(crbug.com/457406): This is a bit hacky, but there is no known better solution
            // as this service will get reused (at least if not sandboxed).
            // In fact, we might really want to always exit() from onDestroy(), not just from
            // the early return here.
            System.exit(0);
            return;
        }
        synchronized (mMainThread) {
            try {
                while (!mLibraryInitialized) {
                    // Avoid a potential race in calling through to native code before the library
                    // has loaded.
                    mMainThread.wait();
                }
            } catch (InterruptedException e) {
                // Ignore
            }
        }
        // Try to shutdown the MainThread gracefully, but it might not
        // have chance to exit normally.
        nativeShutdownMainThread();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // We call stopSelf() to request that this service be stopped as soon as the client
        // unbinds. Otherwise the system may keep it around and available for a reconnect. The
        // child processes do not currently support reconnect; they must be initialized from
        // scratch every time.
        stopSelf();
        initializeParams(intent);
        return mBinder;
    }

    /**
     * Helper method to initialize the params from intent.
     * @param intent Intent to launch the service.
     */
    protected void initializeParams(Intent intent) {
        synchronized (mMainThread) {
            mCommandLineParams =
                    intent.getStringArrayExtra(ChildProcessConstants.EXTRA_COMMAND_LINE);
            // mLinkerParams is never used if Linker.isUsed() returns false.
            // See onCreate().
            mLinkerParams = new ChromiumLinkerParams(intent);
            mLibraryProcessType =
                    ChildProcessLauncher.ChildProcessCreationParams.getLibraryProcessType(intent);
            mIsBound = true;
            mMainThread.notifyAll();
        }
    }

    /**
     * Helper method to get the information about the service from a given bundle.
     * @param bundle Bundle that contains the information to start the service.
     */
    void getServiceInfo(Bundle bundle) {
        // Required to unparcel FileDescriptorInfo.
        bundle.setClassLoader(getClassLoader());
        synchronized (mMainThread) {
            // Allow the command line to be set via bind() intent or setupConnection, but
            // the FD can only be transferred here.
            if (mCommandLineParams == null) {
                mCommandLineParams =
                        bundle.getStringArray(ChildProcessConstants.EXTRA_COMMAND_LINE);
            }
            // We must have received the command line by now
            assert mCommandLineParams != null;
            mCpuCount = bundle.getInt(ChildProcessConstants.EXTRA_CPU_COUNT);
            mCpuFeatures = bundle.getLong(ChildProcessConstants.EXTRA_CPU_FEATURES);
            assert mCpuCount > 0;
            Parcelable[] fdInfosAsParcelable =
                    bundle.getParcelableArray(ChildProcessConstants.EXTRA_FILES);
            if (fdInfosAsParcelable != null) {
                // For why this arraycopy is necessary:
                // http://stackoverflow.com/questions/8745893/i-dont-get-why-this-classcastexception-occurs
                mFdInfos = new FileDescriptorInfo[fdInfosAsParcelable.length];
                System.arraycopy(fdInfosAsParcelable, 0, mFdInfos, 0, fdInfosAsParcelable.length);
            } else {
                String processType = ContentSwitches.getSwitchValue(
                        mCommandLineParams, ContentSwitches.SWITCH_PROCESS_TYPE);
                assert ContentSwitches.SWITCH_DOWNLOAD_PROCESS.equals(processType);
                mFdInfos = EMPTY_FILE_DESCRIPTOR_INFO;
            }
            Bundle sharedRelros = bundle.getBundle(Linker.EXTRA_LINKER_SHARED_RELROS);
            if (sharedRelros != null) {
                getLinker().useSharedRelros(sharedRelros);
                sharedRelros = null;
            }
            mMainThread.notifyAll();
        }
    }

    /**
     * Called from native code to share a surface texture with another child process.
     * Through using the callback object the browser is used as a proxy to route the
     * call to the correct process.
     *
     * @param pid Process handle of the child process to share the SurfaceTexture with.
     * @param surfaceObject The Surface or SurfaceTexture to share with the other child process.
     * @param primaryID Used to route the call to the correct client instance.
     * @param secondaryID Used to route the call to the correct client instance.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void establishSurfaceTexturePeer(
            int pid, Object surfaceObject, int primaryID, int secondaryID) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }

        Surface surface = null;
        boolean needRelease = false;
        if (surfaceObject instanceof Surface) {
            surface = (Surface) surfaceObject;
        } else if (surfaceObject instanceof SurfaceTexture) {
            surface = new Surface((SurfaceTexture) surfaceObject);
            needRelease = true;
        } else {
            Log.e(TAG, "Not a valid surfaceObject: %s", surfaceObject);
            return;
        }
        try {
            mCallback.establishSurfacePeer(pid, surface, primaryID, secondaryID);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call establishSurfaceTexturePeer: %s", e);
            return;
        } finally {
            if (needRelease) {
                surface.release();
            }
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private Surface getViewSurface(int surfaceId) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return null;
        }

        try {
            SurfaceWrapper wrapper = mCallback.getViewSurface(surfaceId);
            return wrapper != null ? wrapper.getSurface() : null;
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call getViewSurface: %s", e);
            return null;
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void createSurfaceTextureSurface(
            int surfaceTextureId, int clientId, SurfaceTexture surfaceTexture) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }

        Surface surface = new Surface(surfaceTexture);
        try {
            mCallback.registerSurfaceTextureSurface(surfaceTextureId, clientId, surface);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call registerSurfaceTextureSurface: %s", e);
        }
        surface.release();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void destroySurfaceTextureSurface(int surfaceTextureId, int clientId) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }

        try {
            mCallback.unregisterSurfaceTextureSurface(surfaceTextureId, clientId);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call unregisterSurfaceTextureSurface: %s", e);
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private Surface getSurfaceTextureSurface(int surfaceTextureId) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return null;
        }

        try {
            return mCallback.getSurfaceTextureSurface(surfaceTextureId).getSurface();
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call getSurfaceTextureSurface: %s", e);
            return null;
        }
    }

    /**
     * Helper for registering FileDescriptorInfo objects with GlobalFileDescriptors.
     * This includes the IPC channel, the crash dump signals and resource related
     * files.
     */
    private static native void nativeRegisterGlobalFileDescriptor(
            int id, int fd, long offset, long size);

    /**
     * The main entry point for a child process. This should be called from a new thread since
     * it will not return until the child process exits. See child_process_service.{h,cc}
     *
     * @param service The current ChildProcessService object.
     * renderer.
     */
    private static native void nativeInitChildProcess(
            ChildProcessService service, int cpuCount, long cpuFeatures);

    /**
     * Force the child process to exit.
     */
    private static native void nativeExitChildProcess();

    private native void nativeShutdownMainThread();
}
