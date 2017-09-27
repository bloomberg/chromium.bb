// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThanOrEqualTo;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.NodeParent;
import org.chromium.chrome.browser.ntp.cards.TreeNode;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.ChromeHome;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
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
public class TileGridLayoutTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @SuppressFBWarnings("URF_UNREAD_PUBLIC_OR_PROTECTED_FIELD")
    @Rule
    public ChromeHome.Processor mChromeHomeStateRule = new ChromeHome.Processor();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RenderTestRule mRenderTestRule = new RenderTestRule();

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

    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testHomePageIsMovedToFirstRowWhenNotThereInitially() throws Exception {
        NewTabPage ntp = setUpFakeDataToShowOnNtp(7);

        TileView homePageTileView = (TileView) getTileGridLayout(ntp).getChildAt(7);

        // This is assuming that the rows on the first row are less than 6.
        TileView tileOnSecondRow = (TileView) getTileGridLayout(ntp).getChildAt(6);

        Assert.assertNotNull(homePageTileView);
        Assert.assertNotNull(tileOnSecondRow);
        Assert.assertTrue(isTileViewOnFirstRow(homePageTileView));
        Assert.assertFalse(isTileViewOnFirstRow(tileOnSecondRow));
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testHomePageStaysAtFirstRowWhenThereInitially() throws Exception {
        NewTabPage ntp = setUpFakeDataToShowOnNtp(2);

        TileView homePageTileView = (TileView) getTileGridLayout(ntp).getChildAt(2);

        Assert.assertNotNull(homePageTileView);
        Assert.assertTrue(isTileViewOnFirstRow(homePageTileView));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    public void testTileGridAppearance() throws Exception {
        NewTabPage ntp = setUpFakeDataToShowOnNtp(2);
        mRenderTestRule.render(getTileGridLayout(ntp), "ntp_tile_grid_layout");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @ChromeHome
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testModernTileGridAppearance_Full() throws IOException, InterruptedException {
        View tileGridLayout = renderTiles(makeSuggestions(FAKE_MOST_VISITED_URLS.length));

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "modern_full_grid_portrait");

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "modern_full_grid_landscape");

        // In landscape, modern tiles should use all available space.
        int tileGridMaxWidthPx = tileGridLayout.getResources().getDimensionPixelSize(
                R.dimen.tile_grid_layout_max_width);
        if (((FrameLayout) tileGridLayout.getParent()).getMeasuredWidth() > tileGridMaxWidthPx) {
            assertThat(tileGridLayout.getMeasuredWidth(), greaterThan(tileGridMaxWidthPx));
        }
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @ChromeHome(false)
    public void testTileGridAppearance_Full() throws IOException, InterruptedException {
        View tileGridLayout = renderTiles(makeSuggestions(FAKE_MOST_VISITED_URLS.length));

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "full_grid_portrait");

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "full_grid_landscape");

        // In landscape, classic tiles should use at most tile_grid_layout_max_width px.
        int tileGridMaxWidthPx = tileGridLayout.getResources().getDimensionPixelSize(
                R.dimen.tile_grid_layout_max_width);
        if (((FrameLayout) tileGridLayout.getParent()).getMeasuredWidth() > tileGridMaxWidthPx) {
            assertThat(tileGridLayout.getMeasuredWidth(), lessThanOrEqualTo(tileGridMaxWidthPx));
        }
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @ChromeHome
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testModernTileGridAppearance_Two() throws IOException, InterruptedException {
        View tileGridLayout = renderTiles(makeSuggestions(2));

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "modern_two_tiles_grid_portrait");

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "modern_two_tiles_grid_landscape");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @ChromeHome(false)
    public void testTileGridAppearance_Two() throws IOException, InterruptedException {
        View tileGridLayout = renderTiles(makeSuggestions(2));

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "two_tiles_grid_portrait");

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "two_tiles_grid_landscape");
    }

    private List<SiteSuggestion> makeSuggestions(int count) {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>(count);

        assertEquals(FAKE_MOST_VISITED_URLS.length, FAKE_MOST_VISITED_TITLES.length);
        assertTrue(count <= FAKE_MOST_VISITED_URLS.length);

        for (int i = 0; i < count; i++) {
            String url = FAKE_MOST_VISITED_URLS[i];
            String title = FAKE_MOST_VISITED_TITLES[i];
            siteSuggestions.add(FakeMostVisitedSites.createSiteSuggestion(
                    title, mTestServerRule.getServer().getURL(url)));
        }

        return siteSuggestions;
    }

    private NewTabPage setUpFakeDataToShowOnNtp(int homePagePosition) throws InterruptedException {
        List<SiteSuggestion> siteSuggestions = makeSuggestions(FAKE_MOST_VISITED_URLS.length);
        siteSuggestions.add(homePagePosition,
                new SiteSuggestion("HOMEPAGE", HOME_PAGE_URL, "", TileSource.HOMEPAGE,
                        TileSectionType.PERSONALIZED));

        FakeMostVisitedSites mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(siteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mSuggestionsDeps.getFactory().suggestionsSource = new FakeSuggestionsSource();

        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        Tab mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        NewTabPage ntp = (NewTabPage) mTab.getNativePage();

        RecyclerViewTestUtils.waitForStableRecyclerView(ntp.getNewTabPageView().getRecyclerView());
        return ntp;
    }

    private void setOrientation(int requestedOrientation, Activity activity) {
        if (orientationMatchesRequest(activity, requestedOrientation)) return;

        ThreadUtils.runOnUiThreadBlocking(
                () -> activity.setRequestedOrientation(requestedOrientation));

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return orientationMatchesRequest(activity, requestedOrientation);
            }
        });
    }

    /**
     * Checks whether the requested orientation matches the current one.
     * @param activity Activity to check the orientation from. We pull its {@link Configuration}.
     * @param requestedOrientation The requested orientation, as used in
     *         {@link ActivityInfo#screenOrientation}. Note: This value is different from the one we
     *         check against from {@link Configuration#orientation}. The constants used to represent
     *         the values are not the same.
     */
    public boolean orientationMatchesRequest(Activity activity, int requestedOrientation) {
        int currentOrientation = activity.getResources().getConfiguration().orientation;

        // 1 => 1
        if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT) {
            return currentOrientation == Configuration.ORIENTATION_PORTRAIT;
        }

        // 0 => 2
        if (requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE) {
            return currentOrientation == Configuration.ORIENTATION_LANDSCAPE;
        }

        throw new IllegalArgumentException(
                "unexpected orientation requested: " + requestedOrientation);
    }

    private TileGridLayout getTileGridLayout(NewTabPage ntp) {
        TileGridLayout tileGridLayout = ntp.getNewTabPageView().findViewById(R.id.tile_grid_layout);
        Assert.assertNotNull("Unable to retrieve the TileGridLayout.", tileGridLayout);
        return tileGridLayout;
    }

    /** {@link TileView}s on the first row have a top margin equal to 0. */
    private boolean isTileViewOnFirstRow(TileView tileView) {
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) tileView.getLayoutParams();
        return marginLayoutParams.topMargin == 0;
    }

    /**
     * Starts and sets up an activity to render the provided site suggestions in the activity.
     * @return the layout in which the suggestions are rendered.
     */
    private TileGridLayout renderTiles(List<SiteSuggestion> siteSuggestions)
            throws IOException, InterruptedException {
        // Launching the activity, that should now use the right UI.
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeActivity activity = mActivityTestRule.getActivity();

        // Setting up the dummy data.
        FakeMostVisitedSites mostVisitedSites = new FakeMostVisitedSites();
        mostVisitedSites.setTileSuggestions(siteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mostVisitedSites;
        mSuggestionsDeps.getFactory().suggestionsSource = new FakeSuggestionsSource();

        FrameLayout contentView = new FrameLayout(activity);
        UiConfig uiConfig = new UiConfig(contentView);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            activity.setContentView(contentView);

            SiteSectionViewHolder viewHolder = SiteSection.createViewHolder(
                    SiteSection.inflateSiteSection(contentView), uiConfig);

            uiConfig.updateDisplayStyle();

            SiteSection siteSection = createSiteSection(viewHolder, uiConfig);
            siteSection.getTileGroup().onSwitchToForeground(false);
            assertTrue("Tile Data should be visible.", siteSection.isVisible());

            siteSection.onBindViewHolder(viewHolder, 0);
            contentView.addView(viewHolder.itemView);
        });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertEquals(1, contentView.getChildCount());
        return (TileGridLayout) contentView.getChildAt(0);
    }

    private SiteSection createSiteSection(SiteSectionViewHolder viewHolder, UiConfig uiConfig) {
        ThreadUtils.assertOnUiThread();

        ChromeActivity activity = mActivityTestRule.getActivity();

        Profile profile = Profile.getLastUsedProfile();
        SuggestionsUiDelegate uiDelegate = new SuggestionsUiDelegateImpl(
                mSuggestionsDeps.getFactory().createSuggestionSource(null),
                mSuggestionsDeps.getFactory().createEventReporter(), null, profile, null,
                activity.getChromeApplication().getReferencePool(), activity.getSnackbarManager());

        OfflinePageBridge opb = OfflinePageBridge.getForProfile(profile);
        TileGroup.Delegate delegate = new TileGroupDelegateImpl(activity, profile, null, null);
        SiteSection siteSection = new SiteSection(uiDelegate, null, delegate, opb, uiConfig);

        siteSection.setParent(new NodeParent() {
            @Override
            public void onItemRangeChanged(TreeNode child, int index, int count,
                    @Nullable NewTabPageViewHolder.PartialBindCallback callback) {
                if (callback != null) callback.onResult(viewHolder);
            }

            @Override
            public void onItemRangeInserted(TreeNode child, int index, int count) {}

            @Override
            public void onItemRangeRemoved(TreeNode child, int index, int count) {}
        });

        return siteSection;
    }
}
