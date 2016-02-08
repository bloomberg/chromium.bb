// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gcore;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

/**
 * Tests for {@link GoogleApiClientHelper}
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleApiClientHelperTest {
    private GoogleApiClient mMockClient;

    @Before
    public void setUp() {
        mMockClient = mock(GoogleApiClient.class);
    }

    @After
    public void tearDown() {
        ApplicationStatus.destroyForJUnitTests();
        LifecycleHook.destroyInstanceForJUnitTests();
    }

    /** Tests that connection attempts are delayed. */
    @Test
    @Feature({"GCore"})
    public void connectionAttemptDelayTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        ConnectionResult mockResult = mock(ConnectionResult.class);
        when(mockResult.getErrorCode()).thenReturn(ConnectionResult.SERVICE_UPDATING);

        Robolectric.pauseMainLooper();
        helper.onConnectionFailed(mockResult);
        verify(mMockClient, times(0)).connect();
        Robolectric.unPauseMainLooper();

        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).connect();
    }

    /** Tests that the connection handler gives up after a number of connection attempts. */
    @Test
    @Feature({"GCore"})
    public void connectionFailureTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);

        ConnectionResult mockResult = mock(ConnectionResult.class);
        when(mockResult.getErrorCode()).thenReturn(ConnectionResult.DEVELOPER_ERROR);

        helper.onConnectionFailed(mockResult);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();

        // Should not retry on unrecoverable errors
        verify(mMockClient, times(0)).connect();

        // Connection attempts
        when(mockResult.getErrorCode()).thenReturn(ConnectionResult.SERVICE_UPDATING);
        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT; i++) {
            helper.onConnectionFailed(mockResult);
            Robolectric.runUiThreadTasksIncludingDelayedTasks();
        }

        // Should have tried to connect every time.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT)).connect();

        // Try again
        helper.onConnectionFailed(mockResult);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();

        // The connection handler should have given up, no new call.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT)).connect();
    }

    /** Tests that when a connection succeeds, the retry limit is reset. */
    @Test
    @Feature({"GCore"})
    public void connectionAttemptsResetTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        ConnectionResult mockResult = mock(ConnectionResult.class);
        when(mockResult.getErrorCode()).thenReturn(ConnectionResult.SERVICE_UPDATING);

        // Connection attempts
        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT - 1; i++) {
            helper.onConnectionFailed(mockResult);
            Robolectric.runUiThreadTasksIncludingDelayedTasks();
        }

        // Should have tried to connect every time.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT - 1)).connect();

        // Connection successful now
        helper.onConnected(null);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();

        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT; i++) {
            helper.onConnectionFailed(mockResult);
            Robolectric.runUiThreadTasksIncludingDelayedTasks();
        }

        // A success should allow for more connection attempts.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT * 2 - 1)).connect();

        // This should not result in a connection attempt, the limit is still there.
        helper.onConnectionFailed(mockResult);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();

        // The connection handler should have given up, no new call.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT * 2 - 1)).connect();
    }

    @Test
    @Feature({"GCore"})
    public void lifecycleManagementTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Activity mockActivity = mock(Activity.class);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);

        // The helper should have been registered to handle connectivity issues.
        verify(mMockClient, times(1)).registerConnectionCallbacks(helper);
        verify(mMockClient, times(1)).registerConnectionFailedListener(helper);

        // Client was not connected. Coming in the foreground should not change that.
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        verify(mMockClient, times(0)).connect();

        // We now say we are connected
        when(mMockClient.isConnected()).thenReturn(true);

        // Should be disconnected when we go in the background
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).disconnect();

        // Should be reconnected when we come in the foreground
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        verify(mMockClient, times(1)).connect();

        helper.disable();

        // The helper should have been unregistered from handling connectivity issues.
        verify(mMockClient, times(1)).unregisterConnectionCallbacks(helper);
        verify(mMockClient, times(1)).unregisterConnectionFailedListener(helper);

        // Should not be interacted with anymore when we stop managing it.
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        verify(mMockClient, times(1)).disconnect();
    }

    @Test
    @Feature({"GCore"})
    public void lifecycleManagementDelayTest() {
        GoogleApiClientHelper helper = new GoogleApiClientHelper(mMockClient);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        Activity mockActivity = mock(Activity.class);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.CREATED);
        helper.setDisconnectionDelay(5000);

        // We have a connected client
        when(mMockClient.isConnected()).thenReturn(true);

        // Should not be disconnected when we go in the background
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        Robolectric.runUiThreadTasks();
        verify(mMockClient, times(0)).disconnect();

        // Should be disconnected when we wait.
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).disconnect();

        // Should be reconnected when we come in the foreground
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        verify(mMockClient, times(1)).connect();

        // Should not disconnect when we became visible during the delay
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        Robolectric.runUiThreadTasks();
        verify(mMockClient, times(1)).disconnect();
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STARTED);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).disconnect();
    }

}
