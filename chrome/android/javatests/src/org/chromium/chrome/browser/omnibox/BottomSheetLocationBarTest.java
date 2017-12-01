// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.isEmptyString;
import static org.hamcrest.Matchers.not;

import android.annotation.SuppressLint;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.widget.ImageButton;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.parameter.CommandLineParameter;
import org.chromium.base.test.util.parameter.SkipCommandLineParameterization;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for {@link LocationBarLayout} UI component in the home sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
@CommandLineParameter({"", "disable-features=" + ChromeFeatureList.SPANNABLE_INLINE_AUTOCOMPLETE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
@SuppressLint("SetTextI18n")
public class BottomSheetLocationBarTest {
    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    private EmbeddedTestServer mTestServer;

    private static final String THEME_COLOR_TEST_PAGE =
            "/chrome/test/data/android/theme_color_test.html";

    private final CallbackHelper mDidThemeColorChangedCallbackHelper = new CallbackHelper();
    private final CallbackHelper mOnSSLStateUpdatedCallbackHelper = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        new TabModelSelectorTabObserver(mBottomSheetTestRule.getActivity().getTabModelSelector()) {
            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                mDidThemeColorChangedCallbackHelper.notifyCalled();
            }
            @Override
            public void onSSLStateUpdated(Tab tab) {
                mOnSSLStateUpdatedCallbackHelper.notifyCalled();
            }
        };
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Test whether the contents of the location bar are correct for HTTPS scheme.
     */
    @Test
    @SmallTest
    @SkipCommandLineParameterization
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CHROME_HOME_CLEAR_URL_ON_OPEN})
    public void testHttpsLocationBar() throws Exception {
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);

        final String testHttpsUrl = mTestServer.getURL(THEME_COLOR_TEST_PAGE);

        mBottomSheetTestRule.loadUrl(testHttpsUrl);
        mDidThemeColorChangedCallbackHelper.waitForCallback(0);
        mOnSSLStateUpdatedCallbackHelper.waitForCallback(0);

        LocationBarLayout locationBarLayout =
                (LocationBarLayout) mBottomSheetTestRule.getActivity().findViewById(
                        R.id.location_bar);
        ImageButton securityButton =
                (ImageButton) mBottomSheetTestRule.getActivity().findViewById(R.id.security_button);

        Assert.assertTrue(
                "Omnibox should have a security icon", isSecurityButtonShown(locationBarLayout));
        Assert.assertEquals("security_button with wrong resource-id", R.id.security_button,
                securityButton.getId());

        Assert.assertTrue(shouldEmphasizeHttpsScheme(locationBarLayout));

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_HALF, false);

        Assert.assertFalse("Omnibox should not have a security icon",
                isSecurityButtonShown(locationBarLayout));
        Assert.assertThat("Url bar text should be empty.", getLocationBarText(locationBarLayout),
                isEmptyString());

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);

        Assert.assertTrue(
                "Omnibox should have a security icon", isSecurityButtonShown(locationBarLayout));
        Assert.assertThat("Url bar text should not be empty.",
                getLocationBarText(locationBarLayout), is(not(isEmptyString())));
    }

    private boolean isSecurityButtonShown(LocationBarLayout locationBar) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return locationBar.isSecurityButtonShown();
            }
        });
    }

    private String getLocationBarText(LocationBarLayout locationBar) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return locationBar.mUrlBar.getText().toString();
            }
        });
    }

    private boolean shouldEmphasizeHttpsScheme(LocationBarLayout locationBar)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return locationBar.shouldEmphasizeHttpsScheme();
            }
        });
    }
}
