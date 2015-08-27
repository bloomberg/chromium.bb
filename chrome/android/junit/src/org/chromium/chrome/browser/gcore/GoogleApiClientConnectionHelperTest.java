// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gcore;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.google.android.gms.common.api.GoogleApiClient;

import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

/**
 * Tests for {@link GoogleApiClientConnectionHelper}
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoogleApiClientConnectionHelperTest {
    private GoogleApiClient mMockClient;

    @Before
    public void setUp() {
        mMockClient = mock(GoogleApiClient.class);
    }

    /** Tests that connection attempts are delayed. */
    @Test
    @Feature({"GCore"})
    public void connectionAttemptDelayTest() {
        GoogleApiClientConnectionHelper helper = new GoogleApiClientConnectionHelper(mMockClient);

        Robolectric.pauseMainLooper();
        helper.onConnectionFailed(null);
        verify(mMockClient, times(0)).connect();
        Robolectric.unPauseMainLooper();

        Robolectric.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockClient, times(1)).connect();
    }

    /** Tests that the connection handler gives up after a number of connection attempts. */
    @Test
    @Feature({"GCore"})
    public void connectionFailureTest() {
        GoogleApiClientConnectionHelper helper = new GoogleApiClientConnectionHelper(mMockClient);

        // Connection attempts
        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT; i++) {
            helper.onConnectionFailed(null);
            Robolectric.runUiThreadTasksIncludingDelayedTasks();
        }

        // Should have tried to connect every time.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT)).connect();

        // Try again
        helper.onConnectionFailed(null);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();

        // The connection handler should have given up, no new call.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT)).connect();
    }

    /** Tests that when a connection succeeds, the retry limit is reset. */
    @Test
    @Feature({"GCore"})
    public void connectionAttemptsResetTest() {
        GoogleApiClientConnectionHelper helper = new GoogleApiClientConnectionHelper(mMockClient);

        // Connection attempts
        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT - 1; i++) {
            helper.onConnectionFailed(null);
            Robolectric.runUiThreadTasksIncludingDelayedTasks();
        }

        // Should have tried to connect every time.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT - 1)).connect();

        // Connection successful now
        helper.onConnected(null);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();

        for (int i = 0; i < ConnectedTask.RETRY_NUMBER_LIMIT; i++) {
            helper.onConnectionFailed(null);
            Robolectric.runUiThreadTasksIncludingDelayedTasks();
        }

        // A success should allow for more connection attempts.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT * 2 - 1)).connect();

        // This should not result in a connection attempt, the limit is still there.
        helper.onConnectionFailed(null);
        Robolectric.runUiThreadTasksIncludingDelayedTasks();

        // The connection handler should have given up, no new call.
        verify(mMockClient, times(ConnectedTask.RETRY_NUMBER_LIMIT * 2 - 1)).connect();
    }
}
