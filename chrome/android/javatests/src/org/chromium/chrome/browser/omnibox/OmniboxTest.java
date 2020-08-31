// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.annotation.SuppressLint;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.KeyEvent;
import android.widget.ImageButton;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.params.SkipCommandLineParameterization;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.EnormousTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.status.StatusViewCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteResult;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/**
 * Tests of the Omnibox.
 *
 * TODO(yolandyan): Replace the ParameterizedCommandLineFlags with new JUnit4
 * parameterized framework once it supports Test Rule Parameterization.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// clang-format off
@ParameterizedCommandLineFlags({
  @Switches(),
  @Switches("disable-features=" + ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE),
})
// clang-format on
@SuppressLint("SetTextI18n")
public class OmniboxTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private void clearUrlBar() {
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        TestThreadUtils.runOnUiThreadBlocking(() -> { urlBar.setText(""); });
    }

    private static final OnSuggestionsReceivedListener sEmptySuggestionListener =
            new OnSuggestionsReceivedListener() {
                @Override
                public void onSuggestionsReceived(
                        AutocompleteResult autocompleteResult, String inlineAutocompleteText) {}
            };

    /**
     * Sanity check of Omnibox.  The problem in http://b/5021723 would
     * cause this to fail (hang or crash).
     */
    @Test
    @EnormousTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testSimpleUse() throws InterruptedException {
        mActivityTestRule.typeInOmnibox("aaaaaaa", false);

        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        ChromeTabUtils.waitForTabPageLoadStart(
                mActivityTestRule.getActivity().getActivityTab(), null, new Runnable() {
                    @Override
                    public void run() {
                        final UrlBar urlBar =
                                (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
                        KeyUtils.singleKeyEventView(InstrumentationRegistry.getInstrumentation(),
                                urlBar, KeyEvent.KEYCODE_ENTER);
                    }
                }, 20L);
    }

    // Sanity check that no text is displayed in the omnibox when on the NTP page and that the hint
    // text is correct.
    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @RetryOnFailure
    public void testDefaultText() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);

        // Omnibox on NTP shows the hint text.
        Assert.assertNotNull(urlBar);
        Assert.assertEquals("Location bar has text.", "", urlBar.getText().toString());
        Assert.assertEquals("Location bar has incorrect hint.",
                mActivityTestRule.getActivity().getResources().getString(
                        R.string.search_or_type_web_address),
                urlBar.getHint().toString());

        // Type something in the omnibox.
        // Note that the TextView does not provide a way to test if the hint is showing, the API
        // documentation simply says it shows when the text is empty.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            urlBar.requestFocus();
            urlBar.setText("G");
        });
        Assert.assertEquals("Location bar should have text.", "G", urlBar.getText().toString());
    }

    /**
     * The following test is a basic way to assess how much instant slows down typing in the
     * omnibox. It is meant to be run manually for investigation purposes.
     * When instant was enabled for all suggestions (including searched), I would get a 40% increase
     * in the average time on this test. With instant off, it was almost identical.
     * Marking the test disabled so it is not picked up by our test runner, as it is supposed to be
     * run manually.
     */
    public void manualTestTypingPerformance() throws InterruptedException {
        final String text = "searching for pizza";
        // Type 10 times something on the omnibox and get the average time with and without instant.
        long instantAverage = 0;
        long noInstantAverage = 0;

        for (int i = 0; i < 2; ++i) {
            boolean instantOn = (i == 1);
            mActivityTestRule.setNetworkPredictionEnabled(instantOn);

            for (int j = 0; j < 10; ++j) {
                long before = System.currentTimeMillis();
                mActivityTestRule.typeInOmnibox(text, true);
                if (instantOn) {
                    instantAverage += System.currentTimeMillis() - before;
                } else {
                    noInstantAverage += System.currentTimeMillis() - before;
                }
                clearUrlBar();
                InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            }
        }
        instantAverage /= 10;
        noInstantAverage /= 10;
        System.err.println("******************************************************************");
        System.err.println("**** Instant average=" + instantAverage);
        System.err.println("**** No instant average=" + noInstantAverage);
        System.err.println("******************************************************************");
    }

    /**
     * Test to verify that the security icon is present when visiting http:// URLs.
     */
    @Test
    @MediumTest
    @SkipCommandLineParameterization
    @DisableFeatures("OmniboxSearchEngineLogo")
    public void testSecurityIconOnHTTP() {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        try {
            final String testUrl = testServer.getURL("/chrome/test/data/android/omnibox/one.html");

            mActivityTestRule.loadUrl(testUrl);
            final LocationBarLayout locationBar =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            StatusViewCoordinator statusViewCoordinator =
                    locationBar.getStatusViewCoordinatorForTesting();
            boolean securityIcon = statusViewCoordinator.isSecurityButtonShown();
            if (mActivityTestRule.getActivity().isTablet()) {
                Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
                Assert.assertEquals(R.drawable.omnibox_info,
                        statusViewCoordinator.getSecurityIconResourceIdForTesting());
            } else {
                Assert.assertFalse("Omnibox should not have a Security icon", securityIcon);
            }
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * Test to verify that the security icon is present when visiting https:// URLs.
     */
    @Test
    @MediumTest
    @SkipCommandLineParameterization
    public void testSecurityIconOnHTTPS() throws Exception {
        EmbeddedTestServer httpsTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getContext(),
                ServerCertificate.CERT_OK);
        CallbackHelper onSSLStateUpdatedCallbackHelper = new CallbackHelper();
        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onSSLStateUpdated(Tab tab) {
                onSSLStateUpdatedCallbackHelper.notifyCalled();
            }
        };
        mActivityTestRule.getActivity().getActivityTab().addObserver(observer);

        try {
            final String testHttpsUrl =
                    httpsTestServer.getURL("/chrome/test/data/android/omnibox/one.html");

            ImageButton securityButton = (ImageButton) mActivityTestRule.getActivity().findViewById(
                    R.id.location_bar_status_icon);

            mActivityTestRule.loadUrl(testHttpsUrl);
            onSSLStateUpdatedCallbackHelper.waitForCallback(0);

            final LocationBarLayout locationBar =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            StatusViewCoordinator statusViewCoordinator =
                    locationBar.getStatusViewCoordinatorForTesting();
            boolean securityIcon = statusViewCoordinator.isSecurityButtonShown();
            Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
            Assert.assertEquals("location_bar_status_icon with wrong resource-id",
                    R.id.location_bar_status_icon, securityButton.getId());
            Assert.assertTrue(securityButton.isShown());
            Assert.assertEquals(R.drawable.omnibox_https_valid,
                    statusViewCoordinator.getSecurityIconResourceIdForTesting());
        } finally {
            httpsTestServer.stopAndDestroyServer();
        }
    }

    /**
     * Test whether the color of the Location bar is correct for HTTPS scheme.
     */
    @Test
    @SmallTest
    @SkipCommandLineParameterization
    public void testHttpsLocationBarColor() throws Exception {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        CallbackHelper didThemeColorChangedCallbackHelper = new CallbackHelper();
        CallbackHelper onSSLStateUpdatedCallbackHelper = new CallbackHelper();
        new TabModelSelectorTabObserver(mActivityTestRule.getActivity().getTabModelSelector()) {
            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                didThemeColorChangedCallbackHelper.notifyCalled();
            }
            @Override
            public void onSSLStateUpdated(Tab tab) {
                onSSLStateUpdatedCallbackHelper.notifyCalled();
            }
        };

        try {
            final String testHttpsUrl =
                    testServer.getURL("/chrome/test/data/android/theme_color_test.html");

            mActivityTestRule.loadUrl(testHttpsUrl);

            // Tablets don't have website theme colors.
            if (!mActivityTestRule.getActivity().isTablet()) {
                didThemeColorChangedCallbackHelper.waitForCallback(0);
            }

            onSSLStateUpdatedCallbackHelper.waitForCallback(0);

            LocationBarLayout locationBarLayout =
                    (LocationBarLayout) mActivityTestRule.getActivity().findViewById(
                            R.id.location_bar);
            ImageButton securityButton = (ImageButton) mActivityTestRule.getActivity().findViewById(
                    R.id.location_bar_status_icon);

            boolean securityIcon =
                    locationBarLayout.getStatusViewCoordinatorForTesting().isSecurityButtonShown();
            Assert.assertTrue("Omnibox should have a Security icon", securityIcon);
            Assert.assertEquals("location_bar_status_icon with wrong resource-id",
                    R.id.location_bar_status_icon, securityButton.getId());

            if (mActivityTestRule.getActivity().isTablet()) {
                Assert.assertTrue(mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getLocationBarModelForTesting()
                                          .shouldEmphasizeHttpsScheme());
            } else {
                Assert.assertFalse(mActivityTestRule.getActivity()
                                           .getToolbarManager()
                                           .getLocationBarModelForTesting()
                                           .shouldEmphasizeHttpsScheme());
            }
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        if (mActivityTestRule.getName().equals("testsplitPathFromUrlDisplayText")
                || mActivityTestRule.getName().equals("testDefaultText")) {
            return;
        }
        mActivityTestRule.startMainActivityOnBlankPage();
    }
}
