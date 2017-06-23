// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.CpuFeatures;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.Linker;
import org.chromium.base.process_launcher.ChildProcessConstants;
import org.chromium.base.process_launcher.ChildProcessCreationParams;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.content.app.ChromiumLinkerParams;
import org.chromium.content.app.SandboxedProcessService;
import org.chromium.content.common.ContentSwitches;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * This is the java counterpart to ChildProcessLauncherHelper. It is owned by native side and
 * has an explicit destroy method.
 * Each public or jni methods should have explicit documentation on what threads they are called.
 */
@JNINamespace("content::internal")
public class ChildProcessLauncherHelper {
    private static final String TAG = "ChildProcLH";

    // Manifest values used to specify the service names.
    private static final String NUM_SANDBOXED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_SANDBOXED_SERVICES";
    private static final String SANDBOXED_SERVICES_NAME_KEY =
            "org.chromium.content.browser.SANDBOXED_SERVICES_NAME";
    private static final String NUM_PRIVILEGED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_PRIVILEGED_SERVICES";
    private static final String PRIVILEGED_SERVICES_NAME_KEY =
            "org.chromium.content.browser.PRIVILEGED_SERVICES_NAME";

    // Represents an invalid process handle; same as base/process/process.h kNullProcessHandle.
    private static final int NULL_PROCESS_HANDLE = 0;

    // Delay between the call to freeConnection and the connection actually beeing   freed.
    private static final long FREE_CONNECTION_DELAY_MILLIS = 1;

    // Factory used by the SpareConnection to create the actual ChildProcessConnection.
    private static final SpareChildConnection.ConnectionFactory SANDBOXED_CONNECTION_FACTORY =
            new SpareChildConnection.ConnectionFactory() {
                @Override
                public ChildProcessConnection allocateBoundConnection(Context context,
                        ChildProcessCreationParams creationParams,
                        ChildProcessConnection.StartCallback startCallback,
                        ChildProcessConnection.DeathCallback deathCallback) {
                    boolean bindToCallerCheck =
                            creationParams == null ? false : creationParams.getBindToCallerCheck();
                    ChildConnectionAllocator allocator =
                            getConnectionAllocator(context, creationParams, true /* sandboxed */);
                    return ChildProcessLauncherHelper.allocateBoundConnection(context, allocator,
                            false /* useStrongBinding */, true /* useBindingManager */,
                            ChildProcessLauncherHelper.createServiceBundle(bindToCallerCheck),
                            startCallback, deathCallback);
                }
            };

    // A warmed-up connection to a sandboxed service.
    private static SpareChildConnection sSpareSandboxedConnection;

    // Map from package name to ChildConnectionAllocator.
    private static final Map<String, ChildConnectionAllocator>
            sSandboxedChildConnectionAllocatorMap = new HashMap<>();

    // Allocator used for non-sandboxed services.
    private static ChildConnectionAllocator sPrivilegedChildConnectionAllocator;

    // Used by tests to override the default sandboxed service allocator settings.
    private static ChildConnectionAllocator.ConnectionFactory sSandboxedServiceFactoryForTesting;
    private static int sSandboxedServicesCountForTesting = -1;
    private static String sSandboxedServicesNameForTesting;

    // Map from PID to ChildService connection.
    private static Map<Integer, ChildProcessLauncherHelper> sLauncherByPid = new HashMap<>();

    // Manages OOM bindings used to bind chind services. Lazily initialized by getBindingManager().
    private static BindingManager sBindingManager;

    // Whether the main application is currently brought to the foreground.
    private static boolean sApplicationInForeground = true;

    // The IBinder provided to the created service.
    private final IBinder mIBinderCallback;

    // Whether the connection is managed by the BindingManager.
    private final boolean mUseBindingManager;

    // Whether the connection has a strong binding (which also means it is not managed by the
    // BindingManager).
    private final boolean mUseStrongBinding;

    // Whether the connection should be setup once connected. Tests can set this to false to
    // simulate a connected but not yet setup connection.
    private final boolean mDoSetupConnection;

    private final ChildProcessCreationParams mCreationParams;

    private final String[] mCommandLine;

    private final FileDescriptorInfo[] mFilesToBeMapped;

    // The allocator used to create the connection.
    private final ChildConnectionAllocator mConnectionAllocator;

