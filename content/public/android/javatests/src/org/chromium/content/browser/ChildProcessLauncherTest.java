// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
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

import java.util.Map;
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
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChildProcessLauncherHelper.initLinker();
            }
        });
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
        final ChildProcessConnection connection = startConnection();
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
        final ChildProcessConnection connection = startConnection();
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
     * Tests that connection requests get queued when no slot is availabe and created once a slot
     * frees up .
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    @ChildProcessAllocatorSettings(sandboxedServiceCount = 1)
    public void testPendingSpawnQueue() throws RemoteException {
        final Context appContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Assert.assertEquals(0, allocatedChromeSandboxedConnectionsCount());

        ChildProcessCreationParams creationParams =
                getDefaultChildProcessCreationParams(appContext.getPackageName());

        final ChildProcessLauncherHelper connectionLauncher1 =
                ChildProcessLauncherTestUtils.startForTesting(appContext, true /* sandboxed */,
                        false /* alwaysInForeground */, sProcessWaitArguments,
                        new FileDescriptorInfo[0], creationParams);
        Assert.assertTrue(ChildProcessLauncherTestUtils.hasConnection(connectionLauncher1));

        final ChildProcessLauncherHelper connectionLauncher2 =
                ChildProcessLauncherTestUtils.startForTesting(appContext, true /* sandboxed */,
                        false /* alwaysInForeground */, sProcessWaitArguments,
                        new FileDescriptorInfo[0], creationParams);
        Assert.assertFalse(ChildProcessLauncherTestUtils.hasConnection(connectionLauncher2));

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(true, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return ChildProcessLauncherTestUtils.getConnection(connectionLauncher1) != null;
            }
        }));

        final ChildProcessConnection connection1 =
                ChildProcessLauncherTestUtils.getConnection(connectionLauncher1);

        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    connection1.crashServiceForTesting();
                } catch (RemoteException e) {
                }
            }
        });

        CriteriaHelper.pollInstrumentationThread(Criteria.equals(true, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return ChildProcessLauncherTestUtils.getConnection(connectionLauncher2) != null;
            }
        }));

        final ChildProcessConnection connection2 =
                ChildProcessLauncherTestUtils.getConnection(connectionLauncher2);
        Assert.assertNotNull(connection2);
    }

    /**
     * Tests that external APKs and regular use different ChildConnectionAllocators.
     */
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    @ChildProcessAllocatorSettings(
            sandboxedServiceCount = 4, sandboxedServiceName = DEFAULT_SANDBOXED_PROCESS_SERVICE)
    public void testAllocatorForPackage() {
        Context appContext = InstrumentationRegistry.getInstrumentation().getTargetContext();

        ChildConnectionAllocator connectionAllocator = getChildConnectionAllocator(
                appContext, appContext.getPackageName(), true /* sandboxed */);
        ChildConnectionAllocator externalConnectionAllocator = getChildConnectionAllocator(
                appContext, EXTERNAL_APK_PACKAGE_NAME, true /* sandboxed */);
        Assert.assertNotEquals(connectionAllocator, externalConnectionAllocator);
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
        // will fail to start and the ChildProcessLauncher will retry and use the slot 1.
        final ChildProcessCreationParams creationParams = new ChildProcessCreationParams(
                context.getPackageName(), false /* isExternalService */,
                LibraryProcessType.PROCESS_CHILD, true /* bindToCallerCheck */);
        final ChildProcessLauncherHelper launcherHelper =
                ChildProcessLauncherTestUtils.startForTesting(context, true /* sandboxed */,
                        false /* alwaysInForeground */, sProcessWaitArguments,
                        new FileDescriptorInfo[0], creationParams);

        // Retrieve the connection (this waits for the service to connect).
        final ChildProcessConnection retryConn = retrieveConnection(launcherHelper);
        Assert.assertEquals(1, ChildProcessLauncherTestUtils.getConnectionServiceNumber(retryConn));

        final ChildProcessConnection[] sandboxedConnections =
                getSandboxedConnectionArrayForTesting(context, context.getPackageName());

        // Check that only two connections are created.
        for (int i = 0; i < sandboxedConnections.length; ++i) {
            ChildProcessConnection sandboxedConn = sandboxedConnections[i];
            if (i <= 1) {
                Assert.assertNotNull(sandboxedConn);
                Assert.assertNotNull(
                        ChildProcessLauncherTestUtils.getConnectionService(sandboxedConn));
            } else {
                Assert.assertNull(sandboxedConn);
            }
        }

        Assert.assertTrue(retryConn == sandboxedConnections[1]);

        ChildProcessConnection conn = sandboxedConnections[0];
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
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        warmUpOnUiThreadBlocking(context);

        Assert.assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        ChildProcessLauncherHelper launcherHelper = ChildProcessLauncherTestUtils.startForTesting(
                context, true /* sandboxed */, false /* alwaysInForeground */, new String[0],
                new FileDescriptorInfo[0], null);

        final ChildProcessConnection conn = retrieveConnection(launcherHelper);

        Assert.assertEquals(
                1, allocatedChromeSandboxedConnectionsCount()); // Used warmup connection.

        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChildProcessLauncher.stop(ChildProcessLauncherTestUtils.getConnectionPid(conn));
            }
        });

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting for connection to be freed") {
                    @Override
                    public boolean isSatisfied() {
                        return allocatedChromeSandboxedConnectionsCount() == 0;
                    }
                });
    }

    // Tests that the warm-up connection is freed from its allocator if it crashes.
    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testWarmUpProcessCrash() throws RemoteException {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        warmUpOnUiThreadBlocking(context);

        Assert.assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        final ChildProcessConnection warmupConnection = getWarmUpConnection();
        Assert.assertNotNull(warmupConnection);

        waitForConnectionToConnect(warmupConnection);

        warmupConnection.crashServiceForTesting();

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting for connection to be freed") {
                    @Override
                    public boolean isSatisfied() {
                        return allocatedChromeSandboxedConnectionsCount() == 0;
                    }
                });
    }

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testSandboxedAllocatorFreed() {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        ChildProcessLauncherHelper launcherHelper = ChildProcessLauncherTestUtils.startForTesting(
                context, true /* sandboxed */, false /* alwaysInForeground */, new String[0],
                new FileDescriptorInfo[0], null);
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Map<String, ChildConnectionAllocator> allocatorMap =
                        ChildProcessLauncher.getSandboxedAllocatorMapForTesting();
                Assert.assertTrue(allocatorMap.containsKey(context.getPackageName()));
            }
        });

        final ChildProcessConnection conn = retrieveConnection(launcherHelper);
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChildProcessLauncher.stop(ChildProcessLauncherTestUtils.getConnectionPid(conn));
            }
        });

        // Poll until allocator is removed. Need to poll here because actually freeing a connection
        // from allocator is a posted task, rather than a direct call from stop.
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(Boolean.FALSE, new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                                new Callable<Boolean>() {
                                    @Override
                                    public Boolean call() {
                                        Map<String, ChildConnectionAllocator> allocatorMap =
                                                ChildProcessLauncher
                                                        .getSandboxedAllocatorMapForTesting();
                                        return allocatorMap.containsKey(context.getPackageName());
                                    }
                                });
                    }
                }));
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

    @Test
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testAlwaysInForegroundConnection() {
        // Since warmUp only uses default params.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        ChildProcessCreationParams creationParams = new ChildProcessCreationParams(
                context.getPackageName(), false /* isExternalService */,
                LibraryProcessType.PROCESS_CHILD, true /* bindToCallerCheck */);

        for (final boolean alwaysInForeground : new boolean[] {true, false}) {
            ChildProcessLauncherHelper launcherHelper =
                    ChildProcessLauncherTestUtils.startForTesting(context, false /* sandboxed */,
                            alwaysInForeground, sProcessWaitArguments, new FileDescriptorInfo[0],
                            creationParams);
            final ChildProcessConnection connection = retrieveConnection(launcherHelper);
            Assert.assertNotNull(connection);
            ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    Assert.assertEquals(alwaysInForeground, connection.isStrongBindingBound());
                }
            });
        }
    }

    private ChildProcessConnection startConnection() {
        // Allocate a new connection.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        final ChildProcessConnection connection = allocateBoundConnectionForTesting(
                context, getDefaultChildProcessCreationParams(context.getPackageName()));

        // Wait for the service to connect.
        waitForConnectionToConnect(connection);
        return connection;
    }

    private void waitForConnectionToConnect(final ChildProcessConnection connection) {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("The connection wasn't established.") {
                    @Override
                    public boolean isSatisfied() {
                        return connection.isConnected();
                    }
                });
    }

    private ChildProcessConnection startChildService() {
        // Allocate a new connection.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        final ChildProcessConnection connection = allocateBoundConnectionForTesting(
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
        ChildProcessLauncherHelper.createAndStart(0L, paramId,
                new String[] {"--" + ContentSwitches.SWITCH_PROCESS_TYPE + "="
                        + ContentSwitches.SWITCH_RENDERER_PROCESS},
                filesToMap);
    }

    private static ChildConnectionAllocator getChildConnectionAllocator(
            final Context context, final String packageName, final boolean sandboxed) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<ChildConnectionAllocator>() {
                    @Override
                    public ChildConnectionAllocator call() {
                        return ChildProcessLauncher.getConnectionAllocator(context,
                                getDefaultChildProcessCreationParams(packageName), sandboxed);
                    }
                });
    }

    private static ChildProcessConnection allocateBoundConnectionForTesting(
            final Context context, final ChildProcessCreationParams creationParams) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<ChildProcessConnection>() {
                    @Override
                    public ChildProcessConnection call() {
                        return ChildProcessLauncher.allocateSandboxedBoundConnectionForTesting(
                                context, creationParams);
                    }
                });
    }

    private static int allocatedSandboxedConnectionsCountForTesting(
            final Context context, final String packageName) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        ChildProcessCreationParams creationParams = null;
                        if (packageName != null) {
                            // The creationParams is just used for the package name.
                            creationParams = new ChildProcessCreationParams(packageName,
                                    true /* isExternalService */, 0 /* libraryProcessType */,
                                    false /* bindToCallerCheck */);
                        }
                        return ChildProcessLauncher
                                .getConnectionAllocator(
                                        context, creationParams, true /* isSandboxed */)
                                .allocatedConnectionsCountForTesting();
                    }
                });
    }

    private static ChildProcessConnection[] getSandboxedConnectionArrayForTesting(
            final Context context, final String packageName) {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<ChildProcessConnection[]>() {
                    @Override
                    public ChildProcessConnection[] call() {
                        return ChildProcessLauncher
                                .getConnectionAllocator(context,
                                        getDefaultChildProcessCreationParams(packageName),
                                        true /*isSandboxed */)
                                .connectionArrayForTesting();
                    }
                });
    }

    /**
     * Returns the number of Chrome's sandboxed connections.
     */
    private int allocatedChromeSandboxedConnectionsCount() {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        return allocatedSandboxedConnectionsCountForTesting(context, null /* packageName */);
    }

    private static ChildProcessCreationParams getDefaultChildProcessCreationParams(
            String packageName) {
        return packageName == null
                ? null
                : new ChildProcessCreationParams(packageName, false /* isExternalService */,
                          LibraryProcessType.PROCESS_CHILD, false /* bindToCallerCheck */);
    }

    private void triggerConnectionSetup(final ChildProcessConnection connection) {
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Bundle connectionBundle = ChildProcessLauncherHelper.createConnectionBundle(
                        sProcessWaitArguments, new FileDescriptorInfo[0]);
                ChildProcessLauncher.setupConnection(connection, connectionBundle,
                        null /* launchCallback */, null /* childProcessCallback */,
                        true /* addToBindingmanager */);
            }
        });
    }

    private static ChildProcessConnection retrieveConnection(
            final ChildProcessLauncherHelper launcherHelper) {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting for child process to connect") {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncherTestUtils.getConnection(launcherHelper) != null;
                    }
                });

        return ChildProcessLauncherTestUtils.getConnection(launcherHelper);
    }

    private static ChildProcessConnection getWarmUpConnection() {
        return ChildProcessLauncherTestUtils.runOnLauncherAndGetResult(
                new Callable<ChildProcessConnection>() {
                    @Override
                    public ChildProcessConnection call() {
                        return ChildProcessLauncher.getWarmUpConnectionForTesting();
                    }
                });
    }
}
