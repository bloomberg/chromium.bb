// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.text.TextUtils;

import org.chromium.base.CpuFeatures;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.Linker;
import org.chromium.base.process_launcher.ChildProcessCreationParams;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.content.app.ChromiumLinkerParams;
import org.chromium.content.app.SandboxedProcessService;
import org.chromium.content.common.ContentSwitches;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * This class provides the method to start/stop ChildProcess called by native.
 *
 * Note about threading. The threading here is complicated and not well documented.
 * Code can run on these threads: UI, Launcher, async thread pool, binder, and one-off
 * background threads.
 */
public class ChildProcessLauncher {
    private static final String TAG = "ChildProcLauncher";

    private static final String NUM_SANDBOXED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_SANDBOXED_SERVICES";
    private static final String SANDBOXED_SERVICES_NAME_KEY =
            "org.chromium.content.browser.SANDBOXED_SERVICES_NAME";
    private static final String NUM_PRIVILEGED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_PRIVILEGED_SERVICES";
    private static final String PRIVILEGED_SERVICES_NAME_KEY =
            "org.chromium.content.browser.PRIVILEGED_SERVICES_NAME";

    /**
     * Implemented by ChildProcessLauncherHelper.
     */
    public interface LaunchCallback {
        void onChildProcessStarted(BaseChildProcessConnection connection);
    }

    private static final boolean SPARE_CONNECTION_ALWAYS_IN_FOREGROUND = false;

    // Map from package name to ChildConnectionAllocator.
    private static final Map<String, ChildConnectionAllocator>
            sSandboxedChildConnectionAllocatorMap = new HashMap<>();

    // Map from a connection to its ChildConnectionAllocator.
    private static final Map<BaseChildProcessConnection, ChildConnectionAllocator>
            sConnectionsToAllocatorMap = new HashMap<>();

    // Allocator used for non-sandboxed services.
    private static ChildConnectionAllocator sPrivilegedChildConnectionAllocator;

    // Used by test to override the default sandboxed service allocator settings.
    private static BaseChildProcessConnection.Factory sSandboxedServiceFactoryForTesting;
    private static int sSandboxedServicesCountForTesting = -1;
    private static String sSandboxedServicesNameForTesting;

    @SuppressFBWarnings("LI_LAZY_INIT_STATIC") // Method is single thread.
    public static ChildConnectionAllocator getConnectionAllocator(
            Context context, String packageName, boolean sandboxed) {
        assert LauncherThread.runningOnLauncherThread();
        if (!sandboxed) {
            if (sPrivilegedChildConnectionAllocator == null) {
                sPrivilegedChildConnectionAllocator = ChildConnectionAllocator.create(context,
                        ImportantChildProcessConnection.FACTORY, packageName,
                        PRIVILEGED_SERVICES_NAME_KEY, NUM_PRIVILEGED_SERVICES_KEY);
            }
            return sPrivilegedChildConnectionAllocator;
        }

        if (!sSandboxedChildConnectionAllocatorMap.containsKey(packageName)) {
            Log.w(TAG,
                    "Create a new ChildConnectionAllocator with package name = %s,"
                            + " inSandbox = true",
                    packageName);
            ChildConnectionAllocator connectionAllocator = null;
            if (sSandboxedServicesCountForTesting != -1) {
                // Testing case where allocator settings are overriden.
                BaseChildProcessConnection.Factory factory =
                        sSandboxedServiceFactoryForTesting == null
                        ? ManagedChildProcessConnection.FACTORY
                        : sSandboxedServiceFactoryForTesting;
                String serviceName = !TextUtils.isEmpty(sSandboxedServicesNameForTesting)
                        ? sSandboxedServicesNameForTesting
                        : SandboxedProcessService.class.getName();
                connectionAllocator = ChildConnectionAllocator.createForTest(
                        factory, packageName, serviceName, sSandboxedServicesCountForTesting);
            } else {
                connectionAllocator = ChildConnectionAllocator.create(context,
                        ManagedChildProcessConnection.FACTORY, packageName,
                        SANDBOXED_SERVICES_NAME_KEY, NUM_SANDBOXED_SERVICES_KEY);
            }
            sSandboxedChildConnectionAllocatorMap.put(packageName, connectionAllocator);
        }
        return sSandboxedChildConnectionAllocatorMap.get(packageName);
        // TODO(pkotwicz|hanxi): Figure out when old allocators should be removed from
        // {@code sSandboxedChildConnectionAllocatorMap}.
    }