    // Whether the created process should be sandboxed.
    private final boolean mSandboxed;

    // Note native pointer is only guaranteed live until nativeOnChildProcessStarted.
    private long mNativeChildProcessLauncherHelper;

    // The actual service connection. Set once we have connected to the service.
    private ChildProcessConnection mConnection;

    @CalledByNative
    private static FileDescriptorInfo makeFdInfo(
            int id, int fd, boolean autoClose, long offset, long size) {
        assert LauncherThread.runningOnLauncherThread();
        ParcelFileDescriptor pFd;
        if (autoClose) {
            // Adopt the FD, it will be closed when we close the ParcelFileDescriptor.
            pFd = ParcelFileDescriptor.adoptFd(fd);
        } else {
            try {
                pFd = ParcelFileDescriptor.fromFd(fd);
            } catch (IOException e) {
                Log.e(TAG, "Invalid FD provided for process connection, aborting connection.", e);
                return null;
            }
        }
        return new FileDescriptorInfo(id, pFd, offset, size);
    }

    @VisibleForTesting
    @CalledByNative
    public static ChildProcessLauncherHelper createAndStart(long nativePointer, int paramId,
            String[] commandLine, FileDescriptorInfo[] filesToBeMapped) {
        assert LauncherThread.runningOnLauncherThread();
        ChildProcessCreationParams creationParams = ChildProcessCreationParams.get(paramId);
        if (paramId != ChildProcessCreationParams.DEFAULT_ID && creationParams == null) {
            throw new RuntimeException("CreationParams id " + paramId + " not found");
        }

        String processType =
                ContentSwitches.getSwitchValue(commandLine, ContentSwitches.SWITCH_PROCESS_TYPE);

        boolean sandboxed = true;
        boolean useStrongBinding = false;
        if (!ContentSwitches.SWITCH_RENDERER_PROCESS.equals(processType)) {
            if (ContentSwitches.SWITCH_GPU_PROCESS.equals(processType)) {
                sandboxed = false;
                useStrongBinding = true;
            } else {
                // We only support sandboxed utility processes now.
                assert ContentSwitches.SWITCH_UTILITY_PROCESS.equals(processType);
            }
        }

        IBinder binderCallback = ContentSwitches.SWITCH_GPU_PROCESS.equals(processType)
                ? new GpuProcessCallback()
                : null;

        ChildProcessLauncherHelper process_launcher = new ChildProcessLauncherHelper(nativePointer,
                creationParams, commandLine, filesToBeMapped, useStrongBinding, sandboxed,
                binderCallback, true /* doSetupConnection */);
        process_launcher.start();
        return process_launcher;
    }

    /**
     * Creates a ready to use sandboxed child process. Should be called early during startup so the
     * child process is created while other startup work is happening.
     * @param context the application context used for the connection.
     */
    public static void warmUp(final Context context) {
        assert ThreadUtils.runningOnUiThread();
        LauncherThread.post(new Runnable() {
            @Override
            public void run() {
                if (sSpareSandboxedConnection != null && !sSpareSandboxedConnection.isEmpty()) {
                    return;
                }
                sSpareSandboxedConnection =
                        new SpareChildConnection(context, SANDBOXED_CONNECTION_FACTORY,
                                ChildProcessCreationParams.getDefault(), true /* sandboxed */);
            }
        });
    }

    public static String getPackageNameFromCreationParams(
            Context context, ChildProcessCreationParams params, boolean sandboxed) {
        return (sandboxed && params != null) ? params.getPackageNameForSandboxedService()
                                             : context.getPackageName();
    }

    public static boolean isServiceExternalFromCreationParams(
            ChildProcessCreationParams params, boolean sandboxed) {
        return sandboxed && params != null && params.getIsSandboxedServiceExternal();
    }

    /**
     * Starts the moderate binding management that adjust a process priority in response to various
     * signals (app sent to background/foreground for example).
     * Note: WebAPKs and non WebAPKs share the same moderate binding pool, so the size of the
     * shared moderate binding pool is always set based on the number of sandboxes processes
     * used by Chrome.
     * @param context Android's context.
     */
    public static void startModerateBindingManagement(final Context context) {
        assert ThreadUtils.runningOnUiThread();
        LauncherThread.post(new Runnable() {
            @Override
            public void run() {
                ChildConnectionAllocator allocator = getConnectionAllocator(
                        context, ChildProcessCreationParams.getDefault(), true /* sandboxed */);
                getBindingManager().startModerateBindingManagement(
                        context, allocator.getNumberOfServices());
            }
        });
    }

