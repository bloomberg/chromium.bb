// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.compositor.layouts.DisableChromeAnimations;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;

/**
 * Capture the New Tab Page UI for UX review.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
public class NewTabPageUiCaptureTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();
    @Rule
    public TestRule mCreateSuggestions =
            new SuggestionsDependenciesRule(NtpUiCaptureTestData.createFactory());
    @Rule
    public TestRule mDisableChromeAnimations = new DisableChromeAnimations();

    private NewTabPage mNtp;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        // TODO(aberent): this sequence or similar is used in a number of tests, extract to common
        // test method?
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        Assert.assertTrue(tab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) tab.getNativePage();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "UiCatalogue"})
    @CommandLineFlags.Add({
        "disable-features=" + ChromeFeatureList.CHROME_HOME_PROMO,
    })
    @ScreenShooter.Directory("New Tab Page")
    public void testCaptureNewTabPage() {
        mScreenShooter.shoot("New Tab Page");

        // Scroll to search bar
        final NewTabPageRecyclerView recyclerView = mNtp.getNewTabPageView().getRecyclerView();

        final View fakebox = mNtp.getView().findViewById(org.chromium.chrome.R.id.search_box);
        final int firstScrollHeight = fakebox.getTop() + fakebox.getPaddingTop();
        final int subsequentScrollHeight = mNtp.getView().getHeight()
                - mActivityTestRule.getActivity().getToolbarManager().getToolbar().getHeight();

        ThreadUtils.runOnUiThreadBlocking(() -> { recyclerView.scrollBy(0, firstScrollHeight); });
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mScreenShooter.shoot("New Tab Page scrolled");

        ThreadUtils.runOnUiThreadBlocking(
                () -> { recyclerView.scrollBy(0, subsequentScrollHeight); });
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mScreenShooter.shoot("New Tab Page scrolled twice");

        ThreadUtils.runOnUiThreadBlocking(
                () -> { recyclerView.scrollBy(0, subsequentScrollHeight); });
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mScreenShooter.shoot("New Tab Page scrolled thrice");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "UiCatalogue"})
    @CommandLineFlags.Add({
        "enable-features=" + ChromeFeatureList.CHROME_HOME_PROMO,
    })
    @ScreenShooter.Directory("New Tab Page")
    public void testCaptureNewTabPageWithChromeHomePromo() {
        Assert.assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_HOME_PROMO));
        mScreenShooter.shoot("New Tab Page with Chrome Home Promo");
    }
}
