// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.util.Feature;
import org.chromium.content.common.FileDescriptorInfo;
import org.chromium.content.common.IChildProcessCallback;
import org.chromium.content.common.IChildProcessService;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/**
 * Unit tests for BindingManagerImpl. The tests run agains mock ChildProcessConnection
 * implementation, thus testing only the BindingManagerImpl itself.
 *
 * Default property of being low-end device is overriden, so that both low-end and high-end policies
 * are tested.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BindingManagerImplTest {
    private static class MockChildProcessConnection implements ChildProcessConnection {
        boolean mInitialBindingBound;
        boolean mModerateBindingBound;
        int mStrongBindingCount;
        final int mPid;

        /**
         * Creates a mock binding corresponding to real ChildProcessConnectionImpl after the
         * connection is established: with initial binding bound and no strong binding.
         */
        MockChildProcessConnection(int pid) {
            mInitialBindingBound = true;
            mStrongBindingCount = 0;
            mPid = pid;
        }

        @Override
        public int getPid() {
            return mPid;
        }

        @Override
        public boolean isInitialBindingBound() {
            return mInitialBindingBound;
        }

        @Override
        public boolean isStrongBindingBound() {
            return mStrongBindingCount > 0;
        }

        @Override
        public void removeInitialBinding() {
            mInitialBindingBound = false;
        }

        @Override
        public boolean isOomProtectedOrWasWhenDied() {
            return mInitialBindingBound || mStrongBindingCount > 0;
        }

        @Override
        public void dropOomBindings() {
            mInitialBindingBound = false;
            mStrongBindingCount = 0;
        }

        @Override
        public void addStrongBinding() {
            mStrongBindingCount++;
        }

        @Override
        public void removeStrongBinding() {
            assert mStrongBindingCount > 0;
            mStrongBindingCount--;
        }

        @Override
        public void stop() {
            mInitialBindingBound = false;
            mStrongBindingCount = 0;
        }

        @Override
        public int getServiceNumber() {
            return mPid;
        }

        @Override
        public boolean isInSandbox() {
            return true;
        }

        @Override
        public IChildProcessService getService() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void start(StartCallback startCallback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void setupConnection(String[] commandLine, FileDescriptorInfo[] filesToBeMapped,
                IChildProcessCallback processCallback, ConnectionCallback connectionCallbacks,
                Bundle sharedRelros) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void addModerateBinding() {
            mModerateBindingBound = true;
        }

        @Override
        public void removeModerateBinding() {
            mModerateBindingBound = false;
        }

        @Override
        public boolean isModerateBindingBound() {
            return mModerateBindingBound;
        }

        @Override
        public String getPackageName() {
            return null;
        }

        @Override
        public ChildProcessCreationParams getCreationParams() {
            return null;
        }
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
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        mLowEndManager = BindingManagerImpl.createBindingManagerForTesting(true);
        mHighEndManager = BindingManagerImpl.createBindingManagerForTesting(false);
        mModerateBindingManager = BindingManagerImpl.createBindingManagerForTesting(false);
        mModerateBindingManager.startModerateBindingManagement(mActivity, 4, false);
        mAllManagers = new ManagerEntry[] {
                new ManagerEntry(mLowEndManager, "low-end"),
                new ManagerEntry(mHighEndManager, "high-end"),
                new ManagerEntry(mModerateBindingManager, "moderate-binding")};
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
        MockChildProcessConnection firstConnection = new MockChildProcessConnection(1);
        manager.addNewConnection(firstConnection.getPid(), firstConnection);

        // Bind a strong binding on the connection.
        manager.setInForeground(firstConnection.getPid(), true);
        Assert.assertTrue(firstConnection.isStrongBindingBound());

        // Add a new connection.
        MockChildProcessConnection secondConnection = new MockChildProcessConnection(2);
        manager.addNewConnection(secondConnection.getPid(), secondConnection);

        // Verify that the strong binding for the first connection wasn't dropped.
        Assert.assertTrue(firstConnection.isStrongBindingBound());

        // Verify that the strong binding for the first connection was dropped when a new connection
        // got used in foreground.
        manager.setInForeground(secondConnection.getPid(), true);
        Assert.assertFalse(firstConnection.isStrongBindingBound());
    }

    /**
     * Verifies the strong binding removal policies for low end devices:
     * - the initial binding should not be affected
     * - removal of a strong binding should be executed synchronously
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalOnLowEnd() throws Throwable {
        // This test applies only to the low-end manager.
        final BindingManagerImpl manager = mLowEndManager;

        // Add a connection to the manager.
        final MockChildProcessConnection connection = new MockChildProcessConnection(1);
        manager.addNewConnection(connection.getPid(), connection);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isStrongBindingBound());

        // Add a strong binding, verify that the initial binding is not removed.
        manager.setInForeground(connection.getPid(), true);
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isInitialBindingBound());

        // Remove the strong binding, verify that the strong binding is removed immediately
        // and that the initial binding is not affected.
        manager.setInForeground(connection.getPid(), false);
        Assert.assertFalse(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isInitialBindingBound());
    }

    /**
     * Verifies the strong binding removal policies for high end devices, where the removal should
     * be delayed.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalOnHighEnd() throws Throwable {
        // This test applies only to the high-end manager.
        final BindingManagerImpl manager = mHighEndManager;

        // Add a connection to the manager.
        final MockChildProcessConnection connection = new MockChildProcessConnection(1);
        manager.addNewConnection(connection.getPid(), connection);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isStrongBindingBound());

        // Add a strong binding, verify that the initial binding is not removed.
        manager.setInForeground(connection.getPid(), true);
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isInitialBindingBound());

        // Remove the strong binding, verify that the strong binding is not removed
        // immediately.
        manager.setInForeground(connection.getPid(), false);
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isInitialBindingBound());

        // Wait until the posted unbinding tasks get executed and verify that the strong binding was
        // removed while the initial binding is not affected.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isInitialBindingBound());
    }

    /**
     * Verifies the strong binding removal policies with moderate binding management, where the
     * moderate binding should be bound.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalWithModerateBinding() throws Throwable {
        // This test applies only to the moderate-binding manager.
        final BindingManagerImpl manager = mModerateBindingManager;

        // Add a connection to the manager.
        final MockChildProcessConnection connection = new MockChildProcessConnection(1);
        manager.addNewConnection(connection.getPid(), connection);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isStrongBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        // Add a strong binding, verify that the initial binding is not removed.
        manager.setInForeground(connection.getPid(), true);
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        // Remove the strong binding, verify that the strong binding is not removed
        // immediately.
        manager.setInForeground(connection.getPid(), false);
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        // Wait until the posted unbinding tasks get executed and verify that the strong binding was
        // removed while the initial binding is not affected, and the moderate binding is bound.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(connection.isStrongBindingBound());
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertTrue(connection.isModerateBindingBound());
    }

    /**
     * Verifies that the initial binding is removed after determinedVisibility() is called.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testInitialBindingRemoval() {
        // This test applies to low-end, high-end and moderate-binding policies.
        for (ManagerEntry managerEntry : mAllManagers) {
            BindingManagerImpl manager = managerEntry.mManager;
            String message = managerEntry.getErrorMessage();

            // Add a connection to the manager.
            MockChildProcessConnection connection = new MockChildProcessConnection(1);
            manager.addNewConnection(connection.getPid(), connection);

            // Verify that the initial binding is held.
            Assert.assertTrue(connection.isInitialBindingBound());

            // Call determinedVisibility() and verify that the initial binding was released.
            manager.determinedVisibility(connection.getPid());
            Assert.assertFalse(connection.isInitialBindingBound());
        }
    }

    /**
     * Verifies that BindingManagerImpl correctly stashes the status of the connection oom bindings
     * when the connection is cleared. BindingManagerImpl should reply to isOomProtected() queries
     * with live status of the connection while it's still around and reply with stashed status
     * after clearConnection() is called.
     *
     * This test corresponds to a process crash scenario: after a process dies and its connection is
     * cleared, isOomProtected() may be called to decide if it was a crash or out-of-memory kill.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testIsOomProtected() {
        // This test applies to low-end, high-end and moderate-binding policies.
        for (ManagerEntry managerEntry : mAllManagers) {
            BindingManagerImpl manager = managerEntry.mManager;
            String message = managerEntry.getErrorMessage();

            // Add a connection to the manager.
            MockChildProcessConnection connection = new MockChildProcessConnection(1);
            manager.addNewConnection(connection.getPid(), connection);

            // Initial binding is an oom binding.
            Assert.assertTrue(message, manager.isOomProtected(connection.getPid()));

            // After initial binding is removed, the connection is no longer oom protected.
            manager.setInForeground(connection.getPid(), false);
            manager.determinedVisibility(connection.getPid());
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            Assert.assertFalse(message, manager.isOomProtected(connection.getPid()));

            // Add a strong binding, restoring the oom protection.
            manager.setInForeground(connection.getPid(), true);
            Assert.assertTrue(message, manager.isOomProtected(connection.getPid()));

            // Simulate a process crash - clear a connection in binding manager and remove the
            // bindings.
            Assert.assertFalse(manager.isConnectionCleared(connection.getPid()));
            manager.clearConnection(connection.getPid());
            Assert.assertTrue(manager.isConnectionCleared(connection.getPid()));
            connection.stop();

            // Verify that the connection doesn't keep any oom bindings, but the manager reports the
            // oom status as protected.
            Assert.assertFalse(message, connection.isInitialBindingBound());
            Assert.assertFalse(message, connection.isStrongBindingBound());
            Assert.assertTrue(message, manager.isOomProtected(connection.getPid()));
        }
    }

    /**
     * Verifies that onSentToBackground() / onBroughtToForeground() correctly attach and remove
     * additional strong binding kept on the most recently bound renderer for the background
     * period.
     *
     * The renderer that will be bound for the background period should be the one that was most
     * recendly bound using .setInForeground(), even if there is one that was added using
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
            MockChildProcessConnection firstConnection = new MockChildProcessConnection(1);
            manager.addNewConnection(firstConnection.getPid(), firstConnection);
            manager.setInForeground(firstConnection.getPid(), true);
            manager.setInForeground(firstConnection.getPid(), false);

            MockChildProcessConnection secondConnection = new MockChildProcessConnection(2);
            manager.addNewConnection(secondConnection.getPid(), secondConnection);
            manager.setInForeground(secondConnection.getPid(), true);
            manager.setInForeground(secondConnection.getPid(), false);

            // Add third connection, do not bind it.
            MockChildProcessConnection thirdConnection = new MockChildProcessConnection(3);
            manager.addNewConnection(thirdConnection.getPid(), thirdConnection);
            manager.setInForeground(thirdConnection.getPid(), false);

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

        MockChildProcessConnection[] connections = new MockChildProcessConnection[3];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = new MockChildProcessConnection(i + 1);
            manager.addNewConnection(connections[i].getPid(), connections[i]);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (MockChildProcessConnection connection : connections) {
            manager.setInForeground(connection.getPid(), true);
            manager.setInForeground(connection.getPid(), false);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Exclude lastInForeground because it will be kept in foreground when onSentToBackground()
        // is called as |mLastInForeground|.
        MockChildProcessConnection lastInForeground = new MockChildProcessConnection(0);
        manager.addNewConnection(lastInForeground.getPid(), lastInForeground);
        manager.setInForeground(lastInForeground.getPid(), true);
        manager.setInForeground(lastInForeground.getPid(), false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify that leaving the application for a short time doesn't clear the moderate bindings.
        manager.onSentToBackground();
        for (MockChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }
        Assert.assertTrue(lastInForeground.isStrongBindingBound());
        Assert.assertFalse(lastInForeground.isModerateBindingBound());
        manager.onBroughtToForeground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        for (MockChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Call onSentToBackground() and verify that all the moderate bindings drop after some
        // delay.
        manager.onSentToBackground();
        for (MockChildProcessConnection connection : connections) {
            Assert.assertTrue(connection.isModerateBindingBound());
        }
        Assert.assertTrue(lastInForeground.isStrongBindingBound());
        Assert.assertFalse(lastInForeground.isModerateBindingBound());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        for (MockChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }

        // Call onBroughtToForeground() and verify that the previous moderate bindings aren't
        // recovered.
        manager.onBroughtToForeground();
        for (MockChildProcessConnection connection : connections) {
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

        MockChildProcessConnection[] connections = new MockChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = new MockChildProcessConnection(i + 1);
            manager.addNewConnection(connections[i].getPid(), connections[i]);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (MockChildProcessConnection connection : connections) {
            manager.setInForeground(connection.getPid(), true);
            manager.setInForeground(connection.getPid(), false);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Call onLowMemory() and verify that all the moderate bindings drop.
        app.onLowMemory();
        for (MockChildProcessConnection connection : connections) {
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

        ArrayList<Pair<Integer, Integer>> levelAndExpectedVictimCountList =
                new ArrayList<Pair<Integer, Integer>>();
        levelAndExpectedVictimCountList.add(
                new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_MODERATE, 1));
        levelAndExpectedVictimCountList.add(new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_LOW, 2));
        levelAndExpectedVictimCountList.add(
                new Pair<Integer, Integer>(TRIM_MEMORY_RUNNING_CRITICAL, 4));

        MockChildProcessConnection[] connections = new MockChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = new MockChildProcessConnection(i + 1);
            manager.addNewConnection(connections[i].getPid(), connections[i]);
        }

        for (Pair<Integer, Integer> pair : levelAndExpectedVictimCountList) {
            String message = "Failed for the level=" + pair.first;
            // Verify that each connection has a moderate binding after binding and releasing a
            // strong binding.
            for (MockChildProcessConnection connection : connections) {
                manager.setInForeground(connection.getPid(), true);
                manager.setInForeground(connection.getPid(), false);
                ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
                Assert.assertTrue(message, connection.isModerateBindingBound());
            }

            app.onTrimMemory(pair.first);
            // Verify that some of moderate bindings drop.
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

        MockChildProcessConnection[] connections = new MockChildProcessConnection[4];
        for (int i = 0; i < connections.length; i++) {
            connections[i] = new MockChildProcessConnection(i + 1);
            manager.addNewConnection(connections[i].getPid(), connections[i]);
        }

        // Verify that each connection has a moderate binding after binding and releasing a strong
        // binding.
        for (MockChildProcessConnection connection : connections) {
            manager.setInForeground(connection.getPid(), true);
            manager.setInForeground(connection.getPid(), false);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            Assert.assertTrue(connection.isModerateBindingBound());
        }

        // Call BindingManager.releaseAllModerateBindings() and verify that all the moderate
        // bindings drop.
        manager.releaseAllModerateBindings();
        for (MockChildProcessConnection connection : connections) {
            Assert.assertFalse(connection.isModerateBindingBound());
        }
    }

    /**
     * Test that when the "Moderate Binding Till Backgrounded" experiment is disabled that no
     * moderate binding is added to background renderer processes when the initial binding is
     * removed.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedExperimentDisabledBackgroundProcess() {
        BindingManagerImpl manager = BindingManagerImpl.createBindingManagerForTesting(false);
        manager.startModerateBindingManagement(mActivity, 4, false);

        MockChildProcessConnection connection = new MockChildProcessConnection(0);
        manager.addNewConnection(connection.getPid(), connection);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        manager.setInForeground(connection.getPid(), false);
        manager.determinedVisibility(connection.getPid());
        Assert.assertFalse(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());
    }

    /**
     * Test that when the "Moderate Binding Till Backgrounded" experiment is enabled that a moderate
     * binding is added to background renderer processes when the initial binding is removed.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedExperimentBackgroundProcess() {
        BindingManagerImpl manager = BindingManagerImpl.createBindingManagerForTesting(false);
        manager.startModerateBindingManagement(mActivity, 4, true);

        MockChildProcessConnection connection = new MockChildProcessConnection(0);
        manager.addNewConnection(connection.getPid(), connection);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        manager.setInForeground(connection.getPid(), false);
        manager.determinedVisibility(connection.getPid());
        Assert.assertFalse(connection.isInitialBindingBound());
        Assert.assertTrue(connection.isModerateBindingBound());
    }

    /**
     * Test that when the "Moderate Binding Till Backgrounded" experiment is enabled that a moderate
     * binding is not added to foreground renderer processes when the initial binding is removed.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedExperimentForegroundProcess() {
        BindingManagerImpl manager = BindingManagerImpl.createBindingManagerForTesting(false);
        manager.startModerateBindingManagement(mActivity, 4, true);

        MockChildProcessConnection connection = new MockChildProcessConnection(0);
        manager.addNewConnection(connection.getPid(), connection);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.isStrongBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());

        manager.setInForeground(connection.getPid(), true);
        manager.determinedVisibility(connection.getPid());
        Assert.assertFalse(connection.isInitialBindingBound());
        Assert.assertTrue(connection.isStrongBindingBound());
        Assert.assertFalse(connection.isModerateBindingBound());
    }

    /**
     * Test that when the "Moderate Binding Till Backgrounded" experiment is enabled and that Chrome
     * is sent to the background, that the initially added moderate bindings are removed and are not
     * re-added when Chrome is brought back to the foreground.
     */
    @Test
    @Feature({"ProcessManagement"})
    public void testModerateBindingTillBackgroundedExperimentSentToBackground() {
        BindingManagerImpl manager = BindingManagerImpl.createBindingManagerForTesting(false);
        manager.startModerateBindingManagement(mActivity, 4, true);

        MockChildProcessConnection connection = new MockChildProcessConnection(0);
        manager.addNewConnection(connection.getPid(), connection);
        manager.setInForeground(connection.getPid(), false);
        manager.determinedVisibility(connection.getPid());
        Assert.assertTrue(connection.isModerateBindingBound());

        manager.onSentToBackground();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(connection.isModerateBindingBound());

        // Bringing Chrome to the foreground should not re-add the moderate bindings.
        manager.onBroughtToForeground();
        Assert.assertFalse(connection.isModerateBindingBound());
    }
}