    // Lazy initialize sBindingManager
    // TODO(boliu): This should be internal to content.
    @SuppressFBWarnings("LI_LAZY_INIT_STATIC") // Method is single thread.
    public static BindingManager getBindingManager() {
        assert LauncherThread.runningOnLauncherThread();
        if (sBindingManager == null) {
            sBindingManager = BindingManagerImpl.createBindingManager();
        }
        return sBindingManager;
    }

    /**
     * Called when the embedding application is sent to background.
     */
    public static void onSentToBackground() {
        assert ThreadUtils.runningOnUiThread();
        sApplicationInForeground = false;
        LauncherThread.post(new Runnable() {
            @Override
            public void run() {
                getBindingManager().onSentToBackground();
            }
        });
    }

    /**
     * Called when the embedding application is brought to foreground.
     */
    public static void onBroughtToForeground() {
        assert ThreadUtils.runningOnUiThread();
        sApplicationInForeground = true;
        LauncherThread.post(new Runnable() {
            @Override
            public void run() {
                getBindingManager().onBroughtToForeground();
            }
        });
    }

    @VisibleForTesting
    public static void setSandboxServicesSettingsForTesting(
            ChildConnectionAllocator.ConnectionFactory factory, int serviceCount,
            String serviceName) {
        sSandboxedServiceFactoryForTesting = factory;
        sSandboxedServicesCountForTesting = serviceCount;
        sSandboxedServicesNameForTesting = serviceName;
    }

    @VisibleForTesting
    public static void setBindingManagerForTesting(BindingManager manager) {
        sBindingManager = manager;
    }

    @VisibleForTesting
    static ChildConnectionAllocator getConnectionAllocator(
            Context context, ChildProcessCreationParams creationParams, boolean sandboxed) {
        assert LauncherThread.runningOnLauncherThread();
        String packageName = getPackageNameFromCreationParams(context, creationParams, sandboxed);
        boolean bindAsExternalService =
                isServiceExternalFromCreationParams(creationParams, sandboxed);

        if (!sandboxed) {
            if (sPrivilegedChildConnectionAllocator == null) {
                sPrivilegedChildConnectionAllocator = ChildConnectionAllocator.create(context,
                        creationParams, packageName, PRIVILEGED_SERVICES_NAME_KEY,
                        NUM_PRIVILEGED_SERVICES_KEY, bindAsExternalService);
            }
            return sPrivilegedChildConnectionAllocator;
        }

        if (!sSandboxedChildConnectionAllocatorMap.containsKey(packageName)) {
            Log.w(TAG,
                    "Create a new ChildConnectionAllocator with package name = %s,"
                            + " sandboxed = true",
                    packageName);
            ChildConnectionAllocator connectionAllocator = null;
            if (sSandboxedServicesCountForTesting != -1) {
                // Testing case where allocator settings are overriden.
                String serviceName = !TextUtils.isEmpty(sSandboxedServicesNameForTesting)
                        ? sSandboxedServicesNameForTesting
                        : SandboxedProcessService.class.getName();
                connectionAllocator = ChildConnectionAllocator.createForTest(creationParams,
                        packageName, serviceName, sSandboxedServicesCountForTesting,
                        bindAsExternalService);
            } else {
                connectionAllocator = ChildConnectionAllocator.create(context, creationParams,
                        packageName, SANDBOXED_SERVICES_NAME_KEY, NUM_SANDBOXED_SERVICES_KEY,
                        bindAsExternalService);
            }
            if (sSandboxedServiceFactoryForTesting != null) {
                connectionAllocator.setConnectionFactoryForTesting(
                        sSandboxedServiceFactoryForTesting);
            }
            sSandboxedChildConnectionAllocatorMap.put(packageName, connectionAllocator);
        }
        return sSandboxedChildConnectionAllocatorMap.get(packageName);
    }

