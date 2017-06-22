// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_PHONE;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.action.ViewActions;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.ScreenShooter;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.SuggestionsBottomSheetTestRule;

/**
 * Tests for the appearance of Article Snippets.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class SuggestionsBottomSheetUiCaptureTest {
    @Rule
    public SuggestionsBottomSheetTestRule mSuggestionsTestRule =
            new SuggestionsBottomSheetTestRule();

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    private static final int MAX_WINDOW_UPDATE_TIME_MS = 1000;

    @Before
    public void setup() throws InterruptedException {
        mSuggestionsTestRule.startMainActivityOnBlankPage();
    }

    private void setSheetState(final int position) {
        mSuggestionsTestRule.setSheetState(position, false);
        waitForWindowUpdates();
    }

    private void waitForWindowUpdates() {
        // Wait for update to start and finish.
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.waitForWindowUpdate(null, MAX_WINDOW_UPDATE_TIME_MS);
        device.waitForIdle(MAX_WINDOW_UPDATE_TIME_MS);
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @ScreenShooter.Directory("Suggestions Bottom Sheet Position")
    public void testBottomSheetPosition() throws Exception {
        setSheetState(BottomSheet.SHEET_STATE_HALF);
        mScreenShooter.shoot("Half");
        setSheetState(BottomSheet.SHEET_STATE_FULL);
        mScreenShooter.shoot("Full");
        setSheetState(BottomSheet.SHEET_STATE_PEEK);
        mScreenShooter.shoot("Peek");
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @ScreenShooter.Directory("Suggestions Context Menu")
    public void testContextMenu() throws Exception {
        // Needs to be "Full" to for this to work on small screens in landscape.
        setSheetState(BottomSheet.SHEET_STATE_FULL);
        Espresso.onView(ViewMatchers.withId(R.id.recycler_view))
                .perform(RecyclerViewActions.actionOnItemAtPosition(2, ViewActions.longClick()));
        waitForWindowUpdates();
        mScreenShooter.shoot("Context_menu");
    }
}
