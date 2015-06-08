// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.graphics.Color;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import com.google.android.apps.chrome.R;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.DocumentActivityTestBase;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.browser.InterstitialPageDelegateAndroid;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Contains tests for the brand color feature.
 */
@DisableInTabbedMode
public class BrandColorTest extends DocumentActivityTestBase {
    private static final String BRAND_COLOR_1 = "#482329";
    private static final String BRAND_COLOR_2 = "#505050";
    private static final String INTERSTITIAL_HTML = "<html><head></head><body>test</body></html>";

    private ToolbarPhone mToolbar;
    private ToolbarDataProvider mToolbarDataProvider;

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

    private void checkForBrandColor(final int brandColor) {
        checkNoColorTransition();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertEquals("The data provider doesn't contain the right color",
                        brandColor, mToolbarDataProvider.getPrimaryColor());
                assertEquals("The toolbar view doesn't contain the right color",
                        brandColor, mToolbar.getBackgroundDrawable().getColor());
                assertEquals("The overlay drawable doesn't contain the right color",
                        brandColor, mToolbar.getOverlayDrawable().getColor());
            }
        });
    }

    private void checkNoColorTransition() {
        try {
            CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mToolbarDataProvider.getPrimaryColor()
                            == mToolbar.getBackgroundDrawable().getColor();
                }
            });
        } catch (InterruptedException e) {
            fail();
        }
    }

    @Override
    protected void startMainActivityWithURL(String url) throws InterruptedException {
        super.startMainActivityWithURL(url);
        mToolbar = (ToolbarPhone) getActivity().findViewById(R.id.toolbar);
        mToolbarDataProvider = mToolbar.getToolbarDataProvider();
    }

    /**
     * Test for having default primary color working correctly.
     */
    @SmallTest
    @Feature({"Omnibox"})
    public void testNoBrandColor() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(""));
        checkForBrandColor(getActivity().getResources().getColor(R.color.default_primary_color));
    }

    /**
     * Test for adding a brand color for a url.
     */
    @SmallTest
    @Feature({"Omnibox"})
    public void testBrandColorNoAlpha() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
    }

    /**
     * Test to make sure onLoadStarted doesn't reset the brand color.
     */
    @SmallTest
    @Feature({"Omnibox"})
    public void testBrandColorWithLoadStarted() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().getActivityTab()
                        .getChromeWebContentsDelegateAndroid().onLoadStarted();
            }
        });
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
    }

    /**
     * Test for checking navigating to new brand color updates correctly.
     *
     * Bug: http://crbug.com/474414
     * @SmallTest
     * @Feature({"Omnibox"})
     */
    @FlakyTest
    public void testNavigatingToNewBrandColor() throws InterruptedException {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        loadUrl(getUrlWithBrandColor(BRAND_COLOR_2));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_2));
    }

    /**
     * Test for interstitial page loads resetting brand color.
     * http://crbug.com/497866
     * @SmallTest
     * @Feature({"Omnibox"})
     */
    @FlakyTest
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
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return getActivity().getActivityTab().isShowingInterstitialPage();
                    }
                });
            }
        }));
        checkForBrandColor(getActivity().getResources().getColor(R.color.default_primary_color));
    }
}