    private static ChildProcessConnection allocateBoundConnection(Context context,
            final ChildConnectionAllocator connectionAllocator, boolean useStrongBinding,
            boolean useBindingManager, Bundle serviceBundle,
            ChildProcessConnection.StartCallback startCallback,
            final ChildProcessConnection.DeathCallback deathCallback) {
        assert LauncherThread.runningOnLauncherThread();

        ChildProcessConnection.DeathCallback deathCallbackWrapper =
                new ChildProcessConnection.DeathCallback() {
                    @Override
                    public void onChildProcessDied(ChildProcessConnection connection) {
                        assert LauncherThread.runningOnLauncherThread();
                        if (connection.getPid() != 0) {
                            stop(connection.getPid());
                        } else {
                            freeConnection(connectionAllocator, connection);
                        }
                        // Forward the call to the provided callback if any. The spare connection
                        // uses that for clean-up.
                        if (deathCallback != null) {
                            deathCallback.onChildProcessDied(connection);
                        }
                    }
                };

        ChildProcessConnection connection =
                connectionAllocator.allocate(context, serviceBundle, deathCallbackWrapper);
        if (connection != null) {
            connection.start(useStrongBinding, startCallback);
            if (useBindingManager && !connectionAllocator.isFreeConnectionAvailable()) {
                // Proactively releases all the moderate bindings once all the sandboxed services
                // are allocated, which will be very likely to have some of them killed by OOM
                // killer.
                getBindingManager().releaseAllModerateBindings();
            }
        }
        return connection;
    }

    private ChildProcessLauncherHelper(long nativePointer,
            ChildProcessCreationParams creationParams, String[] commandLine,
            FileDescriptorInfo[] filesToBeMapped, boolean useStrongBinding, boolean sandboxed,
            IBinder binderCallback, boolean doSetupConnection) {
        assert LauncherThread.runningOnLauncherThread();

        mCreationParams = creationParams;
        mCommandLine = commandLine;
        mFilesToBeMapped = filesToBeMapped;
        mUseStrongBinding = useStrongBinding;
        mUseBindingManager = sandboxed;
        mNativeChildProcessLauncherHelper = nativePointer;
        mIBinderCallback = binderCallback;
        mDoSetupConnection = doSetupConnection;
        mSandboxed = sandboxed;

        mConnectionAllocator = getConnectionAllocator(
                ContextUtils.getApplicationContext(), mCreationParams, sandboxed);

        initLinker();
    }

    private void start() {
        assert LauncherThread.runningOnLauncherThread();
        assert mConnection == null;
        try {
            TraceEvent.begin("ChildProcessLauncher.start");

            ChildProcessConnection.StartCallback startCallback =
                    new ChildProcessConnection.StartCallback() {
                        @Override
                        public void onChildStarted() {}

                        @Override
                        public void onChildStartFailed() {
                            assert LauncherThread.runningOnLauncherThread();
                            Log.e(TAG, "ChildProcessConnection.start failed, trying again");
                            LauncherThread.post(new Runnable() {
                                @Override
                                public void run() {
                                    // The child process may already be bound to another client
                                    // (this can happen if multi-process WebView is used in more
                                    // than one process), so try starting the process again.
                                    // This connection that failed to start has not been freed,
                                    // so a new bound connection will be allocated.
                                    mConnection = null;
                                    start();
                                }
                            });
                        }
                    };

            // Try to use the spare connection if there's one.
            if (sSpareSandboxedConnection != null) {
                mConnection = sSpareSandboxedConnection.getConnection(
                        mCreationParams, mSandboxed, startCallback);
                if (mConnection != null) {
                    Log.d(TAG, "Using warmed-up connection for service %s.",
                            mConnection.getServiceName());
                    // Clear the spare connection now that it's been used.
                    sSpareSandboxedConnection = null;
                }
            }

            allocateAndSetupConnection(startCallback);
        } finally {
            TraceEvent.end("ChildProcessLauncher.start");
        }
    }

