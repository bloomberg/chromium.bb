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
import org.chromium.base.test.TestChildProcessConnection;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/**
 * Unit tests for BindingManagerImpl and ChildProcessConnection.
 *
 * Default property of being low-end device is overriden, so that both low-end and high-end policies
 * are tested.
 */
@RunWith(LocalRobolectricTestRunner.class)
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
        connection.start(false /* useStrongBinding */, null /* serviceCallback */,
                false /* retryOnTimeout */);
        if (manager != null) {
            manager.addNewConnection(pid, connection);
        }
        return connection;
    }

    /**
     * Helper class that stores a manager along with its text label. This is used for tests that
     * iterate over all managers to indicate which manager was being tested when an assertion
     * failed.
     */
    private static class ManagerEntry {
        BindingManagerImpl mManager;
        String mLabel;  // Name of the manager.

        ManagerEntry(BindingManagerImpl manager, String label) {
            mManager = manager;
            mLabel = label;
        }

        String getErrorMessage() {
            return "Failed for the " + mLabel + " manager.";
        }
    }

    Activity mActivity;

    // The managers are created in setUp() for convenience.
    BindingManagerImpl mLowEndManager;
    BindingManagerImpl mHighEndManager;
    BindingManagerImpl mModerateBindingManager;
    ManagerEntry[] mAllManagers;

    @Before
    public void setUp() {
        // The tests run on only one thread. Pretend that is the launcher thread so LauncherThread
        // asserts are not triggered.
        LauncherThread.setCurrentThreadAsLauncherThread();

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        mLowEndManager =
                BindingManagerImpl.createBindingManagerForTesting(true /* isLowEndDevice */);
        mHighEndManager =
                BindingManagerImpl.createBindingManagerForTesting(false /* isLowEndDevice */);
        mModerateBindingManager =
                BindingManagerImpl.createBindingManagerForTesting(false /* isLowEndDevice */);
        mModerateBindingManager.startModerateBindingManagement(mActivity, 4 /* maxSize */);
        mAllManagers = new ManagerEntry[] {
                new ManagerEntry(mLowEndManager, "low-end"),
                new ManagerEntry(mHighEndManager, "high-end"),
                new ManagerEntry(mModerateBindingManager, "moderate-binding")};
    }

    @After
    public void tearDown() {
        LauncherThread.setLauncherThreadAsLauncherThread();
    }

    /**
     * Verifies that when running on low-end, the binding manager drops the oom bindings for the
     * previously bound connection when a new connection is used in foreground.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testNewConnectionDropsPreviousOnLowEnd() {
        // This test applies only to the low-end manager.
        BindingManagerImpl manager = mLowEndManager;

        // Add a connection to the manager.
        ChildProcessConnection firstConnection =
                createTestChildProcessConnection(1 /* pid */, manager);

        // Bind a strong binding on the connection.
        manager.setPriority(firstConnection.getPid(), true /* foreground */, false /* boost */);
        Assert.assertTrue(firstConnection.isStrongBindingBound());

        // Add a new connection.
        ChildProcessConnection secondConnection =
                createTestChildProcessConnection(2 /* pid */, manager);

        // Verify that the strong binding for the first connection wasn't dropped.
        Assert.assertTrue(firstConnection.isStrongBindingBound());

        // Verify that the strong binding for the first connection was dropped when a new connection
        // got used in foreground.
        manager.setPriority(secondConnection.getPid(), true /* foreground */, false /* boost */);
        Assert.assertFalse(firstConnection.isStrongBindingBound());
        Assert.assertTrue(secondConnection.isStrongBindingBound());
    }

    /**
     * Verifies the strong binding removal policies for low end devices:
     * - removal of a strong binding should be executed synchronously
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalOnLowEnd() throws Throwable {
        // This test applies only to the low-end manager.
        BindingManagerImpl manager = mLowEndManager;

        // Add a connection to the manager.
        ChildProcessConnection connection = createTestChildProcessConnection(1 /* pid */, manager);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isStrongBindingBound());

        // Add a strong binding.
        manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
        Assert.assertTrue(connection.isStrongBindingBound());

        // Remove the strong binding, verify that the strong binding is removed immediately.
        manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
        Assert.assertFalse(connection.isStrongBindingBound());
    }

    /**
     * Verifies the strong binding removal policies for high end devices, where the removal should
     * be delayed.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalOnHighEnd() throws Throwable {
        // This test applies only to the high-end manager.
        BindingManagerImpl manager = mHighEndManager;

        // Add a connection to the manager.
        ChildProcessConnection connection = createTestChildProcessConnection(1 /* pid */, manager);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isStrongBindingBound());

        // Add a strong binding, verify that the initial binding is not removed.
        manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
        Assert.assertTrue(connection.isStrongBindingBound());

        // Remove the strong binding, verify that the strong binding is not removed
        // immediately.
        manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
        Assert.assertTrue(connection.isStrongBindingBound());

        // Wait until the posted unbinding tasks get executed and verify that the strong binding was
        // removed while the initial binding is not affected.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(connection.isStrongBindingBound());
    }

    /**
     * Verifies the strong binding removal policies with moderate binding management, where the
     * moderate binding should be bound.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalWithModerateBinding() throws Throwable {
        // This test applies only to the moderate-binding manager.
        BindingManagerImpl manager = mModerateBindingManager;

        // Add a connection to the manager and start it.
        ChildProcessConnection connection = createTestChildProcessConnection(1 /* pid */, manager);

        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isStrongBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        // Add a strong binding, verify that the initial binding is not removed.
        manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        // Remove the strong binding, verify that the strong binding is not removed
        // immediately.
        manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        // Wait until the posted unbinding tasks get executed and verify that the strong binding was
        // removed while the initial binding is not affected, and the moderate binding is bound.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isModerateBindingBound());
    }

    /**
     * This test corresponds to a process crash scenario: after a process dies and its connection is
     * cleared, isWaivedBoundOnlyOrWasWhenDied() may be called on the connection to decide if it was
     * a crash or out-of-memory kill.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testIsWaivedBoundOnly() {
        // This test applies to low-end, high-end and moderate-binding policies.
        for (ManagerEntry managerEntry : mAllManagers) {
            BindingManagerImpl manager = managerEntry.mManager;
            String message = managerEntry.getErrorMessage();

            // Add a connection to the manager.
            ChildProcessConnection connection =
                    createTestChildProcessConnection(1 /* pid */, manager);

            // Initial binding is a moderate binding.
            Assert.assertFalse(message, connection.isWaivedBoundOnlyOrWasWhenDied());

            // After initial binding is removed, the connection is no longer waived bound only.
            manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            if (managerEntry.mManager == mModerateBindingManager) {
                // The moderate binding manager adds a moderate binding.
                Assert.assertFalse(message, connection.isWaivedBoundOnlyOrWasWhenDied());
            } else {
                Assert.assertTrue(message, connection.isWaivedBoundOnlyOrWasWhenDied());
            }

            // Add a strong binding.
            manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
            Assert.assertFalse(message, connection.isWaivedBoundOnlyOrWasWhenDied());

            // Simulate a process crash - clear a connection in binding manager and remove the
            // bindings.
            Assert.assertFalse(manager.isConnectionCleared(connection.getPid()));
            manager.removeConnection(connection.getPid());
            Assert.assertTrue(manager.isConnectionCleared(connection.getPid()));
            connection.stop();

            // Verify that manager reports the the connection was waived bound.
            Assert.assertFalse(message, connection.isInitialBindingBound());
            Assert.assertFalse(message, connection.isModerateBindingBound());
            Assert.assertFalse(message, connection.isStrongBindingBound());
            Assert.assertFalse(message, connection.isWaivedBoundOnlyOrWasWhenDied());
        }
    }

    /**
     * Verifies that onSentToBackground() / onBroughtToForeground() correctly attach and remove
     * additional strong binding kept on the most recently bound renderer for the background
     * period.
     *
     * The renderer that will be bound for the background period should be the one that was most
     * recendly bound using .setPriority(), even if there is one that was added using
     * .addNewConnection() after that. Otherwise we would bound a background renderer when user
     * loads a new tab in background and leaves the browser.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testBackgroundPeriodBinding() {
        // This test applies to low-end, high-end and moderate-binding policies.
        for (ManagerEntry managerEntry : mAllManagers) {
            BindingManagerImpl manager = managerEntry.mManager;
            String message = managerEntry.getErrorMessage();

            // Add two connections, bind and release each.
            ChildProcessConnection firstConnection =
                    createTestChildProcessConnection(1 /* pid */, manager);
            manager.setPriority(firstConnection.getPid(), true /* foreground */, false /* boost */);
            manager.setPriority(
                    firstConnection.getPid(), false /* foreground */, false /* boost */);

            ChildProcessConnection secondConnection =
                    createTestChildProcessConnection(2 /* pid */, manager);
            manager.setPriority(
                    secondConnection.getPid(), true /* foreground */, false /* boost */);
            manager.setPriority(
                    secondConnection.getPid(), false /* foreground */, false /* boost */);

            // Add third connection, do not bind it.
            ChildProcessConnection thirdConnection =
                    createTestChildProcessConnection(3 /* pid */, manager);
            manager.setPriority(
                    thirdConnection.getPid(), false /* foreground */, false /* boost */);

            // Sanity check: verify that no connection has a strong binding.
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            Assert.assertFalse(message, firstConnection.isStrongBindingBound());
            Assert.assertFalse(message, secondConnection.isStrongBindingBound());
            Assert.assertFalse(message, thirdConnection.isStrongBindingBound());

            // Call onSentToBackground() and verify that a strong binding was added for the second
            // connection:
            // - not the first one, because it was bound earlier than the second
            // - not the thirs one, because it was never bound at all
            manager.onSentToBackground();
            Assert.assertFalse(message, firstConnection.isStrongBindingBound());
            Assert.assertTrue(message, secondConnection.isStrongBindingBound());
            Assert.assertFalse(message, thirdConnection.isStrongBindingBound());

            // Call onBroughtToForeground() and verify that the strong binding was removed.
            manager.onBroughtToForeground();
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            Assert.assertFalse(message, firstConnection.isStrongBindingBound());
            Assert.assertFalse(message, secondConnection.isStrongBindingBound());
            Assert.assertFalse(message, thirdConnection.isStrongBindingBound());
        }
    }

    /**
     * Verifies that onSentToBackground() drops all the moderate bindings after some delay, and
     * onBroughtToForeground() doesn't recover them.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnBackground() {
        // This test applies only to the moderate-binding manager.
        final BindingManagerImpl manager = mModerateBindingManager;

        ChildProcessConnection[] connections = new ChildProcessConnection[3];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (ChildProcessConnection connection : connections) {
            manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
            manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Exclude lastInForeground because it will be kept in foreground when onSentToBackground()
        // is called as |mLastInForeground|.
        ChildProcessConnection lastInForeground =
                createTestChildProcessConnection(0 /* pid */, manager);
        manager.setPriority(lastInForeground.getPid(), true /* foreground */, false /* boost */);
        manager.setPriority(lastInForeground.getPid(), false /* foreground */, false /* boost */);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify that leaving the application for a short time doesn't clear the moderate bindings.
        manager.onSentToBackground();
        for (ChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }
        Assert.assertTrue(lastInForeground.isStrongBindingBound());
        Assert.assertFalse(lastInForeground.isModerateBindingBound());
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
        Assert.assertTrue(lastInForeground.isStrongBindingBound());
        Assert.assertFalse(lastInForeground.isModerateBindingBound());
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
        final BindingManagerImpl manager = mModerateBindingManager;

        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (ChildProcessConnection connection : connections) {
            manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
            manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
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
        final BindingManagerImpl manager = mModerateBindingManager;

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
                manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
                manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
                ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
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
        // This test applies only to the moderate-binding manager.
        final BindingManagerImpl manager = mModerateBindingManager;

        ChildProcessConnection[] connections = new ChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = createTestChildProcessConnection(i + 1 /* pid */, manager);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (ChildProcessConnection connection : connections) {
            manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
            manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
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
     * Test that a moderate binding is added to background renderer processes when the initial
     * binding is removed.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedBackgroundProcess() {
        BindingManagerImpl manager = BindingManagerImpl.createBindingManagerForTesting(false);
        manager.startModerateBindingManagement(mActivity, 4);

        ChildProcessConnection connection = createTestChildProcessConnection(0 /* pid */, manager);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        manager.setPriority(connection.getPid(), false, false /* boost */);
        Assert.assertFalse(connection.isInitialBindingBound());
        Assert.assertTrue(connection.isModerateBindingBound());
    }

    /*
     * Test that a moderate binding is not added to foreground renderer processes when the initial
     * binding is removed.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedForegroundProcess() {
        BindingManagerImpl manager = BindingManagerImpl.createBindingManagerForTesting(false);
        manager.startModerateBindingManagement(mActivity, 4);

        ChildProcessConnection connection = createTestChildProcessConnection(0 /* pid */, manager);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isStrongBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        manager.setPriority(connection.getPid(), true /* foreground */, false /* boost */);
        Assert.assertFalse(connection.isInitialBindingBound());
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());
    }

    /*
     * Test that Chrome is sent to the background, that the initially added moderate bindings are
     * removed and are not re-added when Chrome is brought back to the foreground.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedSentToBackground() {
        BindingManagerImpl manager = BindingManagerImpl.createBindingManagerForTesting(false);
        manager.startModerateBindingManagement(mActivity, 4);

        ChildProcessConnection connection = createTestChildProcessConnection(0, manager);
        manager.setPriority(connection.getPid(), false /* foreground */, false /* boost */);
        Assert.assertTrue(connection.isModerateBindingBound());

        manager.onSentToBackground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(connection.isModerateBindingBound());

        // Bringing Chrome to the foreground should not re-add the moderate bindings.
        manager.onBroughtToForeground();
        Assert.assertFalse(connection.isModerateBindingBound());
    }
}
