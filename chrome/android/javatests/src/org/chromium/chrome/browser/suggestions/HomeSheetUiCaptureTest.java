// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.test.BottomSheetTestRule.ENABLE_CHROME_HOME;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_PHONE;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;
import android.support.v7.widget.RecyclerView;

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
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NtpUiCaptureTestData;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;

/**
 * Tests for the appearance of the special states of the home sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
// TODO(https://crbug.com/754778) improve annotation processor. We need to remove the currently
// registered Feature flags to be able to change them later.
@CommandLineFlags.Remove(ENABLE_CHROME_HOME)
@ScreenShooter.Directory("HomeSheetStates")
public class HomeSheetUiCaptureTest {
    @Rule
    public BottomSheetTestRule mActivityTestRule = new BottomSheetTestRule();

    private FakeSuggestionsSource mSuggestionsSource;

    @Rule
    public SuggestionsDependenciesRule setupSuggestions() {
        SuggestionsDependenciesRule.TestFactory depsFactory = NtpUiCaptureTestData.createFactory();
        mSuggestionsSource = new FakeSuggestionsSource();
        NtpUiCaptureTestData.registerArticleSamples(mSuggestionsSource);
        depsFactory.suggestionsSource = mSuggestionsSource;
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
    // TODO(bauerb): Parameterize this test to test without the modern layout.
    @CommandLineFlags.Add({
            "enable-features=" + ChromeFeatureList.CHROME_HOME_MODERN_LAYOUT + ","
            + ChromeFeatureList.CHROME_HOME})
    @ScreenShooter.Directory("SignInPromo")
    public void testSignInPromo() {
        // Needs to be "Full" to for this to work on small screens in landscape.
        setSheetState(BottomSheet.SHEET_STATE_FULL);

        scrollToFirstItemOfType(ItemViewType.PROMO);

        mScreenShooter.shoot(
                "SignInPromo" + (FeatureUtilities.isChromeHomeModernEnabled() ? "_modern" : ""));
    }

    @Test
    @MediumTest
    @Feature({"UiCatalogue"})
    // TODO(bauerb): Parameterize this test to test without the modern layout.
    @CommandLineFlags.Add({
            "enable-features=" + ChromeFeatureList.CHROME_HOME_MODERN_LAYOUT + ","
            + ChromeFeatureList.CHROME_HOME})
    @ScreenShooter.Directory("AllDismissed")
    public void testAllDismissed() {
        final SuggestionsRecyclerView recyclerView = getRecyclerView();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            NewTabPageAdapter newTabPageAdapter = recyclerView.getNewTabPageAdapter();
            int signInPromoPosition = newTabPageAdapter.getFirstPositionForType(ItemViewType.PROMO);
            assertNotEquals(signInPromoPosition, RecyclerView.NO_POSITION);
            newTabPageAdapter.dismissItem(signInPromoPosition, ignored -> { });

            // Dismiss all articles.
            while (true) {
                int articlePosition =
                        newTabPageAdapter.getFirstPositionForType(ItemViewType.SNIPPET);
                if (articlePosition == RecyclerView.NO_POSITION) break;
                newTabPageAdapter.dismissItem(articlePosition, ignored -> { });
            }
        });

        scrollToFirstItemOfType(ItemViewType.ALL_DISMISSED);

        mScreenShooter.shoot(
                "All_dismissed" + (FeatureUtilities.isChromeHomeModernEnabled() ? "_modern" : ""));
    }

    private void scrollToFirstItemOfType(@ItemViewType int itemViewType) {
        SuggestionsRecyclerView recyclerView = getRecyclerView();
        NewTabPageAdapter newTabPageAdapter = recyclerView.getNewTabPageAdapter();
        int position = newTabPageAdapter.getFirstPositionForType(itemViewType);
        assertNotEquals("Scroll target of type " + itemViewType + " not found\n"
                        + ContentSuggestionsTestUtils.stringify(
                                  newTabPageAdapter.getRootForTesting()),
                RecyclerView.NO_POSITION, position);

        ThreadUtils.runOnUiThreadBlocking(
                () -> recyclerView.getLinearLayoutManager().scrollToPosition(position));
        RecyclerViewTestUtils.waitForView(recyclerView, position);
    }

    private void setSheetState(final int position) {
        mActivityTestRule.setSheetState(position, false);
        waitForWindowUpdates();
    }

    private SuggestionsRecyclerView getRecyclerView() {
        SuggestionsRecyclerView recyclerView =
                mActivityTestRule.getBottomSheetContent().getContentView().findViewById(
                        R.id.recycler_view);
        assertNotNull(recyclerView);
        return recyclerView;
    }

    /** Wait for update to start and finish. */
    private static void waitForWindowUpdates() {
        final long maxWindowUpdateTimeMs = scaleTimeout(1000);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.waitForWindowUpdate(null, maxWindowUpdateTimeMs);
        device.waitForIdle(maxWindowUpdateTimeMs);
    }
}
