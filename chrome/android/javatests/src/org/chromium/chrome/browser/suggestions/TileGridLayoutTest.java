// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

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
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
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

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

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

    private static final String[] FAKE_MOST_VISITED_TITLES =
            new String[] {"ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"};

    private NewTabPage mNtp;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testHomePageIsMovedToFirstRowWhenNotThereInitially() throws Exception {
        setUpFakeDataToShow(7);

        TileView homePageTileView = (TileView) getTileGridLayout().getChildAt(7);

        // This is assuming that the rows on the first row are less than 6.
        TileView tileOnSecondRow = (TileView) getTileGridLayout().getChildAt(6);

        Assert.assertNotNull(homePageTileView);
        Assert.assertNotNull(tileOnSecondRow);
        Assert.assertTrue(isTileViewOnFirstRow(homePageTileView));
        Assert.assertFalse(isTileViewOnFirstRow(tileOnSecondRow));
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testHomePageStaysAtFirstRowWhenThereInitially() throws Exception {
        setUpFakeDataToShow(2);

        TileView homePageTileView = (TileView) getTileGridLayout().getChildAt(2);

        Assert.assertNotNull(homePageTileView);
        Assert.assertTrue(isTileViewOnFirstRow(homePageTileView));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @RetryOnFailure
    public void testTileGridAppearance() throws Exception {
        setUpFakeDataToShow(2);
        mRenderTestRule.render(getTileGridLayout(), "ntp_tile_grid_layout");
    }
    private void setUpFakeDataToShow(int homePagePosition) throws InterruptedException {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();

        assert FAKE_MOST_VISITED_URLS.length == FAKE_MOST_VISITED_TITLES.length;

        for (int i = 0; i < FAKE_MOST_VISITED_URLS.length; i++) {
            String url = FAKE_MOST_VISITED_URLS[i];
            String title = FAKE_MOST_VISITED_TITLES[i];
            siteSuggestions.add(FakeMostVisitedSites.createSiteSuggestion(
                    title, mTestServerRule.getServer().getURL(url)));
        }

        siteSuggestions.add(homePagePosition,
                new SiteSuggestion("HOMEPAGE", HOME_PAGE_URL, "", TileSource.HOMEPAGE,
                        TileSectionType.PERSONALIZED));

        FakeMostVisitedSites mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(siteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mSuggestionsDeps.getFactory().suggestionsSource = new FakeSuggestionsSource();

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        Tab mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();

        RecyclerViewTestUtils.waitForStableRecyclerView(getRecyclerView());
    }

    private NewTabPageRecyclerView getRecyclerView() {
        return mNtp.getNewTabPageView().getRecyclerView();
    }

    private TileGridLayout getTileGridLayout() {
        TileGridLayout tileGridLayout =
                mNtp.getNewTabPageView().findViewById(R.id.tile_grid_layout);
        Assert.assertNotNull("Unable to retrieve the TileGridLayout.", tileGridLayout);
        return tileGridLayout;
    }

    /** {@link TileView}s on the first row have a top margin equal to 0. */
    public boolean isTileViewOnFirstRow(TileView tileView) {
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) tileView.getLayoutParams();
        return marginLayoutParams.topMargin == 0;
    }
}
