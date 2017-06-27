// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.os.Bundle;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.process_launcher.ChildProcessCreationParams;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Unit tests for the SpareChildConnection class. */
@Config(manifest = Config.NONE)
@RunWith(LocalRobolectricTestRunner.class)
public class SpareChildConnectionTest {
    @Mock
    private ChildProcessConnection.ServiceCallback mServiceCallback;

    // A connection allocator not used to create connections.
    private final ChildConnectionAllocator mWrongConnectionAllocator =
            ChildConnectionAllocator.createForTest(null /* creationParams */, "org.chromium.test",
                    "TestServiceName", 3 /* serviceCount */, false /* bindAsExternalService */,
                    false /* useStrongBinding */);

    // The allocator used to allocate the actual connection.
    private ChildConnectionAllocator mConnectionAllocator;

    private static class TestConnectionFactory
            implements ChildConnectionAllocator.ConnectionFactory {
        private TestChildProcessConnection mConnection;

        @Override
        public ChildProcessConnection createConnection(Context context, ComponentName serviceName,
                boolean bindAsExternalService, Bundle serviceBundle,
                ChildProcessCreationParams creationParams) {
            // We expect to create only one connection in these tests.
            assert mConnection == null;
            mConnection = new TestChildProcessConnection(
                    serviceName, bindAsExternalService, serviceBundle, creationParams);
            return mConnection;
        }

        public void simulateConnectionBindingSuccessfully() {
            mConnection.getServiceCallback().onChildStarted();
        }

        public void simulateConnectionFailingToBind() {
            mConnection.getServiceCallback().onChildStartFailed();
        }

        public void simulateConnectionDied() {
            mConnection.getServiceCallback().onChildProcessDied(mConnection);
        }
    };

    private final TestConnectionFactory mTestConnectionFactory = new TestConnectionFactory();

    // For some reason creating ChildProcessCreationParams from a static context makes the launcher
    // unhappy. (some Dalvik native library is not found when initializing a SparseArray)
    private final ChildProcessCreationParams mCreationParams = new ChildProcessCreationParams(
            "org.chromium.test,.spare_connection", true /* isExternalService */,
            0 /* libraryProcessType */, true /* bindToCallerCheck */);

    private SpareChildConnection mSpareConnection;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // The tests run on only one thread. Pretend that is the launcher thread so LauncherThread
        // asserts are not triggered.
        LauncherThread.setCurrentThreadAsLauncherThread();

        mConnectionAllocator = ChildConnectionAllocator.createForTest(mCreationParams,
                mCreationParams.getPackageNameForSandboxedService(), "TestServiceName",
                5 /* serviceCount */, false /* bindAsExternalService */,
                false /* useStrongBinding */);
        mConnectionAllocator.setConnectionFactoryForTesting(mTestConnectionFactory);
        mSpareConnection = new SpareChildConnection(
                null /* context */, mConnectionAllocator, null /* serviceBundle */);
    }

    @After
    public void tearDown() {
        LauncherThread.setLauncherThreadAsLauncherThread();
    }

    /** Test creation and retrieval of connection. */
    @Test
    @Feature({"ProcessManagement"})
    public void testCreateAndGet() {
        // Tests retrieving the connection with the wrong allocator.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mWrongConnectionAllocator, mServiceCallback);
        assertNull(connection);

        // And with the right one.
        connection = mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);

        // The connection has been used, subsequent calls should return null.
        connection = mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNull(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackNotCalledWhenNoConnection() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();

        // Retrieve the wrong connection, no callback should be fired.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mWrongConnectionAllocator, mServiceCallback);
        assertNull(connection);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed();
        verify(mServiceCallback, times(0)).onChildProcessDied(any());
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackCalledConnectionReady() {
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();

        // Now retrieve the connection, the callback should be invoked.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackCalledConnectionNotReady() {
        // Retrieve the connection before it's bound.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        // No callbacks are called.
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed();

        // Simulate the connection getting bound, it should trigger the callback.
        mTestConnectionFactory.simulateConnectionBindingSuccessfully();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testUnretrievedConnectionFailsToBind() {
        mTestConnectionFactory.simulateConnectionFailingToBind();

        // We should not have a spare connection.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNull(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRetrievedConnectionFailsToBind() {
        // Retrieve the spare connection before it's bound.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);

        mTestConnectionFactory.simulateConnectionFailingToBind();

        // We should get a failure callback.
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(1)).onChildStartFailed();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRetrievedConnectionStops() {
        // Retrieve the spare connection before it's bound.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNotNull(connection);

        mTestConnectionFactory.simulateConnectionDied();

        // We should get a failure callback.
        verify(mServiceCallback, times(0)).onChildStarted();
        verify(mServiceCallback, times(0)).onChildStartFailed();
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testConnectionFreeing() {
        // Simulate the connection dying.
        mTestConnectionFactory.simulateConnectionDied();

        // Connection should be gone.
        ChildProcessConnection connection =
                mSpareConnection.getConnection(mConnectionAllocator, mServiceCallback);
        assertNull(connection);
    }
}
