// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentName;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.TestChildProcessConnection;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;

/**
 * Unit tests for BindingManagerImpl and ChildProcessConnection.
 *
 * Default property of being low-end device is overriden, so that both low-end and high-end policies
 * are tested.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BindingManagerImplTest {
    // Creates a mocked ChildProcessConnection that is optionally added to a BindingManager.
    private static ChildProcessConnection createTestChildProcessConnection(
            int pid, BindingManager manager) {
        TestChildProcessConnection connection = new TestChildProcessConnection(
                new ComponentName("org.chromium.test", "TestService"),
                false /* bindToCallerCheck */, false /* bindAsExternalService */,
                null /* serviceBundle */);
        connection.setPid(pid);
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        if (manager != null) {
            manager.increaseRecency(connection);
        }
        return connection;
    }

    Activity mActivity;

    // Created in setUp() for convenience.
    BindingManagerImpl mManager;

    @Before
    public void setUp() {
        // The tests run on only one thread. Pretend that is the launcher thread so LauncherThread
        // asserts are not triggered.
        LauncherThread.setCurrentThreadAsLauncherThread();
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mManager = new BindingManagerImpl(mActivity, 4, true);
    }

    @After
    public void tearDown() {
        LauncherThread.setLauncherThreadAsLauncherThread();
    }

    /**
     * Verifies that onSentToBackground() drops all the moderate bindings after some delay, and
     * onBroughtToForeground() doesn't recover them.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnBackground() {
        final BindingManagerImpl manager = mManager;

        ChildProcessConnection[] connections = new ChildProcessConnection[3];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify that leaving the application for a short time doesn't clear the moderate bindings.
        manager.onSentToBackground();
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }
        manager.onBroughtToForeground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Call onSentToBackground() and verify that all the moderate bindings drop after some
        // delay.
        manager.onSentToBackground();
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        for (ChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }

        // Call onBroughtToForeground() and verify that the previous moderate bindings aren't
        // recovered.
        manager.onBroughtToForeground();
        for (ChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }
    }

    /**
     * Verifies that onLowMemory() drops all the moderate bindings.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnLowMemory() {
        final Application app = mActivity.getApplication();
        final BindingManagerImpl manager = mManager;

        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager);
        }

        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Call onLowMemory() and verify that all the moderate bindings drop.
        app.onLowMemory();
        for (ChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }
    }

    /**
     * Verifies that onTrimMemory() drops moderate bindings properly.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnTrimMemory() {
        final Application app = mActivity.getApplication();
        // This test applies only to the moderate-binding manager.
        final BindingManagerImpl manager = mManager;

        ArrayList<Pair<Integer, Integer>> levelAndExpectedVictimCountList = new ArrayList<>();
        levelAndExpectedVictimCountList.add(
                new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_MODERATE, 1));
        levelAndExpectedVictimCountList.add(new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_LOW, 2));
        levelAndExpectedVictimCountList.add(
                new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_CRITICAL, 4));

        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager);
        }

        for (Pair<Integer, Integer> pair : levelAndExpectedVictimCountList) {
            String message = "Failed for the level=" + pair.first;
            // Verify that each connection has a moderate binding after binding and releasing a
            // strong binding.
            for (ChildProcessConnection connection : connections) {
                manager.increaseRecency(connection);
                Assert.assertTrue(message, connection.isModerateBindingBound());
            }

            app.onTrimMemory(pair.first);
            // Verify that some of the moderate bindings have been dropped.
            for (int i = 0; i < connections.length; i++) {
                Assert.assertEquals(
                        message, i >= pair.second, connections[i].isModerateBindingBound());
            }
        }
    }

    /**
     * Verifies that BindingManager.releaseAllModerateBindings() drops all the moderate bindings.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnReleaseAllModerateBindings() {
        final BindingManagerImpl manager = mManager;

        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Call BindingManager.releaseAllModerateBindings() and verify that all the moderate
        // bindings drop.
        manager.releaseAllModerateBindings();
        for (ChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }
    }

    /*
     * Test that Chrome is sent to the background, that the initially added moderate bindings are
     * removed and are not re-added when Chrome is brought back to the foreground.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedSentToBackground() {
        BindingManagerImpl manager = mManager;

        ChildProcessConnection connection = createTestChildProcessConnection(0, manager);
        Assert.assertTrue(connection.isModerateBindingBound());

        manager.onSentToBackground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(connection.isModerateBindingBound());

        // Bringing Chrome to the foreground should not re-add the moderate bindings.
        manager.onBroughtToForeground();
        Assert.assertFalse(connection.isModerateBindingBound());
    }
}
