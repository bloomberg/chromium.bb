// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Navigate in UrlBar for BottomSheet tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class BottomSheetNavigateTest {
    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    private static final String HTTP_SCHEME = "http://";
    private static final String NEW_TAB_PAGE = "chrome-native://newtab/";

    private BottomSheet mBottomSheet;
    private ChromeTabbedActivity mActivity;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        mBottomSheet = mBottomSheetTestRule.getBottomSheet();
        mActivity = mBottomSheetTestRule.getActivity();
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Focuses the UrlBar to bring up the BottomSheet and types the passed text in the omnibox to
     * trigger a navigation. You can pass a URL or a search term. This code triggers suggestions and
     * prerendering; unless you are testing these features specifically, you should use loadUrl()
     * which is less prone to flakyness.
     *
     * @param url The URL to navigate to.
     * @param expectedTitle Title that the page is expected to have.  Shouldn't be set if the page
     *                      load causes a redirect.
     * @return the URL in the omnibox.
     */
    private String typeInOmniboxAndNavigate(final String url, final String expectedTitle)
            throws Exception {
        // Focus URL bar.
        final UrlBar urlBar = (UrlBar) mActivity.findViewById(R.id.url_bar);
        Assert.assertNotNull("urlBar is null", urlBar);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            urlBar.requestFocus();
            mBottomSheet.endAnimations();
        });

        Assert.assertEquals("Bottom sheet should be expanded.", BottomSheet.SHEET_STATE_FULL,
                mBottomSheet.getSheetState());

        // Navigate to the URL.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            urlBar.setText(url);
            mBottomSheet.endAnimations();
        });
        final LocationBarLayout locationBar =
                (LocationBarLayout) mActivity.findViewById(R.id.location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);

        TabLoadObserver observer =
                new TabLoadObserver(mActivity.getActivityTab(), expectedTitle, null);
        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), urlBar, KeyEvent.KEYCODE_ENTER);
        observer.assertLoaded();

        Assert.assertEquals("Bottom sheet should be closed.", BottomSheet.SHEET_STATE_PEEK,
                mBottomSheet.getSheetState());

        // The URL has been set before the page notification was broadcast, so it is safe to access.
        return urlBar.getText().toString();
    }

    /**
     * @return the expected contents of the location bar after navigating to url.
     */
    private String expectedLocation(String url) {
        Assert.assertTrue(url.startsWith(HTTP_SCHEME));
        return url.replaceFirst(HTTP_SCHEME, "");
    }

    /**
     * Perform navigation by typing into omnibox after bringing up BottomSheet.
     */
    @Test
    @MediumTest
    @Feature({"Navigation", "Main"})
    public void testNavigate() throws Exception {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        String result = typeInOmniboxAndNavigate(url, "Simple");
        Assert.assertEquals(expectedLocation(url), result);
    }
}
