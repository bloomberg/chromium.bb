// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.res.Resources;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NTPTileSource;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageView;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.browser.suggestions.TileGroupDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link NewTabPageRecyclerView}.
 */
@RetryOnFailure
public class NewTabPageRecyclerViewTest extends ChromeTabbedActivityTestBase {
    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final String[] FAKE_MOST_VISITED_TITLES = new String[] {"Simple"};
    private static final String[] FAKE_MOST_VISITED_WHITELIST_ICON_PATHS = new String[] {""};
    private static final int[] FAKE_MOST_VISITED_SOURCES = new int[] {NTPTileSource.TOP_SITES};
    private static final long FAKE_PUBLISH_TIMESTAMP = 1466614774;
    private static final long FAKE_FETCH_TIMESTAMP = 1466634774;
    private static final float FAKE_SNIPPET_SCORE = 10f;

    // TODO(dgn): Properly bypass the native code when testing with a fake suggestions source.
    // We currently mix the fake and the snippets bridge, resulting in crashes with unregistered
    // categories.
    @CategoryInt
    private static final int TEST_CATEGORY = KnownCategories.BOOKMARKS;

    private Tab mTab;
    private NewTabPage mNtp;
    private String[] mSiteSuggestionUrls;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private FakeSuggestionsSource mSource;

    @Override
    protected void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        mSiteSuggestionUrls = new String[] {mTestServer.getURL(TEST_PAGE)};

