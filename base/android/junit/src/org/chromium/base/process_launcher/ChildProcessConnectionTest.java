// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.TestChildProcessConnection;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Unit tests for ChildProcessConnection. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChildProcessConnectionTest {
    private TestChildProcessConnection createTestConnection() {
        String packageName = "org.chromium.test";
        String serviceName = "TestService";
        return new TestChildProcessConnection(new ComponentName(packageName, serviceName),
                false /* bindToCaller */, false /* bindAsExternalService */,
                null /* serviceBundle */);
    }

    @Test
    public void testWatchdog() {
        TestChildProcessConnection connection = createTestConnection();
        connection.setPostOnServiceConnected(false);

        connection.start(false /* useStrongBinding */, null /* serviceCallback */,
                true /* retryOnTimeout */);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());

        connection.setPostOnServiceConnected(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertTrue(connection.didOnServiceConnectedForTesting());
    }

    @Test
    public void testWatchdogDisabled() {
        TestChildProcessConnection connection = createTestConnection();
        connection.setPostOnServiceConnected(false);

        connection.start(false /* useStrongBinding */, null /* serviceCallback */,
                false /* retryOnTimeout */);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());

        connection.setPostOnServiceConnected(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue(connection.isInitialBindingBound());
        // No retry, so service is still not connected.
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());
    }

    @Test
    public void testWatchdogCancelled() {
        TestChildProcessConnection connection = createTestConnection();
        connection.start(false /* useStrongBinding */, null /* serviceCallback */,
                true /* retryOnTimeout */);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertTrue(connection.didOnServiceConnectedForTesting());

        connection.setPostOnServiceConnected(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        // Did not attempt to rebind.
        Assert.assertTrue(connection.isInitialBindingBound());
    }
}
