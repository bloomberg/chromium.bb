// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabWindowManager.TabModelSelectorFactory;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

/**
 * Test for {@link TabWindowManager} APIs.  Makes sure the class handles multiple {@link Activity}s
 * requesting {@link TabModelSelector}s, {@link Activity}s getting destroyed, etc..
 */
public class TabWindowManagerTest extends InstrumentationTestCase {
    private final TabModelSelectorFactory mMockTabModelSelectorFactory =
            new TabModelSelectorFactory() {
                @Override
                public TabModelSelector buildSelector(Activity activity,
                        TabCreatorManager tabCreatorManager, int selectorIndex) {
                    return new MockTabModelSelector(0, 0, null);
                }
    };

    private ChromeActivity buildActivity() {
        ChromeActivity activity = new CustomTabActivity();
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        return activity;
    }

    private MockTabModelSelector requestSelector(ChromeActivity activity, int requestedIndex) {
        final TabWindowManager manager = TabWindowManager.getInstance();
        manager.setTabModelSelectorFactory(mMockTabModelSelectorFactory);
        return (MockTabModelSelector) manager.requestSelector(activity, activity, requestedIndex);
    }

    /**
     * Test that a single {@link Activity} can request a {@link TabModelSelector}.
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testSingleActivity() {
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);

        assertNotNull("Was not able to build the TabModelSelector", selector0);
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
    }

    /**
     * Test that two {@link Activity}s can request different {@link TabModelSelector}s.
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testMultipleActivities() {
        assertTrue("Not enough selectors", TabWindowManager.MAX_SIMULTANEOUS_SELECTORS >= 2);
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);
        TabModelSelector selector1 = requestSelector(activity1, 1);

        assertNotNull("Was not able to build the TabModelSelector", selector0);
        assertNotNull("Was not able to build the TabModelSelector", selector1);
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that trying to have too many {@link Activity}s requesting {@link TabModelSelector}s is
     * properly capped and returns {@code null}.
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testTooManyActivities() {
        for (int i = 0; i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
            assertNotNull("Could not build selector", requestSelector(buildActivity(), 0));
        }

        assertNull("Built selectors past the max number supported",
                requestSelector(buildActivity(), 0));
    }

    /**
     * Test that requesting the same {@link TabModelSelector} index will fall back and return a
     * model for a different available index instead.  In this case, a higher index (0 -> 1).
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testIndexFallback() {
        assertTrue("Not enough selectors", TabWindowManager.MAX_SIMULTANEOUS_SELECTORS >= 2);

        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);
        // Request 0 again, but should get 1 instead.
        TabModelSelector selector1 = requestSelector(activity1, 0);

        assertNotNull("Was not able to build the TabModelSelector", selector0);
        assertNotNull("Was not able to build the TabModelSelector", selector1);
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that requesting the same {@link TabModelSelector} index will fall back and return a
     * model for a different available index instead.  In this case, a lower index (2 -> 0).
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testIndexFallback2() {
        assertTrue("Not enough selectors", TabWindowManager.MAX_SIMULTANEOUS_SELECTORS >= 3);

        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 2);
        // Request 2 again, but should get 0 instead.
        TabModelSelector selector1 = requestSelector(activity1, 2);

        assertNotNull("Was not able to build the TabModelSelector", selector0);
        assertNotNull("Was not able to build the TabModelSelector", selector1);
        assertEquals("Unexpected model index", 2, manager.getIndexForWindow(activity0));
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that a destroyed {@link Activity} properly gets removed from {@link TabWindowManager}.
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testActivityDeathRemovesSingle() {
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);

        assertNotNull("Was not able to build the TabModelSelector", selector0);
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));

        ApplicationStatus.onStateChangeForTesting(activity0, ActivityState.DESTROYED);

        assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                manager.getIndexForWindow(activity0));
    }

    /**
     * Test that an {@link Activity} requesting an index that was previously assigned to a destroyed
     * {@link Activity} can take that {@link TabModelSelector}.
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testActivityDeathLetsModelReassign() {
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);

        assertNotNull("Was not able to build the TabModelSelector", selector0);
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));

        ApplicationStatus.onStateChangeForTesting(activity0, ActivityState.DESTROYED);

        assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                manager.getIndexForWindow(activity0));

        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector1 = requestSelector(activity1, 0);

        assertNotNull("Was not able to build the TabModelSelector", selector1);
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that an {@link Activity} requesting an index that was previously assigned to a destroyed
     * {@link Activity} can take that {@link TabModelSelector} when there are other
     * {@link Activity}s assigned {@link TabModelSelector}s.
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testActivityDeathWithMultipleActivities() {
        assertTrue("Not enough selectors", TabWindowManager.MAX_SIMULTANEOUS_SELECTORS >= 2);

        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);
        TabModelSelector selector1 = requestSelector(activity1, 1);

        assertNotNull("Was not able to build the TabModelSelector", selector0);
        assertNotNull("Was not able to build the TabModelSelector", selector1);
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity1));

        ApplicationStatus.onStateChangeForTesting(activity1, ActivityState.DESTROYED);

        assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                manager.getIndexForWindow(activity1));

        ChromeActivity activity2 = buildActivity();
        TabModelSelector selector2 = requestSelector(activity2, 1);

        assertNotNull("Was not able to build the TabModelSelector", selector2);
        assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity2));
    }

    /**
     * Tests that tabExistsInAnySelector() functions properly.
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testTabExistsInAnySelector() {
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        MockTabModelSelector selector0 = requestSelector(activity0, 0);
        MockTabModelSelector selector1 = requestSelector(activity1, 1);
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        assertFalse(manager.tabExistsInAnySelector(tab1.getId() - 1));
        assertTrue(manager.tabExistsInAnySelector(tab1.getId()));
        assertTrue(manager.tabExistsInAnySelector(tab2.getId()));
        assertFalse(manager.tabExistsInAnySelector(tab2.getId() + 1));

        AsyncTabParamsManager.getAsyncTabParams().clear();
        final int asyncTabId = 123;
        final TabReparentingParams dummyParams =
                new TabReparentingParams(new Tab(0, false, null), null, null);
        assertFalse(manager.tabExistsInAnySelector(asyncTabId));
        AsyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            assertTrue(manager.tabExistsInAnySelector(asyncTabId));
        } finally {
            AsyncTabParamsManager.getAsyncTabParams().clear();
        }
    }

    /**
     * Tests that getTabById() functions properly.
     */
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testGetTabById() {
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        MockTabModelSelector selector0 = requestSelector(activity0, 0);
        MockTabModelSelector selector1 = requestSelector(activity1, 1);
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        assertNull(manager.getTabById(tab1.getId() - 1));
        assertNotNull(manager.getTabById(tab1.getId()));
        assertNotNull(manager.getTabById(tab2.getId()));
        assertNull(manager.getTabById(tab2.getId() + 1));

        AsyncTabParamsManager.getAsyncTabParams().clear();
        final int asyncTabId = 123;
        final TabReparentingParams dummyParams =
                new TabReparentingParams(new Tab(0, false, null), null, null);
        assertNull(manager.getTabById(asyncTabId));
        AsyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            assertNotNull(manager.getTabById(asyncTabId));
        } finally {
            AsyncTabParamsManager.getAsyncTabParams().clear();
        }
    }
}
