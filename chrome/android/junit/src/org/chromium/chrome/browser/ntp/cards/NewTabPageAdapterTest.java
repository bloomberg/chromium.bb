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
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories.KnownCategoriesEnum;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetsSource;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;


/**
 * Unit tests for {@link NewTabPageAdapter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageAdapterTest {
    private static class FakeSnippetsSource implements SnippetsSource {
        private SnippetsSource.SnippetsObserver mObserver;
        private final Map<Integer, Integer> mCategoryStatus = new HashMap<>();

        public void setStatusForCategory(@KnownCategoriesEnum int category,
                @CategoryStatusEnum int status) {
            mCategoryStatus.put(category, status);
            if (mObserver != null) mObserver.onCategoryStatusChanged(category, status);
        }

        public void setSnippetsForCategory(@KnownCategoriesEnum int category,
                List<SnippetArticleListItem> snippets) {
            mObserver.onSuggestionsReceived(category, snippets);
        }

        @Override
        public void discardSnippet(SnippetArticleListItem snippet) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void fetchSnippetImage(SnippetArticleListItem snippet, Callback<Bitmap> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void getSnippedVisited(SnippetArticleListItem snippet, Callback<Boolean> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void setObserver(SnippetsObserver observer) {
            mObserver = observer;
        }

        @CategoryStatusEnum
        @Override
        public int getCategoryStatus(@KnownCategoriesEnum int category) {
            return mCategoryStatus.get(category);
        }
    }

    private FakeSnippetsSource mSnippetsSource = new FakeSnippetsSource();
    private NewTabPageAdapter mNtpAdapter;

    @Before
    public void setUp() {
        RecordHistogram.disableForTests();

        mSnippetsSource = new FakeSnippetsSource();
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.INITIALIZING);
        mNtpAdapter = new NewTabPageAdapter(null, null, mSnippetsSource, null);
    }

    /**
     * Tests the content of the adapter under standard conditions: on start and after a snippet
     * fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoading() {
        assertEquals(5, mNtpAdapter.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, mNtpAdapter.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_PROGRESS, mNtpAdapter.getItemViewType(3));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, mNtpAdapter.getItemViewType(4));

        List<SnippetArticleListItem> snippets = createDummySnippets();
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);

        List<NewTabPageListItem> loadedItems = new ArrayList<>(mNtpAdapter.getItems());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING,
                mNtpAdapter.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES,
                Arrays.asList(new SnippetArticleListItem[] { new SnippetArticleListItem(
                        "foo", "title1", "pub1", "txt1", "foo", "bar", 0, 0, 0) }));
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
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES,
                new ArrayList<SnippetArticleListItem>());
        assertEquals(5, mNtpAdapter.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, mNtpAdapter.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_PROGRESS, mNtpAdapter.getItemViewType(3));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, mNtpAdapter.getItemViewType(4));

        // We should load new snippets when we get notified about them.
        List<SnippetArticleListItem> snippets = createDummySnippets();
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);
        List<NewTabPageListItem> loadedItems = new ArrayList<>(mNtpAdapter.getItems());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING,
                mNtpAdapter.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsSource.setSnippetsForCategory(
                KnownCategories.ARTICLES,
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
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(3 + snippets.size(), mNtpAdapter.getItemCount());

        // If we get told that snippets are enabled, we just leave the current
        // ones there and not clear.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        assertEquals(3 + snippets.size(), mNtpAdapter.getItemCount());

        // When snippets are disabled, we clear them and we should go back to
        // the situation with the status card.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.SIGNED_OUT);
        assertEquals(5, mNtpAdapter.getItemCount());

        // The adapter should now be waiting for new snippets.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(3 + snippets.size(), mNtpAdapter.getItemCount());
    }

    /**
     * Tests that the adapter loads snippets only when the status is favorable.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoadingBlock() {
        List<SnippetArticleListItem> snippets = createDummySnippets();

        // By default, status is INITIALIZING, so we can load snippets
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(3 + snippets.size(), mNtpAdapter.getItemCount());

        // If we have snippets, we should not load the new list.
        snippets.add(new SnippetArticleListItem("https://site.com/url1", "title1", "pub1", "txt1",
                "https://site.com/url1", "https://amp.site.com/url1", 0, 0, 0));
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(3 + snippets.size() - 1, mNtpAdapter.getItemCount());

        // When snippets are disabled, we should not be able to load them
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.SIGNED_OUT);
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(5, mNtpAdapter.getItemCount());

        // INITIALIZING lets us load snippets still.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.INITIALIZING);
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(3 + snippets.size(), mNtpAdapter.getItemCount());

        // The adapter should now be waiting for new snippets.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        mSnippetsSource.setSnippetsForCategory(KnownCategories.ARTICLES, snippets);
        assertEquals(3 + snippets.size(), mNtpAdapter.getItemCount());
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