    @VisibleForTesting
    static BaseChildProcessConnection allocateConnection(
            ChildSpawnData spawnData, Bundle childProcessCommonParams, boolean forWarmUp) {
        assert LauncherThread.runningOnLauncherThread();
        BaseChildProcessConnection.DeathCallback deathCallback =
                new BaseChildProcessConnection.DeathCallback() {
                    @Override
                    public void onChildProcessDied(BaseChildProcessConnection connection) {
                        assert LauncherThread.runningOnLauncherThread();
                        if (connection.getPid() != 0) {
                            stop(connection.getPid());
                        } else {
                            freeConnection(connection);
                        }
                    }
                };
        final ChildProcessCreationParams creationParams = spawnData.getCreationParams();
        final Context context = spawnData.getContext();
        final boolean inSandbox = spawnData.isInSandbox();
        String packageName =
                creationParams != null ? creationParams.getPackageName() : context.getPackageName();
        ChildConnectionAllocator allocator =
                getConnectionAllocator(context, packageName, inSandbox);
        BaseChildProcessConnection connection =
                allocator.allocate(spawnData, deathCallback, childProcessCommonParams, !forWarmUp);
        sConnectionsToAllocatorMap.put(connection, allocator);
        return connection;
    }

    private static boolean sLinkerInitialized;
    private static long sLinkerLoadAddress;

    private static ChromiumLinkerParams getLinkerParamsForNewConnection() {
        if (!sLinkerInitialized) {
            if (Linker.isUsed()) {
                sLinkerLoadAddress = Linker.getInstance().getBaseLoadAddress();
                if (sLinkerLoadAddress == 0) {
                    Log.i(TAG, "Shared RELRO support disabled!");
                }
            }
            sLinkerInitialized = true;
        }

        if (sLinkerLoadAddress == 0) return null;

        // Always wait for the shared RELROs in service processes.
        final boolean waitForSharedRelros = true;
        if (Linker.areTestsEnabled()) {
            Linker linker = Linker.getInstance();
            return new ChromiumLinkerParams(sLinkerLoadAddress,
                                            waitForSharedRelros,
                                            linker.getTestRunnerClassNameForTesting(),
                                            linker.getImplementationForTesting());
        } else {
            return new ChromiumLinkerParams(sLinkerLoadAddress,
                                            waitForSharedRelros);
        }
    }

    @VisibleForTesting
    static Bundle createCommonParamsBundle(ChildProcessCreationParams params) {
        Bundle commonParams = new Bundle();
        commonParams.putParcelable(
                ChildProcessConstants.EXTRA_LINKER_PARAMS, getLinkerParamsForNewConnection());
        final boolean bindToCallerCheck = params == null ? false : params.getBindToCallerCheck();
        commonParams.putBoolean(ChildProcessConstants.EXTRA_BIND_TO_CALLER, bindToCallerCheck);
        return commonParams;
    }

    @VisibleForTesting
    static BaseChildProcessConnection allocateBoundConnection(ChildSpawnData spawnData,
            BaseChildProcessConnection.StartCallback startCallback, boolean forWarmUp) {
        assert LauncherThread.runningOnLauncherThread();
        final Context context = spawnData.getContext();
        final boolean inSandbox = spawnData.isInSandbox();
        final ChildProcessCreationParams creationParams = spawnData.getCreationParams();

        BaseChildProcessConnection connection = allocateConnection(
                spawnData, createCommonParamsBundle(spawnData.getCreationParams()), forWarmUp);
        if (connection != null) {
            connection.start(startCallback);

            String packageName = creationParams != null ? creationParams.getPackageName()
                                                        : context.getPackageName();
            if (inSandbox
                    && !getConnectionAllocator(context, packageName, true /* sandboxed */)
                                .isFreeConnectionAvailable()) {
                // Proactively releases all the moderate bindings once all the sandboxed services
                // are allocated, which will be very likely to have some of them killed by OOM
                // killer.
                getBindingManager().releaseAllModerateBindings();
            }
        }
        return connection;
    }

