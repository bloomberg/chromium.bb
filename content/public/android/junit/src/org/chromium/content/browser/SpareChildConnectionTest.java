// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
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
    private ChildProcessConnection.StartCallback mStartCallback;

    static class TestConnectionFactory implements SpareChildConnection.ConnectionFactory {
        private ChildProcessConnection mConnection;
        private ChildProcessConnection.DeathCallback mDeathCallback;
        private ChildProcessConnection.StartCallback mStartCallback;

        @Override
        public ChildProcessConnection allocateBoundConnection(Context context,
                ChildProcessCreationParams creationParams,
                ChildProcessConnection.StartCallback startCallback,
                ChildProcessConnection.DeathCallback deathCallback) {
            mConnection = mock(ChildProcessConnection.class);
            mStartCallback = startCallback;
            mDeathCallback = deathCallback;
            return mConnection;
        }

        public void simulateConnectionBindingSuccessfully() {
            mStartCallback.onChildStarted();
        }

        public void simulateConnectionFailingToBind() {
            mStartCallback.onChildStartFailed();
        }

        public void simulateConnectionDeath() {
            mDeathCallback.onChildProcessDied(mConnection);
        }
    }

    private final TestConnectionFactory mTestConnectionfactory = new TestConnectionFactory();

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

        mSpareConnection = new SpareChildConnection(
                null /* context */, mTestConnectionfactory, mCreationParams, true /* sandboxed */);
    }

    @After
    public void tearDown() {
        LauncherThread.setLauncherThreadAsLauncherThread();
    }

    /** Test creation and retrieval of connection. */
    @Test
    @Feature({"ProcessManagement"})
    public void testCreateAndGet() {
        // Tests retrieving the connection with the wrong parameters.
        ChildProcessConnection connection = mSpareConnection.getConnection(
                null /* creationParams */, true /* sandboxed */, mStartCallback);
        assertNull(connection);
        connection = mSpareConnection.getConnection(
                mCreationParams, false /* sandboxed */, mStartCallback);
        assertNull(connection);

        // And with the right one.
        connection = mSpareConnection.getConnection(
                mCreationParams, true /* sandboxed */, mStartCallback);
        assertNotNull(connection);

        // The connection has been used, subsequent calls should return null.
        connection = mSpareConnection.getConnection(
                mCreationParams, true /* sandboxed */, mStartCallback);
        assertNull(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackNotCalledWhenNoConnection() {
        mTestConnectionfactory.simulateConnectionBindingSuccessfully();

        // Retrieve the wrong connection, no callback should be fired.
        ChildProcessConnection connection = mSpareConnection.getConnection(
                null /* creationParams */, true /* sandboxed */, mStartCallback);
        assertNull(connection);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mStartCallback, times(0)).onChildStarted();
        verify(mStartCallback, times(0)).onChildStartFailed();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackCalledConnectionReady() {
        mTestConnectionfactory.simulateConnectionBindingSuccessfully();

        // Now retrieve the connection, the callback should be invoked.
        ChildProcessConnection connection = mSpareConnection.getConnection(
                mCreationParams, true /* sandboxed */, mStartCallback);
        assertNotNull(connection);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mStartCallback, times(1)).onChildStarted();
        verify(mStartCallback, times(0)).onChildStartFailed();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testCallbackCalledConnectionNotReady() {
        // Retrieve the connection before it's bound.
        ChildProcessConnection connection = mSpareConnection.getConnection(
                mCreationParams, true /* sandboxed */, mStartCallback);
        assertNotNull(connection);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        // No callbacks are called.
        verify(mStartCallback, times(0)).onChildStarted();
        verify(mStartCallback, times(0)).onChildStartFailed();

        // Simulate the connection getting bound, it should trigger the callback.
        mTestConnectionfactory.simulateConnectionBindingSuccessfully();
        verify(mStartCallback, times(1)).onChildStarted();
        verify(mStartCallback, times(0)).onChildStartFailed();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testUnretrievedConnectionFailsToBind() {
        mTestConnectionfactory.simulateConnectionFailingToBind();

        // We should not have a spare connection.
        ChildProcessConnection connection = mSpareConnection.getConnection(
                mCreationParams, true /* sandboxed */, mStartCallback);
        assertNull(connection);
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testRetrievedConnectionFailsToBind() {
        // Retrieve the spare connection before it's bound.
        ChildProcessConnection connection = mSpareConnection.getConnection(
                mCreationParams, true /* sandboxed */, mStartCallback);
        assertNotNull(connection);

        mTestConnectionfactory.simulateConnectionFailingToBind();

        // We should get a failure callback.
        verify(mStartCallback, times(0)).onChildStarted();
        verify(mStartCallback, times(1)).onChildStartFailed();
    }

    @Test
    @Feature({"ProcessManagement"})
    public void testConnectionFreeing() {
        // Simulate the connection dying.
        mTestConnectionfactory.simulateConnectionDeath();

        // Connection should be gone.
        ChildProcessConnection connection = mSpareConnection.getConnection(
                mCreationParams, true /* sandboxed */, mStartCallback);
        assertNull(connection);
    }
}