    // Allocates a new connection and calls setUp on it.
    // If there are no available connections, it will retry when one becomes available.
    private boolean allocateAndSetupConnection(
            final ChildProcessConnection.StartCallback startCallback) {
        if (mConnection == null) {
            final Context context = ContextUtils.getApplicationContext();
            boolean bindToCallerCheck =
                    mCreationParams == null ? false : mCreationParams.getBindToCallerCheck();
            Bundle serviceBundle = createServiceBundle(bindToCallerCheck);
            onBeforeConnectionAllocated(serviceBundle);

            mConnection = allocateBoundConnection(context, mConnectionAllocator, mUseStrongBinding,
                    mUseBindingManager, serviceBundle, startCallback, null /* deathCallback */);
            if (mConnection == null) {
                // No connection is available at this time. Add a listener so when one becomes
                // available we can create the service.
                mConnectionAllocator.addListener(new ChildConnectionAllocator.Listener() {
                    @Override
                    public void onConnectionFreed(
                            ChildConnectionAllocator allocator, ChildProcessConnection connection) {
                        assert allocator == mConnectionAllocator;
                        if (!allocator.isFreeConnectionAvailable()) return;
                        allocator.removeListener(this);
                        boolean success = allocateAndSetupConnection(startCallback);
                        assert success;
                    }
                });
                return false;
            }
        }

        assert mConnection != null;
        Bundle connectionBundle = createConnectionBundle();
        onConnectionBound(mConnection, mConnectionAllocator, connectionBundle);

        if (mDoSetupConnection) {
            setupConnection(connectionBundle);
        }
        return true;
    }

    private void setupConnection(Bundle connectionBundle) {
        ChildProcessConnection.ConnectionCallback connectionCallback =
                new ChildProcessConnection.ConnectionCallback() {
                    @Override
                    public void onConnected(ChildProcessConnection connection) {
                        assert mConnection == connection;
                        onChildProcessStarted();
                    }
                };
        mConnection.setupConnection(connectionBundle, getIBinderCallback(), connectionCallback);
    }

    private static void freeConnection(final ChildConnectionAllocator connectionAllocator,
            final ChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();

        // Freeing a service should be delayed. This is so that we avoid immediately reusing the
        // freed service (see http://crbug.com/164069): the framework might keep a service process
        // alive when it's been unbound for a short time. If a new connection to the same service
        // is bound at that point, the process is reused and bad things happen (mostly static
        // variables are set when we don't expect them to).
        LauncherThread.postDelayed(new Runnable() {
            @Override
            public void run() {
                assert connectionAllocator != null;
                connectionAllocator.free(connection);
                String packageName = connectionAllocator.getPackageName();
                if (!connectionAllocator.anyConnectionAllocated()
                        && sSandboxedChildConnectionAllocatorMap.get(packageName)
                                == connectionAllocator) {
                    sSandboxedChildConnectionAllocatorMap.remove(packageName);
                }
            }
        }, FREE_CONNECTION_DELAY_MILLIS);
    }

    public void onChildProcessStarted() {
        assert LauncherThread.runningOnLauncherThread();

        int pid = mConnection.getPid();
        Log.d(TAG, "on connect callback, pid=%d", pid);

        onConnectionEstablished(mConnection);

        sLauncherByPid.put(pid, this);

        // Proactively close the FDs rather than waiting for the GC to do it.
        try {
            for (FileDescriptorInfo fileInfo : mFilesToBeMapped) {
                fileInfo.fd.close();
            }
        } catch (IOException ioe) {
            Log.w(TAG, "Failed to close FD.", ioe);
        }

        // If the connection fails and pid == 0, the Java-side cleanup was already handled by
        // DeathCallback. We still have to call back to native for cleanup there.
        if (mNativeChildProcessLauncherHelper != 0) {
            nativeOnChildProcessStarted(mNativeChildProcessLauncherHelper, getPid());
        }
        mNativeChildProcessLauncherHelper = 0;
    }

    public int getPid() {
        assert LauncherThread.runningOnLauncherThread();
        return mConnection == null ? NULL_PROCESS_HANDLE : mConnection.getPid();
    }

    // Called on client (UI or IO) thread.
    @CalledByNative
    private boolean isOomProtected() {
        // mConnection is set on a different thread but does not change once it's been set. So it is
        // safe to test whether it's null from a different thread.
        if (mConnection == null) {
            return false;
        }

        // We consider the process to be child protected if it has a strong or moderate binding and
        // the app is in the foreground.
        return sApplicationInForeground && !mConnection.isWaivedBoundOnlyOrWasWhenDied();
    }