    private static final long FREE_CONNECTION_DELAY_MILLIS = 1;

    private static void freeConnection(BaseChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();
        if (connection == sSpareSandboxedConnection) clearSpareConnection();

        // Freeing a service should be delayed. This is so that we avoid immediately reusing the
        // freed service (see http://crbug.com/164069): the framework might keep a service process
        // alive when it's been unbound for a short time. If a new connection to the same service
        // is bound at that point, the process is reused and bad things happen (mostly static
        // variables are set when we don't expect them to).
        final BaseChildProcessConnection conn = connection;
        LauncherThread.postDelayed(new Runnable() {
            @Override
            public void run() {
                ChildConnectionAllocator allocator = sConnectionsToAllocatorMap.remove(conn);
                assert allocator != null;
                final ChildSpawnData pendingSpawn = allocator.free(conn);
                if (pendingSpawn != null) {
                    LauncherThread.post(new Runnable() {
                        @Override
                        public void run() {
                            startInternal(pendingSpawn.getContext(), pendingSpawn.getCommandLine(),
                                    pendingSpawn.getFilesToBeMapped(),
                                    pendingSpawn.getLaunchCallback(),
                                    pendingSpawn.getChildProcessCallback(),
                                    pendingSpawn.isInSandbox(), pendingSpawn.isAlwaysInForeground(),
                                    pendingSpawn.getCreationParams());
                        }
                    });
                }
            }
        }, FREE_CONNECTION_DELAY_MILLIS);
    }

    // Map from pid to ChildService connection.
    private static Map<Integer, BaseChildProcessConnection> sServiceMap =
            new ConcurrentHashMap<Integer, BaseChildProcessConnection>();

    // These variables are used for the warm up sandboxed connection.
    // |sSpareSandboxedConnection| is non-null when there is a pending connection. Note it's cleared
    // to null again after the connection is used for a real child process.
    // |sSpareConnectionStarting| is true if ChildProcessConnection.StartCallback has not fired.
    // This is used for a child process allocation to determine if StartCallback should be chained.
    // |sSpareConnectionStartCallback| is the chained StartCallback. This is also used to determine
    // if there is already a child process launch that's used this this connection.
    @SuppressLint("StaticFieldLeak")
    private static BaseChildProcessConnection sSpareSandboxedConnection;
    private static boolean sSpareConnectionStarting;
    private static BaseChildProcessConnection.StartCallback sSpareConnectionStartCallback;

    // Manages oom bindings used to bind chind services. Lazily initialized by getBindingManager()
    private static BindingManager sBindingManager;

    // Whether the main application is currently brought to the foreground.
    private static boolean sApplicationInForeground = true;

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

