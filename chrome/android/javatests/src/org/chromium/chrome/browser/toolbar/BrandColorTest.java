// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.annotation.TargetApi;
import android.graphics.Color;
import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.browser.InterstitialPageDelegateAndroid;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Contains tests for the brand color feature.
 */
public class BrandColorTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public BrandColorTest() {
        super(ChromeActivity.class);
    }

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

    @Override
    public void startMainActivity() {
        // Don't launch activity automatically.
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void checkForBrandColor(final int brandColor) {
        try {
            CriteriaHelper.pollForUIThreadCriteria(new Criteria(
                    "The toolbar background doesn't contain the right color") {
                @Override
                public boolean isSatisfied() {
                    if (mToolbarDataProvider.getPrimaryColor() != brandColor) return false;
                    return mToolbarDataProvider.getPrimaryColor()
                            == mToolbar.getBackgroundDrawable().getColor();
                }
            });
            CriteriaHelper.pollForUIThreadCriteria(new Criteria(
                    "The overlay drawable doesn't contain the right color") {
                @Override
                public boolean isSatisfied() {
                    return mToolbar.getOverlayDrawable().getColor() == brandColor;
                }
            });
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                    && !SysUtils.isLowEndDevice()) {
                final int expectedStatusBarColor = brandColor == mDefaultColor
                        ? Color.BLACK
                        : ColorUtils.getDarkenedColorForStatusBar(brandColor);
                CriteriaHelper.pollForUIThreadCriteria(new Criteria(
                        "The status bar is not set to the right color") {
                    @Override
                    public boolean isSatisfied() {
                        return expectedStatusBarColor
                                == getActivity().getWindow().getStatusBarColor();
                    }
                });
            }

        } catch (InterruptedException e) {
            fail();
        }

    }

    @Override
    protected void startMainActivityWithURL(String url) throws InterruptedException {
        super.startMainActivityWithURL(url);
        mToolbar = (ToolbarPhone) getActivity().findViewById(R.id.toolbar);
        mToolbarDataProvider = mToolbar.getToolbarDataProvider();
        mDefaultColor = ApiCompatibilityUtils.getColor(getActivity().getResources(),
                R.color.default_primary_color);
    }

    /**
     * Test for having default primary color working correctly.
     */
    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testNoBrandColor() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(""));
        checkForBrandColor(mDefaultColor);
    }

    /**
     * Test for adding a brand color for a url.
     */
    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testBrandColorNoAlpha() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
    }

    /**
     * Test to make sure onLoadStarted doesn't reset the brand color.
     */
    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testBrandColorWithLoadStarted() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().getActivityTab()
                        .getTabWebContentsDelegateAndroid().onLoadStarted(true);
            }
        });
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
    }

    /**
     * Test for checking navigating to new brand color updates correctly.
     */
    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testNavigatingToNewBrandColor() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        loadUrl(getUrlWithBrandColor(BRAND_COLOR_2));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_2));
    }

    /**
     * Test for checking navigating to a brand color site from a site with no brand color and then
     * back again.
     */
    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"Omnibox"})
    public void testNavigatingToBrandColorAndBack() throws InterruptedException {
        startMainActivityWithURL("about:blank");
        checkForBrandColor(mDefaultColor);
        loadUrl(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        loadUrl("about:blank");
        checkForBrandColor(mDefaultColor);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().onBackPressed();
            }
        });
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().onBackPressed();
            }
        });
        checkForBrandColor(mDefaultColor);
    }

    /**
     * Test for interstitial page loads resetting brand color.
     *
     * TODO(aurimas): investigate why this test is crasing in tabbed mode.
     */
    @SmallTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
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
                getActivity().getActivityTab().getWebContents().showInterstitialPage(
                        brandColorUrl, delegate.getNative());
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab().isShowingInterstitialPage();
            }
        });
        checkForBrandColor(ApiCompatibilityUtils.getColor(
                getActivity().getResources(), R.color.default_primary_color));
    }
}