    @CalledByNative
    private void setInForeground(int pid, boolean foreground, boolean boostForPendingViews) {
        assert LauncherThread.runningOnLauncherThread();
        assert mConnection != null;
        assert getPid() == pid;
        getBindingManager().setPriority(pid, foreground, boostForPendingViews);
    }

    @CalledByNative
    static void stop(int pid) {
        assert LauncherThread.runningOnLauncherThread();
        Log.d(TAG, "stopping child connection: pid=%d", pid);
        ChildProcessLauncherHelper launcher = sLauncherByPid.remove(pid);
        if (launcher == null) {
            // Can happen for single process.
            return;
        }
        launcher.onConnectionLost(launcher.mConnection, pid);
        launcher.mConnection.stop();
        freeConnection(launcher.mConnectionAllocator, launcher.mConnection);
    }

    @CalledByNative
    private static int getNumberOfRendererSlots() {
        assert ThreadUtils.runningOnUiThread();
        if (sSandboxedServicesCountForTesting != -1) {
            return sSandboxedServicesCountForTesting;
        }

        final ChildProcessCreationParams params = ChildProcessCreationParams.getDefault();
        final Context context = ContextUtils.getApplicationContext();
        final String packageName = ChildProcessLauncherHelper.getPackageNameFromCreationParams(
                context, params, true /* inSandbox */);
        try {
            return ChildConnectionAllocator.getNumberOfServices(
                    context, packageName, NUM_SANDBOXED_SERVICES_KEY);
        } catch (RuntimeException e) {
            // Unittest packages do not declare services. Some tests require a realistic number
            // to test child process policies, so pick a high-ish number here.
            return 65535;
        }
    }

    // Can be called on a number of threads, including launcher, and binder.
    private static native void nativeOnChildProcessStarted(
            long nativeChildProcessLauncherHelper, int pid);

    private static boolean sLinkerInitialized;
    private static long sLinkerLoadAddress;
    @VisibleForTesting
    static void initLinker() {
        assert LauncherThread.runningOnLauncherThread();
        if (sLinkerInitialized) return;
        if (Linker.isUsed()) {
            sLinkerLoadAddress = Linker.getInstance().getBaseLoadAddress();
            if (sLinkerLoadAddress == 0) {
                Log.i(TAG, "Shared RELRO support disabled!");
            }
        }
        sLinkerInitialized = true;
    }

    private static ChromiumLinkerParams getLinkerParamsForNewConnection() {
        assert LauncherThread.runningOnLauncherThread();
        assert sLinkerInitialized;

        if (sLinkerLoadAddress == 0) return null;

        // Always wait for the shared RELROs in service processes.
        final boolean waitForSharedRelros = true;
        if (Linker.areTestsEnabled()) {
            Linker linker = Linker.getInstance();
            return new ChromiumLinkerParams(sLinkerLoadAddress, waitForSharedRelros,
                    linker.getTestRunnerClassNameForTesting(),
                    linker.getImplementationForTesting());
        } else {
            return new ChromiumLinkerParams(sLinkerLoadAddress, waitForSharedRelros);
        }
    }

    /**
     * Creates the common bundle to be passed to child processes through the service binding intent.
     * If the service gets recreated by the framework the intent will be reused, so these parameters
     * should be common to all processes of that type.
     *
     * @param commandLine Command line params to be passed to the service.
     * @param linkerParams Linker params to start the service.
     */
    private static Bundle createServiceBundle(boolean bindToCallerCheck) {
        initLinker();
        Bundle bundle = new Bundle();
        bundle.putBoolean(ChildProcessConstants.EXTRA_BIND_TO_CALLER, bindToCallerCheck);
        bundle.putParcelable(ContentChildProcessConstants.EXTRA_LINKER_PARAMS,
                getLinkerParamsForNewConnection());
        return bundle;
    }

    public Bundle createConnectionBundle() {
        Bundle bundle = new Bundle();
        bundle.putStringArray(ChildProcessConstants.EXTRA_COMMAND_LINE, mCommandLine);
        bundle.putParcelableArray(ChildProcessConstants.EXTRA_FILES, mFilesToBeMapped);
        return bundle;
    }

    // Below are methods that will eventually be moved to a content delegate class.

    private void onBeforeConnectionAllocated(Bundle commonParameters) {
        // TODO(jcivelli): move createServiceBundle in there.
    }

