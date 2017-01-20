// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate.MINIMUM_SHOW_DURATION_MS;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for the BrowserStateBrowserControlsVisibilityDelegate.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BrowserStateBrowserControlsVisibilityDelegateTest {
    @Mock private Runnable mCallback;

    private BrowserStateBrowserControlsVisibilityDelegate mDelegate;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);

        mDelegate = new BrowserStateBrowserControlsVisibilityDelegate(mCallback);
    }

    private void advanceTime(long amount) {
        ShadowSystemClock.setCurrentTimeMillis(ShadowSystemClock.elapsedRealtime() + amount);
    }

    @Test
    @Feature("Fullscreen")
    public void testTransientShow() {
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());
        mDelegate.showControlsTransient();
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());

        verify(mCallback, times(2)).run();
    }

    @Test
    @Feature("Fullscreen")
    public void testShowPersistentTokenWithDelayedHide() {
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());
        int token = mDelegate.showControlsPersistent();
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());
        // Advance the clock to exceed the minimum show time.
        advanceTime(2 * MINIMUM_SHOW_DURATION_MS);
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());
        mDelegate.hideControlsPersistent(token);
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());

        verify(mCallback, times(2)).run();
    }

    @Test
    @Feature("Fullscreen")
    public void testShowPersistentTokenWithImmediateHide() {
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());
        int token = mDelegate.showControlsPersistent();
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());
        mDelegate.hideControlsPersistent(token);

        // If the controls are not shown for the mimimum allowed time, then a task is posted to
        // keep them shown for longer.  Ensure the controls can not be hidden until this delayed
        // task has been run.
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());

        verify(mCallback, times(2)).run();
    }

    @Test
    @Feature("Fullscreen")
    public void testShowPersistentBeyondRequiredMinDurationAndShowTransient() {
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());
        int token = mDelegate.showControlsPersistent();
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());

        // Advance the clock to exceed the minimum show time.
        advanceTime(2 * MINIMUM_SHOW_DURATION_MS);
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());
        // At this point, the controls have been shown long enough that the transient request will
        // be a no-op.
        mDelegate.showControlsTransient();
        mDelegate.hideControlsPersistent(token);
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());

        verify(mCallback, times(2)).run();
    }

    @Test
    @Feature("Fullscreen")
    public void testShowPersistentBelowRequiredMinDurationAndShowTransient() {
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());
        int token = mDelegate.showControlsPersistent();
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());

        // Advance the clock but not beyond the min show duration.
        advanceTime((long) (0.5 * MINIMUM_SHOW_DURATION_MS));
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());
        // At this point, the controls have not been shown long enough, so the transient request
        // will delay the ability to hide.
        mDelegate.showControlsTransient();
        mDelegate.hideControlsPersistent(token);
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());

        // Run the pending tasks on the UI thread, which will include the transient delayed task.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());

        verify(mCallback, times(2)).run();
    }

    @Test
    @Feature("Fullscreen")
    public void testShowPersistentMultipleTimes() {
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());
        int firstToken = mDelegate.showControlsPersistent();
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());

        int secondToken = mDelegate.showControlsPersistent();
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());

        int thirdToken = mDelegate.showControlsPersistent();
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());

        // Advance the clock to exceed the minimum show time.
        advanceTime(2 * MINIMUM_SHOW_DURATION_MS);
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());

        mDelegate.hideControlsPersistent(secondToken);
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());
        mDelegate.hideControlsPersistent(firstToken);
        assertFalse(mDelegate.isHidingBrowserControlsEnabled());
        mDelegate.hideControlsPersistent(thirdToken);
        assertTrue(mDelegate.isHidingBrowserControlsEnabled());

        verify(mCallback, times(2)).run();
    }
}
