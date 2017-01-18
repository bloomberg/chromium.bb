// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.FakeMostVisitedSites;
import org.chromium.chrome.browser.ntp.NTPTileSource;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageView;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.FakeSuggestionsSource;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
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

    private Tab mTab;
    private NewTabPage mNtp;
    private String[] mFakeMostVisitedUrls;
    private FakeMostVisitedSites mFakeMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private FakeSuggestionsSource mSource;

    @Override
    protected void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        mFakeMostVisitedUrls = new String[] {mTestServer.getURL(TEST_PAGE)};

        mSource = new FakeSuggestionsSource();
        mSource.setInfoForCategory(KnownCategories.ARTICLES,
                new SuggestionsCategoryInfo(KnownCategories.ARTICLES, "Articles test title",
                        ContentSuggestionsCardLayout.FULL_CARD, /*hasMoreAction=*/true,
                        /*hasReloadAction=*/true, /*hasViewAllAction=*/false, /*showIfEmpty=*/true,
                        "noSuggestionsMessage"));
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.INITIALIZING);
        NewTabPage.setSuggestionsSourceForTests(mSource);

        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        NewTabPage.setMostVisitedSitesForTests(null);
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
                mFakeMostVisitedSites = new FakeMostVisitedSites(mTab.getProfile(),
                        FAKE_MOST_VISITED_TITLES, mFakeMostVisitedUrls,
                        FAKE_MOST_VISITED_WHITELIST_ICON_PATHS, FAKE_MOST_VISITED_SOURCES);
            }
        });
        NewTabPage.setMostVisitedSitesForTests(mFakeMostVisitedSites);

        loadUrl(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
    }

    @MediumTest
    @Feature({"NewTabPage"})
    @CommandLineFlags.Add("enable-features=NTPSnippets")
    public void testClickSuggestion() throws InterruptedException {
        setSuggestionsAndWaitForUpdate(10);
        List<SnippetArticle> suggestions =
                mSource.getSuggestionsForCategory(KnownCategories.ARTICLES);

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
    @CommandLineFlags.Add("enable-features=NTPSnippets,NTPSuggestionsSectionDismissal")
    public void testAllDismissed() throws InterruptedException, TimeoutException {
        setSuggestionsAndWaitForUpdate(3);
        assertEquals(3, mSource.getSuggestionsForCategory(KnownCategories.ARTICLES).size());
        assertEquals(RecyclerView.NO_POSITION,
                getAdapter().getFirstPositionForType(ItemViewType.ALL_DISMISSED));
        assertEquals(1, mSource.getCategories().length);
        assertEquals(KnownCategories.ARTICLES, mSource.getCategories()[0]);

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
        waitForViewToDetach(allDismissedView);
        assertEquals(1, mSource.getCategories().length);
        assertEquals(KnownCategories.ARTICLES, mSource.getCategories()[0]);
    }

    @MediumTest
    @Feature({"NewTabPage"})
    @CommandLineFlags.Add("enable-features=NTPSnippets")
    public void testDismissArticleWithContextMenu() throws InterruptedException, TimeoutException {
        setSuggestionsAndWaitForUpdate(10);
        List<SnippetArticle> suggestions =
                mSource.getSuggestionsForCategory(KnownCategories.ARTICLES);
        assertEquals(10, suggestions.size());

        // Scroll a suggestion into view.
        int suggestionPosition = getSuggestionPosition(suggestions.get(suggestions.size() - 1));
        View suggestionView = getViewHolderAtPosition(suggestionPosition).itemView;

        // Dismiss the suggestion using the context menu.
        invokeContextMenu(suggestionView, ContextMenuManager.ID_REMOVE);
        waitForViewToDetach(suggestionView);

        suggestions = mSource.getSuggestionsForCategory(KnownCategories.ARTICLES);
        assertEquals(9, suggestions.size());
    }

    @MediumTest
    @Feature({"NewTabPage"})
    @CommandLineFlags.Add("enable-features=NTPSnippets,NTPSuggestionsSectionDismissal")
    public void testDismissStatusCardWithContextMenu()
            throws InterruptedException, TimeoutException {
        setSuggestionsAndWaitForUpdate(0);
        assertArrayEquals(new int[] {KnownCategories.ARTICLES}, mSource.getCategories());

        // Scroll the status card into view.
        int cardPosition = getAdapter().getFirstCardPosition();
        assertEquals(ItemViewType.STATUS, getAdapter().getItemViewType(cardPosition));

        View statusCardView = getViewHolderAtPosition(cardPosition).itemView;

        // Dismiss the status card using the context menu.
        invokeContextMenu(statusCardView, ContextMenuManager.ID_REMOVE);
        waitForViewToDetach(statusCardView);

        assertArrayEquals(new int[0], mSource.getCategories());
    }

    @MediumTest
    @Feature({"NewTabPage"})
    @CommandLineFlags.Add("enable-features=NTPSnippets,NTPSuggestionsSectionDismissal")
    public void testDismissActionItemWithContextMenu()
            throws InterruptedException, TimeoutException {
        setSuggestionsAndWaitForUpdate(0);
        assertArrayEquals(new int[] {KnownCategories.ARTICLES}, mSource.getCategories());

        // Scroll the action item into view.
        int actionItemPosition = getAdapter().getFirstCardPosition() + 1;
        assertEquals(ItemViewType.ACTION, getAdapter().getItemViewType(actionItemPosition));
        View actionItemView = getViewHolderAtPosition(actionItemPosition).itemView;

        // Dismiss the action item using the context menu.
        invokeContextMenu(actionItemView, ContextMenuManager.ID_REMOVE);
        waitForViewToDetach(actionItemView);

        assertArrayEquals(new int[0], mSource.getCategories());
    }

    private NewTabPageView getNtpView() {
        return mNtp.getNewTabPageView();
    }

    private NewTabPageRecyclerView getRecyclerView() {
        return (NewTabPageRecyclerView) getNtpView().getWrapperView();
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
        return waitForView(position);
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
        waitForViewToDetach(viewHolder.itemView);
    }

    private void setSuggestionsAndWaitForUpdate(final int suggestionsCount) {
        final FakeSuggestionsSource source = mSource;

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                source.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
                source.setSuggestionsForCategory(
                        KnownCategories.ARTICLES, buildSuggestions(suggestionsCount));
            }
        });
        waitForStableRecyclerView();
    }

    private List<SnippetArticle> buildSuggestions(int suggestionsCount) {
        List<SnippetArticle> suggestions = new ArrayList<>();
        for (int i = 0; i < suggestionsCount; i++) {
            String url = mTestServer.getURL(TEST_PAGE) + "#" + i;
            suggestions.add(new SnippetArticle(KnownCategories.ARTICLES, "id" + i, "title" + i,
                    "publisher" + i, "previewText" + i, url, 1466614774 + i, 10f));
        }
        return suggestions;
    }

    private void invokeContextMenu(View view, int contextMenuItemId) {
        TestTouchUtils.longClickView(getInstrumentation(), view);
        assertTrue(
                getInstrumentation().invokeContextMenuAction(getActivity(), contextMenuItemId, 0));
    }

    private ViewHolder waitForView(final int position) {
        final NewTabPageRecyclerView recyclerView = getRecyclerView();

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(position);

                if (viewHolder == null) {
                    updateFailureReason("Cannot find view holder for position " + position + ".");
                    return false;
                }

                if (viewHolder.itemView.getParent() == null) {
                    updateFailureReason("The view is not attached for position " + position + ".");
                    return false;
                }

                return true;
            }
        });

        waitForStableRecyclerView();

        return recyclerView.findViewHolderForAdapterPosition(position);
    }

    private void waitForViewToDetach(final View view)
            throws InterruptedException, TimeoutException {
        final RecyclerView recyclerView = getRecyclerView();
        final CallbackHelper callback = new CallbackHelper();

        recyclerView.addOnChildAttachStateChangeListener(
                new RecyclerView.OnChildAttachStateChangeListener() {
                    @Override
                    public void onChildViewAttachedToWindow(View view) {}

                    @Override
                    public void onChildViewDetachedFromWindow(View detachedView) {
                        if (detachedView == view) {
                            recyclerView.removeOnChildAttachStateChangeListener(this);
                            callback.notifyCalled();
                        }
                    }
                });
        callback.waitForCallback("The view did not detach.", 0);

        waitForStableRecyclerView();
    }

    private void waitForStableRecyclerView() {
        final RecyclerView recyclerView = getRecyclerView();

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (recyclerView.isComputingLayout()) {
                    updateFailureReason("The recycler view is computing layout.");
                    return false;
                }

                if (recyclerView.isLayoutFrozen()) {
                    updateFailureReason("The recycler view layout is frozen.");
                    return false;
                }

                if (recyclerView.isAnimating()) {
                    updateFailureReason("The recycler view is animating.");
                    return false;
                }

                return true;
            }
        });
    }

    private static void assertArrayEquals(int[] expected, int[] actual) {
        assertEquals(Arrays.toString(expected), Arrays.toString(actual));
    }
}
