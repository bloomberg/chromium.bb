// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_PHONE;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.registerCategory;

import android.app.Instrumentation;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.action.ViewActions;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.ScreenShooter;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;

/**
 * Tests for the appearance of Article Snippets.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-features=ChromeHome", ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
@Restriction(RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class SuggestionsBottomSheetUiCaptureTest {
    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    private static final int MAX_WINDOW_UPDATE_TIME_MS = 1000;

    private Instrumentation mInstrumentation;

    private ChromeTabbedActivity mActivity;

    private boolean mOldChromeHomeFlagValue;

    private UiDevice mDevice;

    @Before
    public void setup() throws InterruptedException {
        // TODO(dgn,mdjones): Chrome restarts when the ChromeHome feature flag value changes. That
        // crashes the test so we need to manually set the preference to match the flag before
        // staring Chrome.
        ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();
        mOldChromeHomeFlagValue = prefManager.isChromeHomeEnabled();
        prefManager.setChromeHomeEnabled(true);
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        registerCategory(suggestionsSource, /* category = */ 42, /* count = */ 5);
        SuggestionsBottomSheetContent.setSuggestionsSourceForTesting(suggestionsSource);
        SuggestionsBottomSheetContent.setEventReporterForTesting(
                new DummySuggestionsEventReporter());
        mActivityTestRule.startMainActivityOnBlankPage();
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mActivity = mActivityTestRule.getActivity();
        mDevice = UiDevice.getInstance(mInstrumentation);
    }

    @After
    public void tearDown() throws Exception {
        SuggestionsBottomSheetContent.setSuggestionsSourceForTesting(null);
        SuggestionsBottomSheetContent.setEventReporterForTesting(null);
        ChromePreferenceManager.getInstance().setChromeHomeEnabled(mOldChromeHomeFlagValue);
    }

    private void setBottomSheetPosition(final int position) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                BottomSheet bottomSheet = mActivity.getBottomSheet();
                bottomSheet.setSheetState(position, /* animate = */ false);
            }
        });
        // Wait for update to start and finish.
        mDevice.waitForWindowUpdate(null, MAX_WINDOW_UPDATE_TIME_MS);
        mDevice.waitForIdle(MAX_WINDOW_UPDATE_TIME_MS);
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    public void testBottomSheetPosition() throws Exception {
        setBottomSheetPosition(BottomSheet.SHEET_STATE_HALF);
        mScreenShooter.shoot("Half");
        setBottomSheetPosition(BottomSheet.SHEET_STATE_FULL);
        mScreenShooter.shoot("Full");
        setBottomSheetPosition(BottomSheet.SHEET_STATE_PEEK);
        mScreenShooter.shoot("Peek");
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    public void testContextMenu() throws Exception {
        // Needs to be "Full" to for this to work on small screens in landscape.
        setBottomSheetPosition(BottomSheet.SHEET_STATE_FULL);
        Espresso.onView(ViewMatchers.withId(R.id.recycler_view))
                .perform(RecyclerViewActions.actionOnItemAtPosition(2, ViewActions.longClick()));
        mDevice.waitForWindowUpdate(null, MAX_WINDOW_UPDATE_TIME_MS);
        mDevice.waitForIdle(MAX_WINDOW_UPDATE_TIME_MS);
        mScreenShooter.shoot("Context_menu");
    }
}
