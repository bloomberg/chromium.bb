// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW;
import static android.content.ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE;

import android.app.Application;
import android.os.Bundle;
import android.os.SystemClock;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;

import org.chromium.base.test.util.Feature;
import org.chromium.content.common.IChildProcessCallback;
import org.chromium.content.common.IChildProcessService;

import java.util.ArrayList;

/**
 * Unit tests for BindingManagerImpl. The tests run agains mock ChildProcessConnection
 * implementation, thus testing only the BindingManagerImpl itself.
 *
 * Default property of being low-end device is overriden, so that both low-end and high-end policies
 * are tested. Unbinding delays are set to 0, so that we don't need to deal with waiting, but we
 * still can test if the unbinding tasks are posted or executed synchronously.
 */
public class BindingManagerImplTest extends InstrumentationTestCase {
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
        public void start(String[] commandLine) {
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

    // The managers are created in setUp() for convenience.
    BindingManagerImpl mLowEndManager;
    BindingManagerImpl mHighEndManager;
    BindingManagerImpl mModerateBindingManager;
    ManagerEntry[] mAllManagers;

    @Override
    protected void setUp() {
        mLowEndManager = BindingManagerImpl.createBindingManagerForTesting(true);
        mHighEndManager = BindingManagerImpl.createBindingManagerForTesting(false);
        mModerateBindingManager = BindingManagerImpl.createBindingManagerForTesting(false);
        mModerateBindingManager.startModerateBindingManagement(
                getInstrumentation().getTargetContext(), 4, 0.25f, 0.5f);
        mAllManagers = new ManagerEntry[] {
                new ManagerEntry(mLowEndManager, "low-end"),
                new ManagerEntry(mHighEndManager, "high-end"),
                new ManagerEntry(mModerateBindingManager, "moderate-binding")};
    }

    /**
     * Verifies that when running on low-end, the binding manager drops the oom bindings for the
     * previously bound connection when a new connection is used in foreground.
     */
    @SmallTest
    @Feature({"ProcessManagement"})
    public void testNewConnectionDropsPreviousOnLowEnd() {
        // This test applies only to the low-end manager.
        BindingManagerImpl manager = mLowEndManager;

        // Add a connection to the manager.
        MockChildProcessConnection firstConnection = new MockChildProcessConnection(1);
        manager.addNewConnection(firstConnection.getPid(), firstConnection);

        // Bind a strong binding on the connection.
        manager.setInForeground(firstConnection.getPid(), true);
        assertTrue(firstConnection.isStrongBindingBound());

        // Add a new connection.
        MockChildProcessConnection secondConnection = new MockChildProcessConnection(2);
        manager.addNewConnection(secondConnection.getPid(), secondConnection);

        // Verify that the strong binding for the first connection wasn't dropped.
        assertTrue(firstConnection.isStrongBindingBound());

        // Verify that the strong binding for the first connection was dropped when a new connection
        // got used in foreground.
        manager.setInForeground(secondConnection.getPid(), true);
        assertFalse(firstConnection.isStrongBindingBound());
    }

    /**
     * Verifies the strong binding removal policies for low end devices:
     * - the initial binding should not be affected
     * - removal of a strong binding should be executed synchronously
     */
    @SmallTest
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalOnLowEnd() throws Throwable {
        // This test applies only to the low-end manager.
        final BindingManagerImpl manager = mLowEndManager;

        // Add a connection to the manager.
        final MockChildProcessConnection connection = new MockChildProcessConnection(1);
        manager.addNewConnection(connection.getPid(), connection);
        assertTrue(connection.isInitialBindingBound());
        assertFalse(connection.isStrongBindingBound());

        // This has to happen on the UI thread, so that we can check the binding status right after
        // the call to remove it, before the any other task is executed on the main thread.
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Add a strong binding, verify that the initial binding is not removed.
                manager.setInForeground(connection.getPid(), true);
                assertTrue(connection.isStrongBindingBound());
                assertTrue(connection.isInitialBindingBound());

                // Remove the strong binding, verify that the strong binding is removed immediately
                // and that the initial binding is not affected.
                manager.setInForeground(connection.getPid(), false);
                assertFalse(connection.isStrongBindingBound());
                assertTrue(connection.isInitialBindingBound());
            }
        });
    }

    /**
     * Verifies the strong binding removal policies for high end devices, where the removal should
     * be delayed.
     */
    @SmallTest
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalOnHighEnd() throws Throwable {
        // This test applies only to the high-end manager.
        final BindingManagerImpl manager = mHighEndManager;

        // Add a connection to the manager.
        final MockChildProcessConnection connection = new MockChildProcessConnection(1);
        manager.addNewConnection(connection.getPid(), connection);
        assertTrue(connection.isInitialBindingBound());
        assertFalse(connection.isStrongBindingBound());
        // This has to happen on the UI thread, so that we can check the binding status right after
        // the call to remove it, before the any other task is executed on the main thread.
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Add a strong binding, verify that the initial binding is not removed.
                manager.setInForeground(connection.getPid(), true);
                assertTrue(connection.isStrongBindingBound());
                assertTrue(connection.isInitialBindingBound());

                // Remove the strong binding, verify that the strong binding is not removed
                // immediately.
                manager.setInForeground(connection.getPid(), false);
                assertTrue(connection.isStrongBindingBound());
                assertTrue(connection.isInitialBindingBound());
            }
        });

        // Wait until the posted unbinding tasks get executed and verify that the strong binding was
        // removed while the initial binding is not affected. Note that this works only because the
        // test binding manager has the unbinding delay set to 0.
        getInstrumentation().waitForIdleSync();
        assertFalse(connection.isStrongBindingBound());
        assertTrue(connection.isInitialBindingBound());
    }

    /**
     * Verifies the strong binding removal policies with moderate binding management, where the
     * moderate binding should be bound.
     */
    @SmallTest
    @Feature({"ProcessManagement"})
    public void testStrongBindingRemovalWithModerateBinding() throws Throwable {
        // This test applies only to the moderate-binding manager.
        final BindingManagerImpl manager = mModerateBindingManager;

        // Add a connection to the manager.
        final MockChildProcessConnection connection = new MockChildProcessConnection(1);
        manager.addNewConnection(connection.getPid(), connection);
        assertTrue(connection.isInitialBindingBound());
        assertFalse(connection.isStrongBindingBound());
        assertFalse(connection.isModerateBindingBound());
        // This has to happen on the UI thread, so that we can check the binding status right after
        // the call to remove it, before the any other task is executed on the main thread.
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Add a strong binding, verify that the initial binding is not removed.
                manager.setInForeground(connection.getPid(), true);
                assertTrue(connection.isStrongBindingBound());
                assertTrue(connection.isInitialBindingBound());
                assertFalse(connection.isModerateBindingBound());

                // Remove the strong binding, verify that the strong binding is not removed
                // immediately.
                manager.setInForeground(connection.getPid(), false);
                assertTrue(connection.isStrongBindingBound());
                assertTrue(connection.isInitialBindingBound());
                assertFalse(connection.isModerateBindingBound());
            }
        });

        // Wait until the posted unbinding tasks get executed and verify that the strong binding was
        // removed while the initial binding is not affected, and the moderate binding is bound.
        getInstrumentation().waitForIdleSync();
        assertFalse(connection.isStrongBindingBound());
        assertTrue(connection.isInitialBindingBound());
        assertTrue(connection.isModerateBindingBound());
    }

    /**
     * Verifies that the initial binding is removed after determinedVisibility() is called.
     */
    @SmallTest
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
            assertTrue(connection.isInitialBindingBound());

            // Call determinedVisibility() and verify that the initial binding was released.
            manager.determinedVisibility(connection.getPid());
            assertFalse(connection.isInitialBindingBound());
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
    @SmallTest
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
            assertTrue(message, manager.isOomProtected(connection.getPid()));

            // After initial binding is removed, the connection is no longer oom protected.
            manager.setInForeground(connection.getPid(), false);
            manager.determinedVisibility(connection.getPid());
            getInstrumentation().waitForIdleSync();
            assertFalse(message, manager.isOomProtected(connection.getPid()));

            // Add a strong binding, restoring the oom protection.
            manager.setInForeground(connection.getPid(), true);
            assertTrue(message, manager.isOomProtected(connection.getPid()));

            // Simulate a process crash - clear a connection in binding manager and remove the
            // bindings.
            assertFalse(manager.isConnectionCleared(connection.getPid()));
            manager.clearConnection(connection.getPid());
            assertTrue(manager.isConnectionCleared(connection.getPid()));
            connection.stop();

            // Verify that the connection doesn't keep any oom bindings, but the manager reports the
            // oom status as protected.
            assertFalse(message, connection.isInitialBindingBound());
            assertFalse(message, connection.isStrongBindingBound());
            assertTrue(message, manager.isOomProtected(connection.getPid()));
        }
    }

    /**
     * Verifies that onSentToBackground() / onBroughtToForeground() correctly attach and remove
     * additional strong binding keept on the most recently bound renderer for the background
     * period.
     *
     * The renderer that will be bound for the background period should be the one that was most
     * recendly bound using .setInForeground(), even if there is one that was added using
     * .addNewConnection() after that. Otherwise we would bound a background renderer when user
     * loads a new tab in background and leaves the browser.
     */
    @SmallTest
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
            getInstrumentation().waitForIdleSync();
            assertFalse(message, firstConnection.isStrongBindingBound());
            assertFalse(message, secondConnection.isStrongBindingBound());
            assertFalse(message, thirdConnection.isStrongBindingBound());

            // Call onSentToBackground() and verify that a strong binding was added for the second
            // connection:
            // - not the first one, because it was bound earlier than the second
            // - not the thirs one, because it was never bound at all
            manager.onSentToBackground();
            assertFalse(message, firstConnection.isStrongBindingBound());
            assertTrue(message, secondConnection.isStrongBindingBound());
            assertFalse(message, thirdConnection.isStrongBindingBound());

            // Call onBroughtToForeground() and verify that the strong binding was removed.
            manager.onBroughtToForeground();
            getInstrumentation().waitForIdleSync();
            assertFalse(message, firstConnection.isStrongBindingBound());
            assertFalse(message, secondConnection.isStrongBindingBound());
            assertFalse(message, thirdConnection.isStrongBindingBound());
        }
    }

    /**
     * Verifies that onSentToBackground() drops all the moderate bindings after some delay, and
     * onBroughtToForeground() doesn't recover them.
     */
    @SmallTest
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
            getInstrumentation().waitForIdleSync();
            assertTrue(connection.isModerateBindingBound());
        }

        // Exclude lastInForeground because it will be kept in foreground when onSentToBackground()
        // is called as |mLastInForeground|.
        MockChildProcessConnection lastInForeground = new MockChildProcessConnection(0);
        manager.addNewConnection(lastInForeground.getPid(), lastInForeground);
        manager.setInForeground(lastInForeground.getPid(), true);
        manager.setInForeground(lastInForeground.getPid(), false);
        getInstrumentation().waitForIdleSync();

        // Verify that leaving the application for a short time doesn't clear the moderate bindings.
        manager.onSentToBackground();
        for (MockChildProcessConnection connection : connections) {
            assertTrue(connection.isModerateBindingBound());
        }
        assertTrue(lastInForeground.isStrongBindingBound());
        assertFalse(lastInForeground.isModerateBindingBound());
        manager.onBroughtToForeground();
        SystemClock.sleep(500);
        for (MockChildProcessConnection connection : connections) {
            assertTrue(connection.isModerateBindingBound());
        }

        // Call onSentToBackground() and verify that all the moderate bindings drop after some
        // delay.
        manager.onSentToBackground();
        for (MockChildProcessConnection connection : connections) {
            assertTrue(connection.isModerateBindingBound());
        }
        assertTrue(lastInForeground.isStrongBindingBound());
        assertFalse(lastInForeground.isModerateBindingBound());
        SystemClock.sleep(500);
        for (MockChildProcessConnection connection : connections) {
            assertFalse(connection.isModerateBindingBound());
        }

        // Call onBroughtToForeground() and verify that the previous moderate bindings aren't
        // recovered.
        manager.onBroughtToForeground();
        for (MockChildProcessConnection connection : connections) {
            assertFalse(connection.isModerateBindingBound());
        }
    }

    /**
     * Verifies that onLowMemory() drops all the moderate bindings.
     */
    @SmallTest
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnLowMemory() {
        final Application app =
                (Application) getInstrumentation().getTargetContext().getApplicationContext();
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
            getInstrumentation().waitForIdleSync();
            assertTrue(connection.isModerateBindingBound());
        }

        // Call onLowMemory() and verify that all the moderate bindings drop.
        app.onLowMemory();
        for (MockChildProcessConnection connection : connections) {
            assertFalse(connection.isModerateBindingBound());
        }
    }

    /**
     * Verifies that onTrimMemory() drops moderate bindings properly.
     */
    @SmallTest
    @Feature({"ProcessManagement"})
    public void testModerateBindingDropOnTrimMemory() {
        final Application app =
                (Application) getInstrumentation().getTargetContext().getApplicationContext();
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
                getInstrumentation().waitForIdleSync();
                assertTrue(message, connection.isModerateBindingBound());
            }

            app.onTrimMemory(pair.first);
            // Verify that some of moderate bindings drop.
            for (int i = 0; i < connections.length; i++) {
                assertEquals(message, i >= pair.second, connections[i].isModerateBindingBound());
            }
        }
    }

    /**
     * Verifies that BindingManager.releaseAllModerateBindings() drops all the moderate bindings.
     */
    @SmallTest
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
            getInstrumentation().waitForIdleSync();
            assertTrue(connection.isModerateBindingBound());
        }

        // Call BindingManager.releaseAllModerateBindings() and verify that all the moderate
        // bindings drop.
        manager.releaseAllModerateBindings();
        for (MockChildProcessConnection connection : connections) {
            assertFalse(connection.isModerateBindingBound());
        }
    }
}