    @VisibleForTesting
    public static void setBindingManagerForTesting(BindingManager manager) {
        sBindingManager = manager;
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

    /**
     * Returns whether the application is currently in the foreground.
     */
    static boolean isApplicationInForeground() {
        return sApplicationInForeground;
    }

    /**
     * Starts moderate binding management.
     * Note: WebAPKs and non WebAPKs share the same moderate binding pool, so the size of the
     * shared moderate binding pool is always set based on the number of sandboxes processes
     * used by Chrome.
     * @param context Android's context.
     * @param moderateBindingTillBackgrounded true if the BindingManager should add a moderate
     * binding to a render process when it is created and remove the moderate binding when Chrome is
     * sent to the background.
     */
    public static void startModerateBindingManagement(final Context context) {
        assert ThreadUtils.runningOnUiThread();
        LauncherThread.post(new Runnable() {
            @Override
            public void run() {
                ChildConnectionAllocator allocator = getConnectionAllocator(
                        context, context.getPackageName(), true /* sandboxed */);
                getBindingManager().startModerateBindingManagement(
                        context, allocator.getNumberOfServices());
            }
        });
    }

    /**
     * Should be called early in startup so the work needed to spawn the child process can be done
     * in parallel to other startup work. Spare connection is created in sandboxed child process.
     * @param context the application context used for the connection.
     */
    public static void warmUp(final Context context) {
        assert ThreadUtils.runningOnUiThread();
        LauncherThread.post(new Runnable() {
            @Override
            public void run() {
                if (sSpareSandboxedConnection != null) return;
                ChildProcessCreationParams params = ChildProcessCreationParams.getDefault();

                BaseChildProcessConnection.StartCallback startCallback =
                        new BaseChildProcessConnection.StartCallback() {
                            @Override
                            public void onChildStarted() {
                                assert LauncherThread.runningOnLauncherThread();
                                sSpareConnectionStarting = false;
                                if (sSpareConnectionStartCallback != null) {
                                    sSpareConnectionStartCallback.onChildStarted();
                                    clearSpareConnection();
                                }
                                // If there is no chained callback, that means nothing has tried to
                                // use the spare connection yet. It will be cleared when it is used
                                // for an actual child process launch.
                            }

                            @Override
                            public void onChildStartFailed() {
                                assert LauncherThread.runningOnLauncherThread();
                                Log.e(TAG, "Failed to warm up the spare sandbox service");
                                if (sSpareConnectionStartCallback != null) {
                                    sSpareConnectionStartCallback.onChildStartFailed();
                                }
                                clearSpareConnection();
                            }
                        };
                ChildSpawnData spawnData = new ChildSpawnData(context, null /* commandLine */,
                        null /* filesToBeMapped */, null /* launchCallback */,
                        null /* child process callback */, true /* inSandbox */,
                        SPARE_CONNECTION_ALWAYS_IN_FOREGROUND, params);
                sSpareSandboxedConnection =
                        allocateBoundConnection(spawnData, startCallback, true /* forWarmUp */);
                sSpareConnectionStarting = sSpareSandboxedConnection != null;
            }
        });
    }

    private static void clearSpareConnection() {
        assert LauncherThread.runningOnLauncherThread();
        sSpareSandboxedConnection = null;
        sSpareConnectionStarting = false;
        sSpareConnectionStartCallback = null;
    }

    /**
     * Spawns and connects to a child process. It will not block, but will instead callback to
     * {@link #LaunchCallback} on the launcher thread when the connection is established on.
     *
     * @param context Context used to obtain the application context.
     * @param paramId Key used to retrieve ChildProcessCreationParams.
     * @param commandLine The child process command line argv.
     * @param filesToBeMapped File IDs, FDs, offsets, and lengths to pass through.
     * @param launchCallback Callback invoked when the connection is established.
     */
    static void start(Context context, int paramId, final String[] commandLine,
            FileDescriptorInfo[] filesToBeMapped, LaunchCallback launchCallback) {
        assert LauncherThread.runningOnLauncherThread();
        IBinder childProcessCallback = null;
        boolean inSandbox = true;
        boolean alwaysInForeground = false;
        String processType =
                ContentSwitches.getSwitchValue(commandLine, ContentSwitches.SWITCH_PROCESS_TYPE);
        ChildProcessCreationParams params = ChildProcessCreationParams.get(paramId);
        if (paramId != ChildProcessCreationParams.DEFAULT_ID && params == null) {
            throw new RuntimeException("CreationParams id " + paramId + " not found");
        }
        if (!ContentSwitches.SWITCH_RENDERER_PROCESS.equals(processType)) {
            if (params != null && !params.getPackageName().equals(context.getPackageName())) {
                // WebViews and WebAPKs have renderer processes running in their applications.
                // When launching these renderer processes, {@link ManagedChildProcessConnection}
                // requires the package name of the application which holds the renderer process.
                // Therefore, the package name in ChildProcessCreationParams could be the package
                // name of WebViews, WebAPKs, or Chrome, depending on the host application.
                // Except renderer process, all other child processes should use Chrome's package
                // name. In WebAPK, ChildProcessCreationParams are initialized with WebAPK's
                // package name. Make a copy of the WebAPK's params, but replace the package with
                // Chrome's package to use when initializing a non-renderer processes.
                // TODO(boliu): Should fold into |paramId|. Investigate why this is needed.
                params = new ChildProcessCreationParams(context.getPackageName(),
                        params.getIsExternalService(), params.getLibraryProcessType(),
                        params.getBindToCallerCheck());
            }
            if (ContentSwitches.SWITCH_GPU_PROCESS.equals(processType)) {
                childProcessCallback = new GpuProcessCallback();
                inSandbox = false;
                alwaysInForeground = true;
            } else {
                // We only support sandboxed utility processes now.
                assert ContentSwitches.SWITCH_UTILITY_PROCESS.equals(processType);
            }
        }

        startInternal(context, commandLine, filesToBeMapped, launchCallback, childProcessCallback,
                inSandbox, alwaysInForeground, params);
    }

    @VisibleForTesting
    public static BaseChildProcessConnection startInternal(final Context context,
            final String[] commandLine, final FileDescriptorInfo[] filesToBeMapped,
            final LaunchCallback launchCallback, final IBinder childProcessCallback,
            final boolean inSandbox, final boolean alwaysInForeground,
            final ChildProcessCreationParams creationParams) {
        assert LauncherThread.runningOnLauncherThread();
        try {
            TraceEvent.begin("ChildProcessLauncher.startInternal");

            BaseChildProcessConnection allocatedConnection = null;
            String packageName = creationParams != null ? creationParams.getPackageName()
                    : context.getPackageName();
            BaseChildProcessConnection.StartCallback startCallback =
                    new BaseChildProcessConnection.StartCallback() {
                        @Override
                        public void onChildStarted() {}

                        @Override
                        public void onChildStartFailed() {
                            assert LauncherThread.runningOnLauncherThread();
                            Log.e(TAG, "BaseChildProcessConnection.start failed, trying again");
                            LauncherThread.post(new Runnable() {
                                @Override
                                public void run() {
                                    // The child process may already be bound to another client
                                    // (this can happen if multi-process WebView is used in more
                                    // than one process), so try starting the process again.
                                    // This connection that failed to start has not been freed,
                                    // so a new bound connection will be allocated.
                                    startInternal(context, commandLine, filesToBeMapped,
                                            launchCallback, childProcessCallback, inSandbox,
                                            alwaysInForeground, creationParams);
                                }
                            });
                        }
                    };

            if (inSandbox && sSpareSandboxedConnection != null
                    && sSpareConnectionStartCallback == null
                    && SPARE_CONNECTION_ALWAYS_IN_FOREGROUND == alwaysInForeground
                    && sSpareSandboxedConnection.getPackageName().equals(packageName)
                    // Object identity check for getDefault should be enough. The default is
                    // not supposed to change once set.
                    && creationParams == ChildProcessCreationParams.getDefault()) {
                allocatedConnection = sSpareSandboxedConnection;
                if (sSpareConnectionStarting) {
                    sSpareConnectionStartCallback = startCallback;
                } else {
                    clearSpareConnection();
                }
            }
            if (allocatedConnection == null) {
                ChildSpawnData spawnData = new ChildSpawnData(context, commandLine, filesToBeMapped,
                        launchCallback, childProcessCallback, inSandbox, alwaysInForeground,
                        creationParams);
                allocatedConnection =
                        allocateBoundConnection(spawnData, startCallback, false /* forWarmUp */);
                if (allocatedConnection == null) {
                    return null;
                }
            }

            triggerConnectionSetup(allocatedConnection, commandLine, filesToBeMapped,
                    childProcessCallback, launchCallback);
            return allocatedConnection;
        } finally {
            TraceEvent.end("ChildProcessLauncher.startInternal");
        }
    }

    /**
     * Create the common bundle to be passed to child processes.
     * @param context Application context.
     * @param commandLine Command line params to be passed to the service.
     * @param linkerParams Linker params to start the service.
     */
    protected static Bundle createsServiceBundle(
            String[] commandLine, FileDescriptorInfo[] filesToBeMapped) {
        Bundle bundle = new Bundle();
        bundle.putStringArray(ChildProcessConstants.EXTRA_COMMAND_LINE, commandLine);
        bundle.putParcelableArray(ChildProcessConstants.EXTRA_FILES, filesToBeMapped);
        bundle.putInt(ChildProcessConstants.EXTRA_CPU_COUNT, CpuFeatures.getCount());
        bundle.putLong(ChildProcessConstants.EXTRA_CPU_FEATURES, CpuFeatures.getMask());
        bundle.putBundle(Linker.EXTRA_LINKER_SHARED_RELROS, Linker.getInstance().getSharedRelros());
        return bundle;
    }

    @VisibleForTesting
    static void triggerConnectionSetup(final BaseChildProcessConnection connection,
            String[] commandLine, FileDescriptorInfo[] filesToBeMapped,
            final IBinder childProcessCallback, final LaunchCallback launchCallback) {
        assert LauncherThread.runningOnLauncherThread();
        Log.d(TAG, "Setting up connection to process, connection name=%s",
                connection.getServiceName());
        BaseChildProcessConnection.ConnectionCallback connectionCallback =
                new BaseChildProcessConnection.ConnectionCallback() {
                    @Override
                    public void onConnected(BaseChildProcessConnection connection) {
                        assert LauncherThread.runningOnLauncherThread();
                        if (connection != null) {
                            int pid = connection.getPid();
                            Log.d(TAG, "on connect callback, pid=%d", pid);
                            if (connection instanceof ManagedChildProcessConnection) {
                                getBindingManager().addNewConnection(
                                        pid, (ManagedChildProcessConnection) connection);
                            }
                            sServiceMap.put(pid, connection);
                        }
                        // If the connection fails and pid == 0, the Java-side cleanup was already
                        // handled by DeathCallback. We still have to call back to native for
                        // cleanup there.
                        if (launchCallback != null) { // Will be null in Java instrumentation tests.
                            launchCallback.onChildProcessStarted(connection);
                        }
                    }
                };

        connection.setupConnection(
                commandLine, filesToBeMapped, childProcessCallback, connectionCallback);
    }

    /**
     * Terminates a child process. This may be called from any thread.
     *
     * @param pid The pid (process handle) of the service connection obtained from {@link #start}.
     */
    static void stop(int pid) {
        assert LauncherThread.runningOnLauncherThread();
        Log.d(TAG, "stopping child connection: pid=%d", pid);
        BaseChildProcessConnection connection = sServiceMap.remove(pid);
        if (connection == null) {
            // Can happen for single process.
            return;
        }
        getBindingManager().removeConnection(pid);
        connection.stop();
        freeConnection(connection);
    }

    public static int getNumberOfSandboxedServices(Context context, String packageName) {
        assert ThreadUtils.runningOnUiThread();
        if (sSandboxedServicesCountForTesting != -1) {
            return sSandboxedServicesCountForTesting;
        }
        return ChildConnectionAllocator.getNumberOfServices(
                context, packageName, NUM_SANDBOXED_SERVICES_KEY);
    }

    /** @return the count of services set up and working */
    @VisibleForTesting
    static int connectedServicesCountForTesting() {
        return sServiceMap.size();
    }

    @VisibleForTesting
    public static void setSandboxServicesSettingsForTesting(
            BaseChildProcessConnection.Factory factory, int serviceCount, String serviceName) {
        sSandboxedServiceFactoryForTesting = factory;
        sSandboxedServicesCountForTesting = serviceCount;
        sSandboxedServicesNameForTesting = serviceName;
    }

    /**
     * Kills the child process for testing.
     * @return true iff the process was killed as expected
     */
    @VisibleForTesting
    public static boolean crashProcessForTesting(int pid) {
        if (sServiceMap.get(pid) == null) return false;

        try {
            ((ManagedChildProcessConnection) sServiceMap.get(pid)).crashServiceForTesting();
        } catch (RemoteException ex) {
            return false;
        }

        return true;
    }
}
