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
import android.support.test.filters.MediumTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.BaseSwitches;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.common.FileDescriptorInfo;
import org.chromium.content_shell_apk.ChildProcessLauncherTestHelperService;

import java.util.concurrent.Callable;

/**
 * Instrumentation tests for ChildProcessLauncher.
 */
public class ChildProcessLauncherTest extends InstrumentationTestCase {
    // Pseudo command line arguments to instruct the child process to wait until being killed.
    // Allowing the process to continue would lead to a crash when attempting to initialize IPC
    // channels that are not being set up in this test.
    private static final String[] sProcessWaitArguments = {
        "_", "--" + BaseSwitches.RENDERER_WAIT_FOR_JAVA_DEBUGGER };
    private static final String EXTERNAL_APK_PACKAGE_NAME = "org.chromium.external.apk";
    private static final String DEFAULT_SANDBOXED_PROCESS_SERVICE =
            "org.chromium.content.app.SandboxedProcessService";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        LibraryLoader.get(LibraryProcessType.PROCESS_CHILD).ensureInitialized();
    }

    /**
     *  Tests cleanup for a connection that fails to connect in the first place.
     */
    @MediumTest
    @Feature({"ProcessManagement"})
    @CommandLineFlags.Add(ChildProcessLauncher.SWITCH_NUM_SANDBOXED_SERVICES_FOR_TESTING + "=4")
    public void testServiceFailedToBind() {
        assertEquals(0, allocatedChromeSandboxedConnectionsCount());
        assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Try to allocate a connection to service class in incorrect package. We can do that by
        // using the instrumentation context (getContext()) instead of the app context
        // (getTargetContext()).
        Context context = getInstrumentation().getContext();
        ChildProcessLauncher.allocateBoundConnectionForTesting(
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
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceCrashedBeforeSetup() throws RemoteException {
        assertEquals(0, allocatedChromeSandboxedConnectionsCount());
        assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Start and connect to a new service.
        final ChildProcessConnectionImpl connection = startConnection();
        assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        // Verify that the service is not yet set up.
        assertEquals(0, connection.getPid());
        assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Crash the service.
        assertTrue(connection.crashServiceForTesting());

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
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceCrashedAfterSetup() throws RemoteException {
        assertEquals(0, allocatedChromeSandboxedConnectionsCount());

        // Start and connect to a new service.
        final ChildProcessConnectionImpl connection = startConnection();
        assertEquals(1, allocatedChromeSandboxedConnectionsCount());

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
                        return connection.getPid() != 0;
                    }
                });

        // Crash the service.
        assertTrue(connection.crashServiceForTesting());

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
        assertTrue(connection.getPid() != 0);
    }

    /**
     * Tests spawning a pending process from queue.
     */
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testPendingSpawnQueue() throws RemoteException {
        final Context appContext = getInstrumentation().getTargetContext();
        assertEquals(0, allocatedChromeSandboxedConnectionsCount());

        // Start and connect to a new service.
        final ChildProcessConnectionImpl connection = startConnection();
        assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        // Queue up a new spawn request. There is no way to kill the pending connection, leak it
        // until the browser restart.
        final String packageName = appContext.getPackageName();
        final boolean inSandbox = true;
        ChildProcessLauncher.enqueuePendingSpawnForTesting(appContext, sProcessWaitArguments,
                getDefaultChildProcessCreationParams(packageName), inSandbox);
        assertEquals(1, ChildProcessLauncher.pendingSpawnsCountForTesting(appContext, packageName,
                inSandbox));

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
                        return connection.getPid() != 0;
                    }
                });

        // Crash the service.
        assertTrue(connection.crashServiceForTesting());

        // Verify that a new service is started for the pending spawn.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(0, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ChildProcessLauncher.pendingSpawnsCountForTesting(appContext, packageName,
                        inSandbox);
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
    @MediumTest
    @Feature({"ProcessManagement"})
    @CommandLineFlags.Add({ChildProcessLauncher.SWITCH_NUM_SANDBOXED_SERVICES_FOR_TESTING + "=4",
            ChildProcessLauncher.SWITCH_SANDBOXED_SERVICES_NAME_FOR_TESTING + "="
            + DEFAULT_SANDBOXED_PROCESS_SERVICE})
    public void testServiceNumberAllocation() {
        Context appContext = getInstrumentation().getTargetContext();
        assertEquals(0, ChildProcessLauncher.allocatedSandboxedConnectionsCountForTesting(
                                appContext, EXTERNAL_APK_PACKAGE_NAME));
        assertEquals(0, allocatedChromeSandboxedConnectionsCount());

        // Start and connect to a new service of an external APK.
        ChildProcessConnectionImpl externalApkConnection =
                allocateConnection(EXTERNAL_APK_PACKAGE_NAME);
        // Start and connect to a new service for a regular tab.
        ChildProcessConnectionImpl tabConnection = allocateConnection(appContext.getPackageName());

        // Verify that one connection is allocated for an external APK and a regular tab
        // respectively.
        assertEquals(1, ChildProcessLauncher.allocatedSandboxedConnectionsCountForTesting(
                                appContext, EXTERNAL_APK_PACKAGE_NAME));
        assertEquals(1, allocatedChromeSandboxedConnectionsCount());

        // Verify that connections allocated for an external APK and the regular tab are from
        // different ChildConnectionAllocators, since both ChildConnectionAllocators start
        // allocating connections from number 0.
        assertEquals(0, externalApkConnection.getServiceNumber());
        assertEquals(0, tabConnection.getServiceNumber());
    }

    /**
     * Tests that after reaching the maximum allowed connections for an external APK, we can't
     * allocate a new connection to the APK, but we can still allocate a connection for a regular
     * tab.
     */
    @MediumTest
    @Feature({"ProcessManagement"})
    @CommandLineFlags.Add({ChildProcessLauncher.SWITCH_NUM_SANDBOXED_SERVICES_FOR_TESTING + "=1",
            ChildProcessLauncher.SWITCH_SANDBOXED_SERVICES_NAME_FOR_TESTING + "="
            + DEFAULT_SANDBOXED_PROCESS_SERVICE})
    public void testExceedMaximumConnectionNumber() {
        Context appContext = getInstrumentation().getTargetContext();
        assertEquals(0, ChildProcessLauncher.allocatedSandboxedConnectionsCountForTesting(
                                appContext, EXTERNAL_APK_PACKAGE_NAME));

        // Setup a connection for an external APK to reach the maximum allowed connection number.
        ChildProcessConnectionImpl externalApkConnection =
                allocateConnection(EXTERNAL_APK_PACKAGE_NAME);
        assertNotNull(externalApkConnection);

        // Verify that there isn't any connection available for the external APK.
        ChildProcessConnectionImpl exceedNumberExternalApkConnection =
                allocateConnection(EXTERNAL_APK_PACKAGE_NAME);
        assertNull(exceedNumberExternalApkConnection);

        // Verify that we can still allocate connection for a regular tab.
        ChildProcessConnectionImpl tabConnection = allocateConnection(appContext.getPackageName());
        assertNotNull(tabConnection);
    }

    /**
     * Tests binding to the same sandboxed service process from multiple processes in the
     * same package. This uses the ChildProcessLauncherTestHelperService declared in
     * ContentShell.apk as a separate android:process to bind the first (slot 0) service. The
     * instrumentation test then tries to bind the same slot, which fails, so the
     * ChildProcessLauncher retries on a new connection.
     */
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testBindServiceFromMultipleProcesses() throws RemoteException {
        final Context context = getInstrumentation().getTargetContext();

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
        assertTrue(context.bindService(intent, serviceConn, Context.BIND_AUTO_CREATE));

        // Wait for the Helper service to connect.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed to get helper service Messenger") {
                    @Override
                    public boolean isSatisfied() {
                        return serviceConn.mMessenger != null;
                    }
                });

        assertNotNull(serviceConn.mMessenger);

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
        assertNotNull(replyHandler.mMessage);
        assertEquals(ChildProcessLauncherTestHelperService.MSG_BIND_SERVICE_REPLY,
                replyHandler.mMessage.what);
        assertEquals("Connection slot from helper service is not 0", 0, replyHandler.mMessage.arg2);

        final int helperConnPid = replyHandler.mMessage.arg1;
        assertTrue(helperConnPid > 0);

        // Launch a service from this process. Since slot 0 is already bound by the Helper, it
        // will fail to start and the ChildProcessLauncher will retry.
        final ChildProcessConnection conn = ChildProcessLauncher.startForTesting(context,
                sProcessWaitArguments, new FileDescriptorInfo[0],
                getDefaultChildProcessCreationParams(context.getPackageName()));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting for instrumentation-bound service") {
                    @Override
                    public boolean isSatisfied() {
                        return conn.getService() != null;
                    }
                });

        assertEquals(0, conn.getServiceNumber());

        final ChildProcessConnection[] sandboxedConnections =
                ChildProcessLauncher.getSandboxedConnectionArrayForTesting(
                        context.getPackageName());

        // Wait for the retry to succeed.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Failed waiting for both child process services") {
                    @Override
                    public boolean isSatisfied() {
                        boolean allChildrenConnected = true;
                        for (int i = 0; i <= 1; ++i) {
                            ChildProcessConnection conn = sandboxedConnections[i];
                            allChildrenConnected &= conn != null && conn.getService() != null;
                        }
                        return allChildrenConnected;
                    }
                });

        // Check that only two connections are created.
        for (int i = 0; i < sandboxedConnections.length; ++i) {
            ChildProcessConnection sandboxedConn = sandboxedConnections[i];
            if (i <= 1) {
                assertNotNull(sandboxedConn);
                assertNotNull(sandboxedConn.getService());
            } else {
                assertNull(sandboxedConn);
            }
        }

        assertTrue(conn == sandboxedConnections[0]);
        final ChildProcessConnection retryConn = sandboxedConnections[1];

        assertFalse(conn == retryConn);

        assertEquals(0, conn.getServiceNumber());
        assertEquals(0, conn.getPid());
        assertFalse(conn.getService().bindToCaller());

        assertEquals(1, retryConn.getServiceNumber());
        assertTrue(retryConn.getPid() > 0);
        assertTrue(retryConn.getPid() != helperConnPid);
        assertTrue(retryConn.getService().bindToCaller());
    }

    private ChildProcessConnectionImpl startConnection() {
        // Allocate a new connection.
        Context context = getInstrumentation().getTargetContext();
        final ChildProcessConnectionImpl connection =
                (ChildProcessConnectionImpl) ChildProcessLauncher.allocateBoundConnectionForTesting(
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

    /**
     * Returns a new connection if it is allocated. Note this function only allocates a connection
     * but doesn't really start the connection to bind a service. It is for testing whether the
     * connection is allocated properly for different application packages.
     */
    private ChildProcessConnectionImpl allocateConnection(String packageName) {
        // Allocate a new connection.
        Context context = getInstrumentation().getTargetContext();
        return (ChildProcessConnectionImpl) ChildProcessLauncher.allocateConnectionForTesting(
                        context, getDefaultChildProcessCreationParams(packageName));
    }

    /**
     * Returns the number of Chrome's sandboxed connections.
     */
    private int allocatedChromeSandboxedConnectionsCount() {
        Context context = getInstrumentation().getTargetContext();
        return ChildProcessLauncher.allocatedSandboxedConnectionsCountForTesting(
                context, context.getPackageName());
    }

    private ChildProcessCreationParams getDefaultChildProcessCreationParams(String packageName) {
        return new ChildProcessCreationParams(packageName, false /* isExternalService */,
                LibraryProcessType.PROCESS_CHILD);
    }

    private void triggerConnectionSetup(ChildProcessConnectionImpl connection) {
        ChildProcessLauncher.triggerConnectionSetup(connection, sProcessWaitArguments, 1,
                new FileDescriptorInfo[0], ChildProcessLauncher.CALLBACK_FOR_RENDERER_PROCESS, 0);
    }
}