        mSource = new FakeSuggestionsSource();
        mSource.setInfoForCategory(TEST_CATEGORY,
                new SuggestionsCategoryInfo(TEST_CATEGORY, "Suggestions test title",
                        ContentSuggestionsCardLayout.FULL_CARD, /*hasFetchAction=*/true,
                        /*hasViewAllAction=*/false, /*showIfEmpty=*/true, "noSuggestionsMessage"));
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        NewTabPage.setSuggestionsSourceForTests(mSource);

        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        TileGroupDelegateImpl.setMostVisitedSitesForTests(null);
        NewTabPage.setSuggestionsSourceForTests(null);
        mTestServer.stopAndDestroyServer();

        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mTab = getActivity().getActivityTab();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Create FakeMostVisitedSites after starting the activity, since it depends on
                // native code.
                mMostVisitedSites = new FakeMostVisitedSites(mTab.getProfile());
                mMostVisitedSites.setTileSuggestions(FAKE_MOST_VISITED_TITLES, mSiteSuggestionUrls,
                        FAKE_MOST_VISITED_WHITELIST_ICON_PATHS, FAKE_MOST_VISITED_SOURCES);
            }
        });
        TileGroupDelegateImpl.setMostVisitedSitesForTests(mMostVisitedSites);

        loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
    }

    @MediumTest
    @Feature({"NewTabPage"})
    public void testClickSuggestion() throws InterruptedException {
        setSuggestionsAndWaitForUpdate(10);
        List<SnippetArticle> suggestions = mSource.getSuggestionsForCategory(TEST_CATEGORY);

        // Scroll the last suggestion into view and click it.
        SnippetArticle suggestion = suggestions.get(suggestions.size() - 1);
        int suggestionPosition = getSuggestionPosition(suggestion);
        final View suggestionView = getViewHolderAtPosition(suggestionPosition).itemView;
        ChromeTabUtils.waitForTabPageLoaded(mTab, new Runnable() {
            @Override
            public void run() {
                singleClickView(suggestionView);
            }
        });
        assertEquals(suggestion.mUrl, mTab.getUrl());
    }

    @MediumTest
    @Feature({"NewTabPage"})
    public void testAllDismissed() throws InterruptedException, TimeoutException {
        setSuggestionsAndWaitForUpdate(3);
        assertEquals(3, mSource.getSuggestionsForCategory(TEST_CATEGORY).size());
        assertEquals(RecyclerView.NO_POSITION,
                getAdapter().getFirstPositionForType(ItemViewType.ALL_DISMISSED));
        assertEquals(1, mSource.getCategories().length);
        assertEquals(TEST_CATEGORY, mSource.getCategories()[0]);

        // Dismiss the sign in promo.
        int signinPromoPosition = getAdapter().getFirstPositionForType(ItemViewType.PROMO);
        dismissItemAtPosition(signinPromoPosition);

        // Dismiss all the cards, including status cards, which dismisses the associated category.
        while (true) {
            int cardPosition = getAdapter().getFirstCardPosition();
            if (cardPosition == RecyclerView.NO_POSITION) break;
            dismissItemAtPosition(cardPosition);
        }
        assertEquals(0, mSource.getCategories().length);

        // Click the refresh button on the all dismissed item.
        int allDismissedPosition = getAdapter().getFirstPositionForType(ItemViewType.ALL_DISMISSED);
        assertTrue(allDismissedPosition != RecyclerView.NO_POSITION);
        View allDismissedView = getViewHolderAtPosition(allDismissedPosition).itemView;
        singleClickView(allDismissedView.findViewById(R.id.action_button));
        RecyclerViewTestUtils.waitForViewToDetach(getRecyclerView(), allDismissedView);
        assertEquals(1, mSource.getCategories().length);
        assertEquals(TEST_CATEGORY, mSource.getCategories()[0]);
    }

    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissArticleWithContextMenu() throws InterruptedException, TimeoutException {
        setSuggestionsAndWaitForUpdate(10);
        List<SnippetArticle> suggestions = mSource.getSuggestionsForCategory(TEST_CATEGORY);
        assertEquals(10, suggestions.size());

        // Scroll a suggestion into view.
        int suggestionPosition = getSuggestionPosition(suggestions.get(suggestions.size() - 1));
        View suggestionView = getViewHolderAtPosition(suggestionPosition).itemView;

        // Dismiss the suggestion using the context menu.
        invokeContextMenu(suggestionView, ContextMenuManager.ID_REMOVE);
        RecyclerViewTestUtils.waitForViewToDetach(getRecyclerView(), suggestionView);

        suggestions = mSource.getSuggestionsForCategory(TEST_CATEGORY);
        assertEquals(9, suggestions.size());
    }

    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissStatusCardWithContextMenu()
            throws InterruptedException, TimeoutException {
        setSuggestionsAndWaitForUpdate(0);
        assertArrayEquals(new int[] {TEST_CATEGORY}, mSource.getCategories());

        // Scroll the status card into view.
        int cardPosition = getAdapter().getFirstCardPosition();
        assertEquals(ItemViewType.STATUS, getAdapter().getItemViewType(cardPosition));

        View statusCardView = getViewHolderAtPosition(cardPosition).itemView;

        // Dismiss the status card using the context menu.
        invokeContextMenu(statusCardView, ContextMenuManager.ID_REMOVE);
        RecyclerViewTestUtils.waitForViewToDetach(getRecyclerView(), statusCardView);

        assertArrayEquals(new int[0], mSource.getCategories());
    }

    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissActionItemWithContextMenu()
            throws InterruptedException, TimeoutException {
        setSuggestionsAndWaitForUpdate(0);
        assertArrayEquals(new int[] {TEST_CATEGORY}, mSource.getCategories());

        // Scroll the action item into view.
        int actionItemPosition = getAdapter().getFirstCardPosition() + 1;
        assertEquals(ItemViewType.ACTION, getAdapter().getItemViewType(actionItemPosition));
        View actionItemView = getViewHolderAtPosition(actionItemPosition).itemView;

        // Dismiss the action item using the context menu.
        invokeContextMenu(actionItemView, ContextMenuManager.ID_REMOVE);
        RecyclerViewTestUtils.waitForViewToDetach(getRecyclerView(), actionItemView);

        assertArrayEquals(new int[0], mSource.getCategories());
    }

    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE})
    @CommandLineFlags.Add({"disable-features=" + ChromeFeatureList.NTP_CONDENSED_LAYOUT})
    public void testSnapScroll_noCondensedLayout() {
        setSuggestionsAndWaitForUpdate(0);

        Resources res = getInstrumentation().getTargetContext().getResources();
        int toolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                + res.getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
        View searchBox = getNtpView().findViewById(R.id.search_box);
        int searchBoxTop = searchBox.getTop() + searchBox.getPaddingTop();
        int searchBoxTransitionLength =
                res.getDimensionPixelSize(R.dimen.ntp_search_box_transition_length);

        // Two different snapping regions with the default behavior: snapping back up to the
        // watershed point in the middle, snapping forward after that.
        assertEquals(0, getSnapPosition(0));
        assertEquals(0, getSnapPosition(toolbarHeight / 2 - 1));
        assertEquals(toolbarHeight, getSnapPosition(toolbarHeight / 2));
        assertEquals(toolbarHeight, getSnapPosition(toolbarHeight));
        assertEquals(toolbarHeight + 1, getSnapPosition(toolbarHeight + 1));

        assertEquals(searchBoxTop - searchBoxTransitionLength - 1,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength - 1));
        assertEquals(searchBoxTop - searchBoxTransitionLength,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength));
        assertEquals(searchBoxTop - searchBoxTransitionLength,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength / 2 - 1));
        assertEquals(searchBoxTop, getSnapPosition(searchBoxTop - searchBoxTransitionLength / 2));
        assertEquals(searchBoxTop, getSnapPosition(searchBoxTop));
        assertEquals(searchBoxTop + 1, getSnapPosition(searchBoxTop + 1));
    }

    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.NTP_CONDENSED_LAYOUT})
    public void testSnapScroll_condensedLayout() {
        setSuggestionsAndWaitForUpdate(0);

        Resources res = getInstrumentation().getTargetContext().getResources();
        int toolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                + res.getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
        View searchBox = getNtpView().findViewById(R.id.search_box);
        int searchBoxTop = searchBox.getTop() + searchBox.getPaddingTop();
        int searchBoxTransitionLength =
                res.getDimensionPixelSize(R.dimen.ntp_search_box_transition_length);

        // With the condensed layout, the snapping regions overlap, so the effect is that of a
        // single snapping region.
        assertEquals(0, getSnapPosition(0));
        assertEquals(0, getSnapPosition(toolbarHeight / 2 - 1));
        assertEquals(searchBoxTop, getSnapPosition(toolbarHeight / 2));
        assertEquals(searchBoxTop, getSnapPosition(searchBoxTop - searchBoxTransitionLength));
        assertEquals(searchBoxTop, getSnapPosition(toolbarHeight));
        assertEquals(searchBoxTop, getSnapPosition(searchBoxTop));
        assertEquals(searchBoxTop + 1, getSnapPosition(searchBoxTop + 1));
    }

    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_TABLET})
    public void testSnapScroll_tablet() {
        setSuggestionsAndWaitForUpdate(0);

        Resources res = getInstrumentation().getTargetContext().getResources();
        int toolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                + res.getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
        View searchBox = getNtpView().findViewById(R.id.search_box);
        int searchBoxTop = searchBox.getTop() + searchBox.getPaddingTop();
        int searchBoxTransitionLength =
                res.getDimensionPixelSize(R.dimen.ntp_search_box_transition_length);

        // No snapping on tablets.
        // Note: This ignores snapping for the peeking cards, which is currently disabled
        // by default.
        assertEquals(0, getSnapPosition(0));
        assertEquals(toolbarHeight / 2 - 1, getSnapPosition(toolbarHeight / 2 - 1));
        assertEquals(toolbarHeight / 2, getSnapPosition(toolbarHeight / 2));
        assertEquals(toolbarHeight, getSnapPosition(toolbarHeight));
        assertEquals(toolbarHeight + 1, getSnapPosition(toolbarHeight + 1));

        assertEquals(searchBoxTop - searchBoxTransitionLength - 1,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength - 1));
        assertEquals(searchBoxTop - searchBoxTransitionLength,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength));
        assertEquals(searchBoxTop - searchBoxTransitionLength / 2 - 1,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength / 2 - 1));
        assertEquals(searchBoxTop - searchBoxTransitionLength / 2,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength / 2));
        assertEquals(searchBoxTop, getSnapPosition(searchBoxTop));
        assertEquals(searchBoxTop + 1, getSnapPosition(searchBoxTop + 1));
    }

    private int getSnapPosition(int scrollPosition) {
        NewTabPageView ntpView = getNtpView();
        return getRecyclerView().calculateSnapPosition(
                scrollPosition, ntpView.findViewById(R.id.search_box), ntpView.getHeight());
    }

    private NewTabPageView getNtpView() {
        return mNtp.getNewTabPageView();
    }

    private NewTabPageRecyclerView getRecyclerView() {
        return getNtpView().getRecyclerView();
    }

    private NewTabPageAdapter getAdapter() {
        return getRecyclerView().getNewTabPageAdapter();
    }

    private int getSuggestionPosition(SnippetArticle article) {
        NewTabPageAdapter adapter = getAdapter();
        for (int i = 0; i < adapter.getItemCount(); i++) {
            SnippetArticle articleToCheck = adapter.getSuggestionAt(i);
            if (articleToCheck != null && articleToCheck.equals(article)) return i;
        }
        return RecyclerView.NO_POSITION;
    }

    /**
     * Scroll the {@link View} at the given adapter position into view and returns
     * its {@link ViewHolder}.
     * @param position the adapter position for which to return the {@link ViewHolder}.
     * @return the ViewHolder for the given {@code position}.
     */
    private ViewHolder getViewHolderAtPosition(final int position) {
        final NewTabPageRecyclerView recyclerView = getRecyclerView();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                recyclerView.getLinearLayoutManager().scrollToPositionWithOffset(
                        position, getActivity().getResources().getDimensionPixelSize(
                                R.dimen.tab_strip_height));
            }
        });
        return RecyclerViewTestUtils.waitForView(getRecyclerView(), position);
    }

    /**
     * Dismiss the item at the given {@code position} and wait until it has been removed from the
     * {@link RecyclerView}.
     * @param position the adapter position to remove.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    private void dismissItemAtPosition(int position) throws InterruptedException, TimeoutException {
        final ViewHolder viewHolder = getViewHolderAtPosition(position);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getRecyclerView().dismissItemWithAnimation(viewHolder);
            }
        });
        RecyclerViewTestUtils.waitForViewToDetach(getRecyclerView(), (viewHolder.itemView));
    }

    private void setSuggestionsAndWaitForUpdate(final int suggestionsCount) {
        final FakeSuggestionsSource source = mSource;

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                source.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
                source.setSuggestionsForCategory(TEST_CATEGORY, buildSuggestions(suggestionsCount));
            }
        });
        RecyclerViewTestUtils.waitForStableRecyclerView(getRecyclerView());
    }

    private List<SnippetArticle> buildSuggestions(int suggestionsCount) {
        List<SnippetArticle> suggestions = new ArrayList<>();
        for (int i = 0; i < suggestionsCount; i++) {
            String url = mTestServer.getURL(TEST_PAGE) + "#" + i;
            suggestions.add(new SnippetArticle(TEST_CATEGORY, "id" + i, "title" + i,
                    "publisher" + i, "previewText" + i, url, FAKE_PUBLISH_TIMESTAMP + i,
                    FAKE_SNIPPET_SCORE, FAKE_FETCH_TIMESTAMP));
        }
        return suggestions;
    }

    private void invokeContextMenu(View view, int contextMenuItemId) {
        TestTouchUtils.longClickView(getInstrumentation(), view);
        assertTrue(
                getInstrumentation().invokeContextMenuAction(getActivity(), contextMenuItemId, 0));
    }


    private static void assertArrayEquals(int[] expected, int[] actual) {
        assertEquals(Arrays.toString(expected), Arrays.toString(actual));
    }
}
