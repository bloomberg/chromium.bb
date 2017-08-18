// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.test.BottomSheetTestRule.ENABLE_CHROME_HOME;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_PHONE;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.ScreenShooter;
import org.chromium.base.test.util.parameter.CommandLineParameter;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NtpUiCaptureTestData;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;

/**
 * Tests for the appearance of the tiles in the home sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
// TODO(https://crbug.com/754778) improve annotation processor. We need to remove the currently
// registered Feature flags to be able to change them later.
@CommandLineFlags.Remove(ENABLE_CHROME_HOME)
@ScreenShooter.Directory("HomeSheetTiles")
public class HomeSheetTilesUiCaptureTest {
    @Rule
    public BottomSheetTestRule mActivityTestRule = new BottomSheetTestRule();

    @Rule
    public SuggestionsDependenciesRule setupSuggestions() {
        SuggestionsDependenciesRule.TestFactory depsFactory = NtpUiCaptureTestData.createFactory();
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        NtpUiCaptureTestData.registerArticleSamples(suggestionsSource);
        depsFactory.suggestionsSource = suggestionsSource;
        return new SuggestionsDependenciesRule(depsFactory);
    }

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    @Before
    public void setup() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    @CommandLineParameter({ENABLE_CHROME_HOME,
            "enable-features=" + ChromeFeatureList.CHROME_HOME_MODERN_LAYOUT + ","
                    + ChromeFeatureList.CHROME_HOME})
    @ScreenShooter.Directory("Tiles")
    public void testTiles() {
        setSheetState(BottomSheet.SHEET_STATE_FULL);
        mScreenShooter.shoot(
                "Tiles" + (FeatureUtilities.isChromeHomeModernEnabled() ? "_modern" : ""));
    }

    private void setSheetState(final int position) {
        mActivityTestRule.setSheetState(position, false);
        waitForWindowUpdates();
    }

    /** Wait for update to start and finish. */
    private static void waitForWindowUpdates() {
        final long maxWindowUpdateTimeMs = scaleTimeout(1000);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.waitForWindowUpdate(null, maxWindowUpdateTimeMs);
        device.waitForIdle(maxWindowUpdateTimeMs);
    }
}