    // Called once a connection has been allocated.
    private void onConnectionBound(ChildProcessConnection connection,
            ChildConnectionAllocator connectionAllocator, Bundle connectionBundle) {
        if (mUseBindingManager && !connectionAllocator.isFreeConnectionAvailable()) {
            // Proactively releases all the moderate bindings once all the sandboxed services are
            // allocated, which will be very likely to have some of them killed by OOM killer.
            getBindingManager().releaseAllModerateBindings();
        }

        // Popuplate the bundle passed to the service setup call with content specific parameters.
        connectionBundle.putInt(
                ContentChildProcessConstants.EXTRA_CPU_COUNT, CpuFeatures.getCount());
        connectionBundle.putLong(
                ContentChildProcessConstants.EXTRA_CPU_FEATURES, CpuFeatures.getMask());
        connectionBundle.putBundle(
                Linker.EXTRA_LINKER_SHARED_RELROS, Linker.getInstance().getSharedRelros());
    }

    private void onConnectionEstablished(ChildProcessConnection connection) {
        if (mUseBindingManager) {
            getBindingManager().addNewConnection(connection.getPid(), connection);
        }
    }

    // Called when a connection has been disconnected. Only invoked if onConnectionEstablished was
    // called, meaning the connection was already established.
    private void onConnectionLost(ChildProcessConnection connection, int pid) {
        if (mUseBindingManager) {
            getBindingManager().removeConnection(pid);
        }
    }

    private IBinder getIBinderCallback() {
        return mIBinderCallback;
    }

    // Testing only related methods.
    @VisibleForTesting
    public static ChildProcessLauncherHelper createAndStartForTesting(
            ChildProcessCreationParams creationParams, String[] commandLine,
            FileDescriptorInfo[] filesToBeMapped, boolean useStrongBinding, boolean sandboxed,
            IBinder binderCallback, boolean doSetupConnection) {
        ChildProcessLauncherHelper launcherHelper =
                new ChildProcessLauncherHelper(0L, creationParams, commandLine, filesToBeMapped,
                        useStrongBinding, sandboxed, binderCallback, doSetupConnection);
        launcherHelper.start();
        return launcherHelper;
    }

    @VisibleForTesting
    public static ChildProcessLauncherHelper getLauncherForPid(int pid) {
        return sLauncherByPid.get(pid);
    }

    /** @return the count of services set-up and working. */
    @VisibleForTesting
    static int getConnectedServicesCountForTesting() {
        int count = sPrivilegedChildConnectionAllocator == null
                ? 0
                : sPrivilegedChildConnectionAllocator.allocatedConnectionsCountForTesting();
        return count + getConnectedSandboxedServicesCountForTesting(null /* packageName */);
    }

    @VisibleForTesting
    public static int getConnectedSandboxedServicesCountForTesting(String packageName) {
        int count = 0;
        for (ChildConnectionAllocator allocator : sSandboxedChildConnectionAllocatorMap.values()) {
            if (packageName == null || packageName.equals(allocator.getPackageName())) {
                count += allocator.allocatedConnectionsCountForTesting();
            }
        }
        return count;
    }

    @VisibleForTesting
    static boolean hasSandboxedConnectionAllocatorForPackage(String packageName) {
        return sSandboxedChildConnectionAllocatorMap.containsKey(packageName);
    }

    @VisibleForTesting
    ChildProcessConnection getConnection() {
        return mConnection;
    }

    @VisibleForTesting
    public ChildProcessConnection getChildProcessConnection() {
        return mConnection;
    }

    @VisibleForTesting
    public ChildConnectionAllocator getChildConnectionAllocatorForTesting() {
        return mConnectionAllocator;
    }

    @VisibleForTesting
    public static ChildProcessConnection getWarmUpConnectionForTesting() {
        return sSpareSandboxedConnection == null ? null : sSpareSandboxedConnection.getConnection();
    }

    @VisibleForTesting
    public static boolean crashProcessForTesting(int pid) {
        if (sLauncherByPid.get(pid) == null) return false;

        ChildProcessConnection connection = sLauncherByPid.get(pid).mConnection;
        if (connection == null) return false;

        try {
            connection.crashServiceForTesting();
            return true;
        } catch (RemoteException ex) {
            return false;
        }
    }
}
