// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.annotation.TargetApi;
import android.graphics.Color;
import android.os.Build;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.browser.InterstitialPageDelegateAndroid;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;

/**
 * Contains tests for the brand color feature.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class BrandColorTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String BRAND_COLOR_1 = "#482329";
    private static final String BRAND_COLOR_2 = "#505050";
    private static final String INTERSTITIAL_HTML = "<html><head></head><body>test</body></html>";

    private ToolbarPhone mToolbar;
    private ToolbarDataProvider mToolbarDataProvider;
    private int mDefaultColor;

    private static String getUrlWithBrandColor(String brandColor) {
        String brandColorMetaTag = TextUtils.isEmpty(brandColor)
                ? "" : "<meta name='theme-color' content='" + brandColor + "'>";
        return UrlUtils.encodeHtmlDataUri(
                "<html>"
                + "  <head>"
                + "    " + brandColorMetaTag
                + "  </head>"
                + "  <body>"
                + "    Theme color set to " + brandColor
                + "  </body>"
                + "</html>");
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void checkForBrandColor(final int brandColor) {
        CriteriaHelper.pollUiThread(new Criteria(
                "The toolbar background doesn't contain the right color") {
            @Override
            public boolean isSatisfied() {
                if (mToolbarDataProvider.getPrimaryColor() != brandColor) return false;
                return mToolbarDataProvider.getPrimaryColor()
                        == mToolbar.getBackgroundDrawable().getColor();
            }
        });
        CriteriaHelper.pollUiThread(
                Criteria.equals(brandColor, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return mToolbar.getOverlayDrawable().getColor();
                    }
                }));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                && !SysUtils.isLowEndDevice()) {
            final int expectedStatusBarColor = brandColor == mDefaultColor
                    ? Color.BLACK
                    : ColorUtils.getDarkenedColorForStatusBar(brandColor);
            CriteriaHelper.pollUiThread(
                    Criteria.equals(expectedStatusBarColor, new Callable<Integer>() {
                        @Override
                        public Integer call() {
                            return mActivityTestRule.getActivity().getWindow().getStatusBarColor();
                        }
                    }));
        }
    }

    protected void startMainActivityWithURL(String url) throws InterruptedException {
        mActivityTestRule.startMainActivityWithURL(url);
        mToolbar = (ToolbarPhone) mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        mToolbarDataProvider = mToolbar.getToolbarDataProvider();
        mDefaultColor = ApiCompatibilityUtils.getColor(
                mActivityTestRule.getActivity().getResources(), R.color.default_primary_color);
    }

    /**
     * Test for having default primary color working correctly.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testNoBrandColor() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(""));
        checkForBrandColor(mDefaultColor);
    }

    /**
     * Test for adding a brand color for a url.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testBrandColorNoAlpha() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
    }

    /**
     * Test for immediately setting the brand color.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testImmediateColorChange() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().getToolbarManager().updatePrimaryColor(
                        mDefaultColor, false);
                // Since the color should change instantly, there is no need to use the criteria
                // helper.
                Assert.assertEquals(mToolbarDataProvider.getPrimaryColor(),
                        mToolbar.getBackgroundDrawable().getColor());
            }
        });
    }

    /**
     * Test to make sure onLoadStarted doesn't reset the brand color.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testBrandColorWithLoadStarted() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                Tab tab = mActivityTestRule.getActivity().getActivityTab();
                RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(tab);
                while (observers.hasNext()) {
                    observers.next().onLoadStarted(tab, true);
                }
            }
        });
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
    }

    /**
     * Test for checking navigating to new brand color updates correctly.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testNavigatingToNewBrandColor() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        mActivityTestRule.loadUrl(getUrlWithBrandColor(BRAND_COLOR_2));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_2));
    }

    /**
     * Test for checking navigating to a brand color site from a site with no brand color and then
     * back again.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testNavigatingToBrandColorAndBack() throws InterruptedException {
        startMainActivityWithURL("about:blank");
        checkForBrandColor(mDefaultColor);
        mActivityTestRule.loadUrl(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        mActivityTestRule.loadUrl("about:blank");
        checkForBrandColor(mDefaultColor);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().onBackPressed();
            }
        });
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().onBackPressed();
            }
        });
        checkForBrandColor(mDefaultColor);
    }

    /**
     * Test for interstitial page loads resetting brand color.
     *
     * TODO(aurimas): investigate why this test is crasing in tabbed mode.
     */
    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableInTabbedMode
    @Feature({"Omnibox"})
    public void testBrandColorInterstitial() throws InterruptedException {
        final String brandColorUrl = getUrlWithBrandColor(BRAND_COLOR_1);
        startMainActivityWithURL(brandColorUrl);
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        final InterstitialPageDelegateAndroid delegate =
                new InterstitialPageDelegateAndroid(INTERSTITIAL_HTML);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity()
                        .getActivityTab()
                        .getWebContents()
                        .showInterstitialPage(brandColorUrl, delegate.getNative());
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getActivityTab().isShowingInterstitialPage();
            }
        });
        checkForBrandColor(ApiCompatibilityUtils.getColor(
                mActivityTestRule.getActivity().getResources(), R.color.default_primary_color));
    }
}
