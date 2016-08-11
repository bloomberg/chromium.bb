// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleListItem;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Unit tests for {@link NewTabPageAdapter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageAdapterTest {
    /**
     * Number of elements, not including content suggestions that are loaded
     * in a populated recycler view.
     * The 3 elements are: above-the-fold, header, bottom spacer
     * TODO(dgn): Make this depend on the category info of the loaded sections
     * instead of being a constant, as it needs to know if the MORE button is
     * present for example.
     */
    private static final int PERMANENT_ELEMENTS_COUNT = 3;

    /**
     * Number of elements that are loaded in an empty recycler view
     * The 5 elements are: above-the-fold, header, status card, progress
     * indicator, bottom spacer.
     */
    private static final int EMPTY_STATE_ELEMENTS_COUNT = 5;

    private static class FakeSnippetsSource implements SuggestionsSource {
        private SuggestionsSource.Observer mObserver;
        private final Map<Integer, List<SnippetArticleListItem>> mSuggestions = new HashMap<>();
        private final Map<Integer, Integer> mCategoryStatus = new HashMap<>();
        private final Map<Integer, SuggestionsCategoryInfo> mCategoryInfo = new HashMap<>();

        public void setStatusForCategory(@CategoryInt int category,
                @CategoryStatusEnum int status) {
            mCategoryStatus.put(category, status);
            if (mObserver != null) mObserver.onCategoryStatusChanged(category, status);
        }

        public void setSuggestionsForCategory(
                @CategoryInt int category, List<SnippetArticleListItem> suggestions) {
            mSuggestions.put(category, suggestions);
            if (mObserver != null) mObserver.onNewSuggestions(category);
        }

        public void setInfoForCategory(@CategoryInt int category, SuggestionsCategoryInfo info) {
            mCategoryInfo.put(category, info);
        }

        @Override
        public void dismissSuggestion(SnippetArticleListItem suggestion) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void fetchSuggestionImage(
                SnippetArticleListItem suggestion, Callback<Bitmap> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void getSuggestionVisited(
                SnippetArticleListItem suggestion, Callback<Boolean> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void setObserver(Observer observer) {
            mObserver = observer;
        }

        @Override
        public int[] getCategories() {
            Set<Integer> ids = mCategoryStatus.keySet();
            int[] result = new int[ids.size()];
            int index = 0;
            for (int id : ids) result[index++] = id;
            return result;
        }

        @CategoryStatusEnum
        @Override
        public int getCategoryStatus(@CategoryInt int category) {
            return mCategoryStatus.get(category);
        }

        @Override
        public SuggestionsCategoryInfo getCategoryInfo(int category) {
            return mCategoryInfo.get(category);
        }

        @Override
        public List<SnippetArticleListItem> getSuggestionsForCategory(int category) {
            List<SnippetArticleListItem> result = mSuggestions.get(category);
            return result == null ? Collections.<SnippetArticleListItem>emptyList() : result;
        }
    }

    private FakeSnippetsSource mSnippetsSource = new FakeSnippetsSource();
    private NewTabPageAdapter mNtpAdapter;

    @Before
    public void setUp() {
        RecordHistogram.disableForTests();

        mSnippetsSource = new FakeSnippetsSource();
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.INITIALIZING);
        mSnippetsSource.setInfoForCategory(
                KnownCategories.ARTICLES, new SuggestionsCategoryInfo("Articles for you",
                                                  ContentSuggestionsCardLayout.FULL_CARD));
        mNtpAdapter = new NewTabPageAdapter(null, null, mSnippetsSource, null);
    }

    /**
     * Tests the content of the adapter under standard conditions: on start and after a snippet
     * fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoading() {
        assertEquals(EMPTY_STATE_ELEMENTS_COUNT, mNtpAdapter.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, mNtpAdapter.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_PROGRESS, mNtpAdapter.getItemViewType(3));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, mNtpAdapter.getItemViewType(4));

        List<SnippetArticleListItem> snippets = createDummySnippets();
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);

        List<NewTabPageListItem> loadedItems = new ArrayList<>(mNtpAdapter.getItems());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING,
                mNtpAdapter.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES,
                Arrays.asList(new SnippetArticleListItem[] {new SnippetArticleListItem(
                        "foo", "title1", "pub1", "txt1", "foo", "bar", 0, 0, 0)}));
        assertEquals(loadedItems, mNtpAdapter.getItems());
    }

    /**
     * Tests that the adapter keeps listening for snippet updates if it didn't get anything from
     * a previous fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoadingInitiallyEmpty() {
        // If we don't get anything, we should be in the same situation as the initial one.
        mSnippetsSource.setSuggestionsForCategory(
                KnownCategories.ARTICLES, new ArrayList<SnippetArticleListItem>());
        assertEquals(EMPTY_STATE_ELEMENTS_COUNT, mNtpAdapter.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, mNtpAdapter.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_PROGRESS, mNtpAdapter.getItemViewType(3));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, mNtpAdapter.getItemViewType(4));

        // We should load new snippets when we get notified about them.
        List<SnippetArticleListItem> snippets = createDummySnippets();
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        List<NewTabPageListItem> loadedItems = new ArrayList<>(mNtpAdapter.getItems());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING,
                mNtpAdapter.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES,
                Arrays.asList(new SnippetArticleListItem[] {new SnippetArticleListItem(
                        "foo", "title1", "pub1", "txt1", "foo", "bar", 0, 0, 0)}));
        assertEquals(loadedItems, mNtpAdapter.getItems());
    }

    /**
     * Tests that the adapter clears the snippets when asked to.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetClearing() {
        List<SnippetArticleListItem> snippets = createDummySnippets();
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(PERMANENT_ELEMENTS_COUNT + snippets.size(), mNtpAdapter.getItemCount());

        // If we get told that snippets are enabled, we just leave the current
        // ones there and not clear.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        assertEquals(PERMANENT_ELEMENTS_COUNT + snippets.size(), mNtpAdapter.getItemCount());

        // When snippets are disabled, we clear them and we should go back to
        // the situation with the status card.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.SIGNED_OUT);
        assertEquals(EMPTY_STATE_ELEMENTS_COUNT, mNtpAdapter.getItemCount());

        // The adapter should now be waiting for new snippets.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(PERMANENT_ELEMENTS_COUNT + snippets.size(), mNtpAdapter.getItemCount());
    }

    /**
     * Tests that the adapter loads snippets only when the status is favorable.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoadingBlock() {
        List<SnippetArticleListItem> snippets = createDummySnippets();

        // By default, status is INITIALIZING, so we can load snippets
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(PERMANENT_ELEMENTS_COUNT + snippets.size(), mNtpAdapter.getItemCount());

        // If we have snippets, we should not load the new list.
        snippets.add(new SnippetArticleListItem("https://site.com/url1", "title1", "pub1", "txt1",
                "https://site.com/url1", "https://amp.site.com/url1", 0, 0, 0));
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(PERMANENT_ELEMENTS_COUNT + snippets.size() - 1, mNtpAdapter.getItemCount());

        // When snippets are disabled, we should not be able to load them
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.SIGNED_OUT);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(EMPTY_STATE_ELEMENTS_COUNT, mNtpAdapter.getItemCount());

        // INITIALIZING lets us load snippets still.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.INITIALIZING);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(PERMANENT_ELEMENTS_COUNT + snippets.size(), mNtpAdapter.getItemCount());

        // The adapter should now be waiting for new snippets.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(PERMANENT_ELEMENTS_COUNT + snippets.size(), mNtpAdapter.getItemCount());
    }

    /**
     * Tests how the loading indicator reacts to status changes.
     */
    @Test
    @Feature({"Ntp"})
    public void testProgressIndicatorDisplay() {
        int progressPos = mNtpAdapter.getBottomSpacerPosition() - 1;
        ProgressListItem progress = (ProgressListItem) mNtpAdapter.getItems().get(progressPos);

        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.INITIALIZING);
        assertTrue(progress.isVisible());

        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        assertFalse(progress.isVisible());

        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE_LOADING);
        assertTrue(progress.isVisible());

        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.NOT_PROVIDED);
        assertFalse(progress.isVisible());

        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertFalse(progress.isVisible());

        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.SIGNED_OUT);
        assertFalse(progress.isVisible());

        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.LOADING_ERROR);
        assertFalse(progress.isVisible());
    }

    private List<SnippetArticleListItem> createDummySnippets() {
        List<SnippetArticleListItem> snippets = new ArrayList<>();
        snippets.add(new SnippetArticleListItem("https://site.com/url1", "title1", "pub1", "txt1",
                "https://site.com/url1", "https://amp.site.com/url1", 0, 0, 0));
        snippets.add(new SnippetArticleListItem("https://site.com/url2", "title2", "pub2", "txt2",
                "https://site.com/url2", "https://amp.site.com/url2", 0, 0, 0));
        snippets.add(new SnippetArticleListItem("https://site.com/url3", "title3", "pub3", "txt3",
                "https://site.com/url3", "https://amp.site.com/url3", 0, 0, 0));
        return snippets;
    }
}
