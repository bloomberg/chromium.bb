// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.process_launcher.ChildProcessCreationParams;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.ChildProcessAllocatorSettings;
import org.chromium.content.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_shell_apk.ChildProcessLauncherTestHelperService;
import org.chromium.content_shell_apk.ChildProcessLauncherTestUtils;

import java.util.concurrent.Callable;

/**
 * Instrumentation tests for ChildProcessLauncher.
 */
@RunWith(ContentJUnit4ClassRunner.class)
public class ChildProcessLauncherTest {
    // Pseudo command line arguments to instruct the child process to wait until being killed.
    // Allowing the process to continue would lead to a crash when attempting to initialize IPC
    // channels that are not being set up in this test.
    private static final String[] sProcessWaitArguments = {
        "_", "--" + BaseSwitches.RENDERER_WAIT_FOR_JAVA_DEBUGGER };
    private static final String EXTERNAL_APK_PACKAGE_NAME = "org.chromium.external.apk";
    private static final String DEFAULT_SANDBOXED_PROCESS_SERVICE =
            "org.chromium.content.app.SandboxedProcessService";

    @Before
    public void setUp() throws Exception {
        LibraryLoader.get(LibraryProcessType.PROCESS_CHILD).ensureInitialized();
    }

    /**
     *  Tests cleanup for a connection that fails to connect in the first place.
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    @ChildProcessAllocatorSettings(sandboxedServiceCount = 4)
    public void testServiceFailedToBind() {
        Assert.assertEquals(0, allocatedChromeSandboxedConnectionsCount());
        Assert.assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Try to allocate a connection to service class in incorrect package. We can do that by
        // using the instrumentation context (getContext()) instead of the app context
        // (getTargetContext()).
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        allocateBoundConnectionForTesting(
                context, getDefaultChildProcessCreationParams(context.getPackageName()));

        // Verify that the connection is not considered as allocated.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return allocatedChromeSandboxedConnectionsCount();
            }
        }));

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ChildProcessLauncher.connectedServicesCountForTesting();
            }
        }));
    }

    /**
     * Tests cleanup for a connection that terminates before setup.
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceCrashedBeforeSetup() throws RemoteException {
        Assert.assertEquals(0, allocatedChromeSandboxedConnectionsCount());
        Assert.assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Start and connect to a new service.
        final BaseChildProcessConnection connection = startConnection();
        Assert.assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        // Verify that the service is not yet set up.
        Assert.assertEquals(0, ChildProcessLauncherTestUtils.getConnectionPid(connection));
        Assert.assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Crash the service.
        connection.crashServiceForTesting();

        // Verify that the connection gets cleaned-up.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return allocatedChromeSandboxedConnectionsCount();
            }
        }));

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ChildProcessLauncher.connectedServicesCountForTesting();
            }
        }));
    }

    /**
     * Tests cleanup for a connection that terminates after setup.
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceCrashedAfterSetup() throws RemoteException {
        Assert.assertEquals(0, allocatedChromeSandboxedConnectionsCount());

        // Start and connect to a new service.
        final BaseChildProcessConnection connection = startConnection();
        Assert.assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        // Initiate the connection setup.
        triggerConnectionSetup(connection);

        // Verify that the connection completes the setup.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(1, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ChildProcessLauncher.connectedServicesCountForTesting();
            }
        }));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("The connection failed to get a pid in setup.") {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncherTestUtils.getConnectionPid(connection) != 0;
                    }
                });

        // Crash the service.
        connection.crashServiceForTesting();

        // Verify that the connection gets cleaned-up.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return allocatedChromeSandboxedConnectionsCount();
            }
        }));

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ChildProcessLauncher.connectedServicesCountForTesting();
            }
        }));

        // Verify that the connection pid remains set after termination.
        Assert.assertTrue(ChildProcessLauncherTestUtils.getConnectionPid(connection) != 0);
    }

    /**
     * Tests spawning a pending process from queue.
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testPendingSpawnQueue() throws RemoteException {
        final Context appContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Assert.assertEquals(0, allocatedChromeSandboxedConnectionsCount());

        // Start and connect to a new service.
        final BaseChildProcessConnection connection = startConnection();
        Assert.assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        // Queue up a new spawn request. There is no way to kill the pending connection, leak it
        // until the browser restart.
        final String packageName = appContext.getPackageName();
        final boolean inSandbox = true;
        enqueuePendingSpawnForTesting(appContext, sProcessWaitArguments,
                getDefaultChildProcessCreationParams(packageName), inSandbox);
        Assert.assertEquals(1, pendingSpawnsCountForTesting(appContext, packageName, inSandbox));

        // Initiate the connection setup.
        triggerConnectionSetup(connection);

        // Verify that the connection completes the setup.
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(1, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return ChildProcessLauncher.connectedServicesCountForTesting();
                    }
                }));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("The connection failed to get a pid in setup.") {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncherTestUtils.getConnectionPid(connection) != 0;
                    }
                });

        // Crash the service.
        connection.crashServiceForTesting();

        // Verify that a new service is started for the pending spawn.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return pendingSpawnsCountForTesting(appContext, packageName, inSandbox);
            }
        }));

        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(1, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return allocatedChromeSandboxedConnectionsCount();
                    }
                }));

        // Verify that the connection completes the setup for the pending spawn.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(1, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ChildProcessLauncher.connectedServicesCountForTesting();
            }
        }));
    }

    /**
     * Tests service number of connections for external APKs and regular tabs are assigned properly,
     * i.e. from different ChildConnectionAllocators.
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    @ChildProcessAllocatorSettings(
            sandboxedServiceCount = 4, sandboxedServiceName = DEFAULT_SANDBOXED_PROCESS_SERVICE)
    public void testServiceNumberAllocation() {
        Context appContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Assert.assertEquals(0,
                allocatedSandboxedConnectionsCountForTesting(
                        appContext, EXTERNAL_APK_PACKAGE_NAME));
        Assert.assertEquals(0, allocatedChromeSandboxedConnectionsCount());

        // Start and connect to a new service of an external APK.
        BaseChildProcessConnection externalApkConnection =
                allocateConnection(EXTERNAL_APK_PACKAGE_NAME);
        // Start and connect to a new service for a regular tab.
        BaseChildProcessConnection tabConnection = allocateConnection(appContext.getPackageName());

        // Verify that one connection is allocated for an external APK and a regular tab
        // respectively.
        Assert.assertEquals(1,
                allocatedSandboxedConnectionsCountForTesting(
                        appContext, EXTERNAL_APK_PACKAGE_NAME));
        Assert.assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        // Verify that connections allocated for an external APK and the regular tab are from
        // different ChildConnectionAllocators, since both ChildConnectionAllocators start
        // allocating connections from number 0.
        Assert.assertEquals(
                0, ChildProcessLauncherTestUtils.getConnectionServiceNumber(externalApkConnection));
        Assert.assertEquals(
                0, ChildProcessLauncherTestUtils.getConnectionServiceNumber(tabConnection));
    }

    /**
     * Tests that after reaching the maximum allowed connections for an external APK, we can't
     * allocate a new connection to the APK, but we can still allocate a connection for a regular
     * tab.
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    @ChildProcessAllocatorSettings(
            sandboxedServiceCount = 1, sandboxedServiceName = DEFAULT_SANDBOXED_PROCESS_SERVICE)
    public void testExceedMaximumConnectionNumber() {
        Context appContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Assert.assertEquals(0,
                allocatedSandboxedConnectionsCountForTesting(
                        appContext, EXTERNAL_APK_PACKAGE_NAME));

        // Setup a connection for an external APK to reach the maximum allowed connection number.
        BaseChildProcessConnection externalApkConnection =
                allocateConnection(EXTERNAL_APK_PACKAGE_NAME);
        Assert.assertNotNull(externalApkConnection);

        // Verify that there isn't any connection available for the external APK.
        BaseChildProcessConnection exceedNumberExternalApkConnection =
                allocateConnection(EXTERNAL_APK_PACKAGE_NAME);
        Assert.assertNull(exceedNumberExternalApkConnection);

        // Verify that we can still allocate connection for a regular tab.
        BaseChildProcessConnection tabConnection = allocateConnection(appContext.getPackageName());
        Assert.assertNotNull(tabConnection);
    }

    /**
     * Tests binding to the same sandboxed service process from multiple processes in the
     * same package. This uses the ChildProcessLauncherTestHelperService declared in
     * ContentShell.apk as a separate android:process to bind the first (slot 0) service. The
     * instrumentation test then tries to bind the same slot, which fails, so the
     * ChildProcessLauncher retries on a new connection.
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testBindServiceFromMultipleProcesses() throws RemoteException {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        // Start the Helper service.
        class HelperConnection implements ServiceConnection {
            Messenger mMessenger = null;

            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mMessenger = new Messenger(service);
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        }
        final HelperConnection serviceConn = new HelperConnection();

        Intent intent = new Intent();
        intent.setComponent(new ComponentName(context.getPackageName(),
                context.getPackageName() + ".ChildProcessLauncherTestHelperService"));
        Assert.assertTrue(context.bindService(intent, serviceConn, Context.BIND_AUTO_CREATE));

        // Wait for the Helper service to connect.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed to get helper service Messenger") {
                    @Override
                    public boolean isSatisfied() {
                        return serviceConn.mMessenger != null;
                    }
                });

        Assert.assertNotNull(serviceConn.mMessenger);

        class ReplyHandler implements Handler.Callback {
            Message mMessage;

            @Override
            public boolean handleMessage(Message msg) {
                // Copy the message so its contents outlive this Binder transaction.
                mMessage = Message.obtain();
                mMessage.copyFrom(msg);
                return true;
            }
        }
        final ReplyHandler replyHandler = new ReplyHandler();

        // Send a message to the Helper and wait for the reply. This will cause the slot 0
        // sandboxed service connection to be bound by a different PID (i.e., not this
        // process).
        Message msg = Message.obtain(null, ChildProcessLauncherTestHelperService.MSG_BIND_SERVICE);
        msg.replyTo = new Messenger(new Handler(Looper.getMainLooper(), replyHandler));
        serviceConn.mMessenger.send(msg);

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting for helper service reply") {
                    @Override
                    public boolean isSatisfied() {
                        return replyHandler.mMessage != null;
                    }
                });

        // Verify that the Helper was able to launch the sandboxed service.
        Assert.assertNotNull(replyHandler.mMessage);
        Assert.assertEquals(ChildProcessLauncherTestHelperService.MSG_BIND_SERVICE_REPLY,
                replyHandler.mMessage.what);
        Assert.assertEquals(
                "Connection slot from helper service is not 0", 0, replyHandler.mMessage.arg2);

        final int helperConnPid = replyHandler.mMessage.arg1;
        Assert.assertTrue(helperConnPid > 0);

        // Launch a service from this process. Since slot 0 is already bound by the Helper, it
        // will fail to start and the ChildProcessLauncher will retry.
        final ChildProcessCreationParams creationParams = new ChildProcessCreationParams(
                context.getPackageName(), false /* isExternalService */,
                LibraryProcessType.PROCESS_CHILD, true /* bindToCallerCheck */);
        final BaseChildProcessConnection conn =
                ChildProcessLauncherTestUtils.startInternalForTesting(
                        context, sProcessWaitArguments, new FileDescriptorInfo[0], creationParams);

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting for instrumentation-bound service") {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncherTestUtils.getConnectionService(conn) != null;
                    }
                });

        Assert.assertEquals(0, ChildProcessLauncherTestUtils.getConnectionServiceNumber(conn));

        final BaseChildProcessConnection[] sandboxedConnections =
                getSandboxedConnectionArrayForTesting(context, context.getPackageName());

        // Wait for the retry to succeed.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting for both child process services") {
                    @Override
                    public boolean isSatisfied() {
                        boolean allChildrenConnected = true;
                        for (int i = 0; i <= 1; ++i) {
                            BaseChildProcessConnection conn = sandboxedConnections[i];
                            allChildrenConnected &= conn != null
                                    && ChildProcessLauncherTestUtils.getConnectionService(conn)
                                            != null;
                        }
                        return allChildrenConnected;
                    }
                });

        // Check that only two connections are created.
        for (int i = 0; i < sandboxedConnections.length; ++i) {
            BaseChildProcessConnection sandboxedConn = sandboxedConnections[i];
            if (i <= 1) {
                Assert.assertNotNull(sandboxedConn);
                Assert.assertNotNull(
                        ChildProcessLauncherTestUtils.getConnectionService(sandboxedConn));
            } else {
                Assert.assertNull(sandboxedConn);
            }
        }

        Assert.assertTrue(conn == sandboxedConnections[0]);
        final BaseChildProcessConnection retryConn = sandboxedConnections[1];

        Assert.assertFalse(conn == retryConn);

        Assert.assertEquals(0, ChildProcessLauncherTestUtils.getConnectionServiceNumber(conn));
        Assert.assertEquals(0, ChildProcessLauncherTestUtils.getConnectionPid(conn));
        Assert.assertFalse(ChildProcessLauncherTestUtils.getConnectionService(conn).bindToCaller());

        Assert.assertEquals(1, ChildProcessLauncherTestUtils.getConnectionServiceNumber(retryConn));
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting retry connection to get pid") {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncherTestUtils.getConnectionPid(retryConn) > 0;
                    }
                });
        Assert.assertTrue(
                ChildProcessLauncherTestUtils.getConnectionPid(retryConn) != helperConnPid);
        Assert.assertTrue(
                ChildProcessLauncherTestUtils.getConnectionService(retryConn).bindToCaller());
    }

    private static void warmUpOnUiThreadBlocking(final Context context) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChildProcessLauncher.warmUp(context);
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testWarmUp() {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        warmUpOnUiThreadBlocking(context);
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(1, allocatedChromeSandboxedConnectionsCount());

                final BaseChildProcessConnection conn =
                        ChildProcessLauncherTestUtils.startInternalForTesting(
                                context, new String[0], new FileDescriptorInfo[0], null);
                Assert.assertEquals(
                        1, allocatedChromeSandboxedConnectionsCount()); // Used warmup connection.

                ChildProcessLauncher.stop(ChildProcessLauncherTestUtils.getConnectionPid(conn));
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testCustomCreationParamDoesNotReuseWarmupConnection() {
        // Since warmUp only uses default params.
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        // Check uses object identity, having the params match exactly is fine.
        ChildProcessCreationParams.registerDefault(
                getDefaultChildProcessCreationParams(context.getPackageName()));
        final int paramId = ChildProcessCreationParams.register(
                getDefaultChildProcessCreationParams(context.getPackageName()));

        warmUpOnUiThreadBlocking(context);
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(1, allocatedChromeSandboxedConnectionsCount());

                startRendererProcess(context, paramId, new FileDescriptorInfo[0]);
                Assert.assertEquals(
                        2, allocatedChromeSandboxedConnectionsCount()); // Warmup not used.

                startRendererProcess(
                        context, ChildProcessCreationParams.DEFAULT_ID, new FileDescriptorInfo[0]);
                Assert.assertEquals(2, allocatedChromeSandboxedConnectionsCount()); // Warmup used.

                ChildProcessCreationParams.unregister(paramId);
            }
        });
    }

    private BaseChildProcessConnection startConnection() {
        // Allocate a new connection.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        final BaseChildProcessConnection connection = allocateBoundConnectionForTesting(
                context, getDefaultChildProcessCreationParams(context.getPackageName()));

        // Wait for the service to connect.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("The connection wasn't established.") {
                    @Override
                    public boolean isSatisfied() {
                        return connection.isConnected();
                    }
                });
        return connection;
    }

    private static void startRendererProcess(
            Context context, int paramId, FileDescriptorInfo[] filesToMap) {
        assert LauncherThread.runningOnLauncherThread();
        ChildProcessLauncher.start(context, paramId,
                new String[] {"--" + ContentSwitches.SWITCH_PROCESS_TYPE + "="
                        + ContentSwitches.SWITCH_RENDERER_PROCESS},
                filesToMap, null /* launchCallback */);
    }

    private static BaseChildProcessConnection allocateBoundConnectionForTesting(
            final Context context, final ChildProcessCreationParams creationParams) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<BaseChildProcessConnection>() {
                    @Override
                    public BaseChildProcessConnection call() {
                        return ChildProcessLauncher.allocateBoundConnection(
                                new ChildSpawnData(context, null /* commandLine */,
                                        null /* filesToBeMapped */, null /* LaunchCallback */,
                                        null /* childProcessCallback */, true /* inSandbox */,
                                        false /* alwaysInForeground */, creationParams),
                                null /* startCallback */, false /* forWarmUp */);
                    }
                });
    }

    /**
     * Returns a new connection if it is allocated. Note this function only allocates a connection
     * but doesn't really start the connection to bind a service. It is for testing whether the
     * connection is allocated properly for different application packages.
     */
    private BaseChildProcessConnection allocateConnection(final String packageName) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<BaseChildProcessConnection>() {
                    @Override
                    public BaseChildProcessConnection call() {
                        // Allocate a new connection.
                        Context context = InstrumentationRegistry.getTargetContext();
                        ChildProcessCreationParams creationParams =
                                getDefaultChildProcessCreationParams(packageName);
                        return ChildProcessLauncher.allocateConnection(
                                new ChildSpawnData(context, null /* commandLine */,
                                        null /* filesToBeMapped */, null /* launchCallback */,
                                        null /* childProcessCallback */, true /* inSandbox */,
                                        false /* alwaysInForeground */, creationParams),
                                ChildProcessLauncher.createCommonParamsBundle(creationParams),
                                false /* forWarmUp */);
                    }
                });
    }

    private static void enqueuePendingSpawnForTesting(final Context context,
            final String[] commandLine, final ChildProcessCreationParams creationParams,
            final boolean inSandbox) {
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                String packageName = creationParams != null ? creationParams.getPackageName()
                                                            : context.getPackageName();
                ChildConnectionAllocator allocator = ChildProcessLauncher.getConnectionAllocator(
                        context, packageName, inSandbox);
                allocator.enqueuePendingQueueForTesting(new ChildSpawnData(context, commandLine,
                        new FileDescriptorInfo[0], null /* launchCallback */,
                        null /* childProcessCallback */, true /* inSandbox */,
                        false /* alwaysInForeground */, creationParams));
            }
        });
    }

    private static int allocatedSandboxedConnectionsCountForTesting(
            final Context context, final String packageName) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return ChildProcessLauncher
                                .getConnectionAllocator(context, packageName, true /*isSandboxed */)
                                .allocatedConnectionsCountForTesting();
                    }
                });
    }

    private static BaseChildProcessConnection[] getSandboxedConnectionArrayForTesting(
            final Context context, final String packageName) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<BaseChildProcessConnection[]>() {
                    @Override
                    public BaseChildProcessConnection[] call() {
                        return ChildProcessLauncher
                                .getConnectionAllocator(context, packageName, true /*isSandboxed */)
                                .connectionArrayForTesting();
                    }
                });
    }

    private static int pendingSpawnsCountForTesting(
            final Context context, final String packageName, final boolean inSandbox) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return ChildProcessLauncher
                                .getConnectionAllocator(context, packageName, inSandbox)
                                .pendingSpawnsCountForTesting();
                    }
                });
    }

    /**
     * Returns the number of Chrome's sandboxed connections.
     */
    private int allocatedChromeSandboxedConnectionsCount() {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        return allocatedSandboxedConnectionsCountForTesting(context, context.getPackageName());
    }

    private ChildProcessCreationParams getDefaultChildProcessCreationParams(String packageName) {
        return new ChildProcessCreationParams(packageName, false /* isExternalService */,
                LibraryProcessType.PROCESS_CHILD, false /* bindToCallerCheck */);
    }

    private void triggerConnectionSetup(final BaseChildProcessConnection connection) {
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChildProcessLauncher.triggerConnectionSetup(connection, sProcessWaitArguments,
                        new FileDescriptorInfo[0], null /* launchCallback */,
                        null /* childProcessCallback */);
            }
        });
    }
}
