// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.RemoteException;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.BaseSwitches;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Instrumentation tests for ChildProcessLauncher.
 * TODO(hanxi): Add tests for assigning {@ChildConnectionAllocator} for different package names
 * when render processes can be run in WebAPKs.
 */
public class ChildProcessLauncherTest extends InstrumentationTestCase {
    // Pseudo command line arguments to instruct the child process to wait until being killed.
    // Allowing the process to continue would lead to a crash when attempting to initialize IPC
    // channels that are not being set up in this test.
    private static final String[] sProcessWaitArguments = {
        "_", "--" + BaseSwitches.RENDERER_WAIT_FOR_JAVA_DEBUGGER };

    /**
     *  Tests cleanup for a connection that fails to connect in the first place.
     */
    @MediumTest
    @Feature({"ProcessManagement"})
    @CommandLineFlags.Add(ChildProcessLauncher.SWITCH_NUM_SANDBOXED_SERVICES_FOR_TESTING + "=4")
    public void testServiceFailedToBind() throws InterruptedException, RemoteException {
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
    public void testServiceCrashedBeforeSetup() throws InterruptedException, RemoteException {
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
    public void testServiceCrashedAfterSetup() throws InterruptedException, RemoteException {
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
    public void testPendingSpawnQueue() throws InterruptedException, RemoteException {
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

    private ChildProcessConnectionImpl startConnection() throws InterruptedException {
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
     * Returns the number of Chrome's sandboxed connections.
     */
    private int allocatedChromeSandboxedConnectionsCount() {
        Context context = getInstrumentation().getTargetContext();
        return ChildProcessLauncher.allocatedSandboxedConnectionsCountForTesting(
                context, context.getPackageName());
    }

    private ChildProcessCreationParams getDefaultChildProcessCreationParams(String packageName) {
        return new ChildProcessCreationParams(packageName, 0,
                LibraryProcessType.PROCESS_CHILD);
    }

    private void triggerConnectionSetup(ChildProcessConnectionImpl connection) {
        ChildProcessLauncher.triggerConnectionSetup(connection, sProcessWaitArguments, 1,
                new FileDescriptorInfo[0], ChildProcessLauncher.CALLBACK_FOR_RENDERER_PROCESS, 0);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        LibraryLoader.get(LibraryProcessType.PROCESS_CHILD)
                .ensureInitialized(getInstrumentation().getTargetContext());
    }
}
