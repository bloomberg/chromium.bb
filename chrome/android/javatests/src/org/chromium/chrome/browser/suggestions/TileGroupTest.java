// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link TileGroup} on the New Tab Page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
@RetryOnFailure
public class TileGroupTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String[] FAKE_MOST_VISITED_URLS =
            new String[] {"/chrome/test/data/android/navigate/one.html",
                    "/chrome/test/data/android/navigate/two.html",
                    "/chrome/test/data/android/navigate/three.html"};

    private NewTabPage mNtp;
    private String[] mSiteSuggestionUrls;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());

        mSiteSuggestionUrls = new String[] {mTestServer.getURL(FAKE_MOST_VISITED_URLS[0]),
                mTestServer.getURL(FAKE_MOST_VISITED_URLS[1]),
                mTestServer.getURL(FAKE_MOST_VISITED_URLS[2])};

        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestionUrls);
        TileGroupDelegateImpl.setMostVisitedSitesForTests(mMostVisitedSites);

        FakeSuggestionsSource mSource = new FakeSuggestionsSource();
        NewTabPage.setSuggestionsSourceForTests(mSource);

        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        Tab mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();

        RecyclerViewTestUtils.waitForStableRecyclerView(getRecyclerView());
    }

    @After
    public void tearDown() throws Exception {
        TileGroupDelegateImpl.setMostVisitedSitesForTests(null);
        NewTabPage.setSuggestionsSourceForTests(null);
        mTestServer.stopAndDestroyServer();

    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissTileWithContextMenu() throws InterruptedException, TimeoutException {
        final View tileView = getTileViewForUrl(mSiteSuggestionUrls[0]);

        // Dismiss the tile using the context menu.
        invokeContextMenu(tileView, ContextMenuManager.ID_REMOVE);
        Assert.assertTrue(mMostVisitedSites.isUrlBlacklisted(mSiteSuggestionUrls[0]));

        // Ensure that undoing the removal is reflected in the ui.
        Assert.assertEquals(3, getTileGridLayout().getChildCount());
        mMostVisitedSites.setTileSuggestions(mSiteSuggestionUrls[1], mSiteSuggestionUrls[2]);
        waitForTileRemoved(mSiteSuggestionUrls[0]);
        Assert.assertEquals(2, getTileGridLayout().getChildCount());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissTileUndo() throws InterruptedException, TimeoutException {
        final ViewGroup tileContainer = getTileGridLayout();
        final View tileView = getTileViewForUrl(mSiteSuggestionUrls[0]);
        Assert.assertEquals(3, tileContainer.getChildCount());

        // Dismiss the tile using the context menu.
        invokeContextMenu(tileView, ContextMenuManager.ID_REMOVE);

        // Ensure that the removal update goes through.
        mMostVisitedSites.setTileSuggestions(mSiteSuggestionUrls[1], mSiteSuggestionUrls[2]);
        waitForTileRemoved(mSiteSuggestionUrls[0]);
        Assert.assertEquals(2, tileContainer.getChildCount());
        final View snackbarButton = waitForSnackbar(mActivityTestRule.getActivity());

        Assert.assertTrue(mMostVisitedSites.isUrlBlacklisted(mSiteSuggestionUrls[0]));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                snackbarButton.callOnClick();
            }
        });

        Assert.assertFalse(mMostVisitedSites.isUrlBlacklisted(mSiteSuggestionUrls[0]));

        // Ensure that the removal of the update goes through.
        mMostVisitedSites.setTileSuggestions(mSiteSuggestionUrls);
        waitForTileAdded(mSiteSuggestionUrls[0]);
        Assert.assertEquals(3, tileContainer.getChildCount());
    }

    private NewTabPageRecyclerView getRecyclerView() {
        return mNtp.getNewTabPageView().getRecyclerView();
    }

    private TileGridLayout getTileGridLayout() {
        ViewGroup aboveTheFoldView = getRecyclerView().getAboveTheFoldView();
        Assert.assertNotNull("Unable to retrieve the AboveTheFold view.", aboveTheFoldView);

        TileGridLayout tileGridLayout =
                (TileGridLayout) aboveTheFoldView.findViewById(R.id.tile_grid_layout);
        Assert.assertNotNull("Unable to retrieve the TileGridLayout.", tileGridLayout);
        return tileGridLayout;
    }

    private View getTileViewForUrl(String url) {
        View tileView = getTileGridLayout().getTileView(url);
        Assert.assertNotNull("Tile not found for url " + url, tileView);

        return tileView;
    }

    private void invokeContextMenu(View view, int contextMenuItemId) {
        TestTouchUtils.longClickView(InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), contextMenuItemId, 0));
    }

    /** Wait for the snackbar associated to a tile dismissal to be shown and returns its button. */
    private static View waitForSnackbar(final ChromeActivity activity) {
        final String expectedSnackbarMessage =
                activity.getResources().getString(R.string.most_visited_item_removed);
        CriteriaHelper.pollUiThread(new Criteria("The snackbar was not shown.") {
            @Override
            public boolean isSatisfied() {
                SnackbarManager snackbarManager = activity.getSnackbarManager();
                if (!snackbarManager.isShowing()) return false;

                TextView snackbarMessage = (TextView) activity.findViewById(R.id.snackbar_message);
                if (snackbarMessage == null) return false;

                return snackbarMessage.getText().toString().equals(expectedSnackbarMessage);
            }
        });

        return activity.findViewById(R.id.snackbar_button);
    }

    private void waitForTileRemoved(final String url)
            throws TimeoutException, InterruptedException {
        TileGridLayout tileContainer = getTileGridLayout();
        final TileView removedTile = tileContainer.getTileView(url);
        if (removedTile == null) return;

        final CallbackHelper callback = new CallbackHelper();
        tileContainer.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
            @Override
            public void onChildViewAdded(View parent, View child) {}

            @Override
            public void onChildViewRemoved(View parent, View child) {
                if (child == removedTile) callback.notifyCalled();
            }
        });
        callback.waitForCallback("The expected tile was not removed.", 0);
        tileContainer.setOnHierarchyChangeListener(null);
    }

    private void waitForTileAdded(final String url) throws TimeoutException, InterruptedException {
        TileGridLayout tileContainer = getTileGridLayout();
        if (tileContainer.getTileView(url) != null) return;

        final CallbackHelper callback = new CallbackHelper();
        tileContainer.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
            @Override
            public void onChildViewAdded(View parent, View child) {
                if (!(child instanceof TileView)) return;
                if (!((TileView) child).getUrl().equals(url)) return;

                callback.notifyCalled();
            }

            @Override
            public void onChildViewRemoved(View parent, View child) {}
        });
        callback.waitForCallback("The expected tile was not added.", 0);
        tileContainer.setOnHierarchyChangeListener(null);
    }
}
