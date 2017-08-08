// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.ArrayList;
import java.util.List;

/**
 * Instrumentation tests for the {@link TileGridLayout} on the New Tab Page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
@RetryOnFailure
public class TileGridLayoutTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final String HOME_PAGE_URL = "http://ho.me/";

    private static final String[] FAKE_MOST_VISITED_URLS = new String[] {
            "/chrome/test/data/android/navigate/one.html",
            "/chrome/test/data/android/navigate/two.html",
            "/chrome/test/data/android/navigate/three.html",
            "/chrome/test/data/android/navigate/four.html",
            "/chrome/test/data/android/navigate/five.html",
            "/chrome/test/data/android/navigate/six.html",
            "/chrome/test/data/android/navigate/seven.html",
            "/chrome/test/data/android/navigate/eight.html",
            "/chrome/test/data/android/navigate/nine.html",
    };

    private NewTabPage mNtp;

    @Before
    public void setUp() throws Exception {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();
        for (String url : FAKE_MOST_VISITED_URLS) {
            siteSuggestions.add(FakeMostVisitedSites.createSiteSuggestion(
                    mTestServerRule.getServer().getURL(url)));
        }
        siteSuggestions.add(new SiteSuggestion("HOMEPAGE", HOME_PAGE_URL, "", TileSource.HOMEPAGE));

        FakeMostVisitedSites mostVisitedSites = new FakeMostVisitedSites();
        mostVisitedSites.setTileSuggestions(siteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mostVisitedSites;

        mSuggestionsDeps.getFactory().suggestionsSource = new FakeSuggestionsSource();

        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        Tab mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();

        RecyclerViewTestUtils.waitForStableRecyclerView(getRecyclerView());
    }

    /**
     * Verifies that the assumed constants in {@link TileGridLayout#calculateNumColumns()}
     * are correct.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testNumColumnsPaddingAndMarginSizes() {
        TileGridLayout tileGridLayout = getTileGridLayout();
        Assert.assertEquals(TileGridLayout.PADDING_START_PX,
                ApiCompatibilityUtils.getPaddingStart(tileGridLayout));
        Assert.assertEquals(
                TileGridLayout.PADDING_END_PX, ApiCompatibilityUtils.getPaddingEnd(tileGridLayout));

        DisplayMetrics metrics = tileGridLayout.getResources().getDisplayMetrics();
        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) tileGridLayout.getLayoutParams();
        Assert.assertEquals(convertToPx(TileGridLayout.MARGIN_START_DP, metrics),
                ApiCompatibilityUtils.getMarginStart(layoutParams));
        Assert.assertEquals(convertToPx(TileGridLayout.MARGIN_END_DP, metrics),
                ApiCompatibilityUtils.getMarginEnd(layoutParams));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testNumColumnsCalculatedEqualToMeasurement() {
        final int maxColumns = 6;
        final TileGridLayout layout = getTileGridLayout();
        layout.setMaxRows(1);
        layout.setMaxColumns(maxColumns);

        reMeasureLayout(layout);
        int calculatedNumColumns = Math.min(maxColumns, TileGridLayout.calculateNumColumns());

        final Activity activity = mActivityTestRule.getActivity();
        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        int portraitVisibleTiles = countVisibleTiles(layout, calculatedNumColumns);
        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        int landscapeVisibleTiles = countVisibleTiles(layout, calculatedNumColumns);
        Assert.assertEquals("The calculated number of columns should be equal to the minimum number"
                        + " of columns across screen orientations.",
                Math.min(portraitVisibleTiles, landscapeVisibleTiles), calculatedNumColumns);
    }

    private int convertToPx(int dp, DisplayMetrics metrics) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dp, metrics));
    }

    private void reMeasureLayout(final ViewGroup group) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                group.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
            }
        });

        // Wait until the view is updated. TODO(oskopek): Is there a better criterion to check for?
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                boolean hasHiddenTile = false;
                for (int i = 0; i < group.getChildCount(); i++) {
                    if (!hasHiddenTile) {
                        hasHiddenTile = group.getChildAt(i).getVisibility() == View.GONE;
                    }
                }

                return hasHiddenTile;
            }
        });
    }

    private int countVisibleTiles(ViewGroup group, int calculatedNumColumns) {
        reMeasureLayout(group);
        int visibleTileViews = 0;
        boolean hasHomePage = false;
        for (int i = 0; i < group.getChildCount(); i++) {
            TileView tileView = (TileView) group.getChildAt(i);
            if (tileView.getVisibility() == View.VISIBLE) {
                visibleTileViews++;
                if (HOME_PAGE_URL.equals(tileView.getUrl())) {
                    hasHomePage = true;
                }
            }
        }

        Assert.assertTrue("The calculated number of columns should be less than or equal to"
                        + "the measured one.",
                calculatedNumColumns <= visibleTileViews);
        Assert.assertTrue("Visible tiles should contain the home page tile.", hasHomePage);

        return visibleTileViews;
    }

    private NewTabPageRecyclerView getRecyclerView() {
        return mNtp.getNewTabPageView().getRecyclerView();
    }

    private TileGridLayout getTileGridLayout() {
        TileGridLayout tileGridLayout =
                (TileGridLayout) mNtp.getNewTabPageView().findViewById(R.id.tile_grid_layout);
        Assert.assertNotNull("Unable to retrieve the TileGridLayout.", tileGridLayout);
        return tileGridLayout;
    }
}
