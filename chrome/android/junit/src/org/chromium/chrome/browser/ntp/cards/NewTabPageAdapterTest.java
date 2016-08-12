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
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
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
            // Copy the suggestions list so that it can't be modified anymore.
            mSuggestions.put(category, new ArrayList<>(suggestions));
            if (mObserver != null) mObserver.onNewSuggestions(category);
        }

        public void setInfoForCategory(@CategoryInt int category, SuggestionsCategoryInfo info) {
            mCategoryInfo.put(category, info);
        }

        public void silentlyRemoveCategory(int category) {
            mSuggestions.remove(category);
            mCategoryStatus.remove(category);
            mCategoryInfo.remove(category);
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
            if (!SnippetsBridge.isCategoryStatusAvailable(mCategoryStatus.get(category))) {
                return Collections.emptyList();
            }
            List<SnippetArticleListItem> result = mSuggestions.get(category);
            return result == null ? Collections.<SnippetArticleListItem>emptyList() : result;
        }
    }

    private FakeSnippetsSource mSnippetsSource = new FakeSnippetsSource();
    private NewTabPageAdapter mNtpAdapter;

    /**
     * Asserts that {@link #mNtpAdapter}.{@link NewTabPageAdapter#getItemCount()} corresponds to an
     * NTP with the given sections in it.
     * @param sections A list of sections, each represented by the number of items that are required
     *                 to represent it on the UI. For readability, these numbers should be generated
     *                 with the methods below.
     */
    private void assertItemsFor(int... sections) {
        int expectedCount = 1; // above-the-fold.
        for (int section : sections) expectedCount += section;
        if (sections.length > 0) expectedCount += 1; // bottom spacer.
        int actualCount = mNtpAdapter.getItemCount();
        assertEquals("Expected " + expectedCount + " items, but the following " + actualCount
                        + " were present: " + mNtpAdapter.getItems(),
                expectedCount, mNtpAdapter.getItemCount());
    }

    /**
     * To be used with {@link #assertItemsFor(int...)}, for a section with {@code numSuggestions}
     * cards in it.
     * @param numSuggestions The number of suggestions in the section. If there are zero, use either
     *                       no section at all (if it is not displayed) or
     *                       {@link #sectionWithStatusCard()}.
     * @return The number of items that should be used by the adapter to represent this section.
     */
    private int section(int numSuggestions) {
        assert numSuggestions > 0;
        return 1 + numSuggestions; // Header and the content.
    }

    /**
     * To be used with {@link #assertItemsFor(int...)}, for a section with {@code numSuggestions}
     * cards and a more-button.
     * @param numSuggestions The number of suggestions in the section. If this is zero, the
     *                       more-button is still shown.
     *                       TODO(pke): In the future, we additionally show an empty-card if
     *                       numSuggestions is zero.
     * @return The number of items that should be used by the adapter to represent this section.
     */
    private int sectionWithMoreButton(int numSuggestions) {
        return 1 + numSuggestions + 1; // Header, the content and the more-button.
    }

    /**
     * To be used with {@link #assertItemsFor(int...)}, for a section that has no suggestions, but
     * a status card to be displayed.
     * @return The number of items that should be used by the adapter to represent this section.
     */
    private int sectionWithStatusCard() {
        return 3; // Header, status card and progress indicator.
    }

    @Before
    public void setUp() {
        RecordHistogram.disableForTests();

        mSnippetsSource = new FakeSnippetsSource();
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.INITIALIZING);
        mSnippetsSource.setInfoForCategory(
                KnownCategories.ARTICLES, new SuggestionsCategoryInfo("Articles for you",
                                                  ContentSuggestionsCardLayout.FULL_CARD, false));
        mNtpAdapter = new NewTabPageAdapter(null, null, mSnippetsSource, null);
    }

    /**
     * Tests the content of the adapter under standard conditions: on start and after a snippet
     * fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoading() {
        assertItemsFor(sectionWithStatusCard());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, mNtpAdapter.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_PROGRESS, mNtpAdapter.getItemViewType(3));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, mNtpAdapter.getItemViewType(4));

        List<SnippetArticleListItem> snippets = createDummySnippets(3);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);

        List<NewTabPageListItem> loadedItems = new ArrayList<>(mNtpAdapter.getItems());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        // From the loadedItems, cut out aboveTheFold and header from the front,
        // and bottom spacer from the back.
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
        assertItemsFor(sectionWithStatusCard());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, mNtpAdapter.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_PROGRESS, mNtpAdapter.getItemViewType(3));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, mNtpAdapter.getItemViewType(4));

        // We should load new snippets when we get notified about them.
        List<SnippetArticleListItem> snippets = createDummySnippets(5);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        List<NewTabPageListItem> loadedItems = new ArrayList<>(mNtpAdapter.getItems());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        // From the loadedItems, cut out aboveTheFold and header from the front,
        // and bottom spacer from the back.
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
        List<SnippetArticleListItem> snippets = createDummySnippets(4);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(4));

        // If we get told that snippets are enabled, we just leave the current
        // ones there and not clear.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        assertItemsFor(section(4));

        // When snippets are disabled, we clear them and we should go back to
        // the situation with the status card.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.SIGNED_OUT);
        assertItemsFor(sectionWithStatusCard());

        // The adapter should now be waiting for new snippets.
        snippets = createDummySnippets(6);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(6));
    }

    /**
     * Tests that the adapter loads snippets only when the status is favorable.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoadingBlock() {
        List<SnippetArticleListItem> snippets = createDummySnippets(3);

        // By default, status is INITIALIZING, so we can load snippets
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(3));

        // If we have snippets, we should not load the new list (i.e. the extra item does *not*
        // appear).
        snippets.add(new SnippetArticleListItem("https://site.com/url1", "title1", "pub1", "txt1",
                "https://site.com/url1", "https://amp.site.com/url1", 0, 0, 0));
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(3));

        // When snippets are disabled, we should not be able to load them.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.SIGNED_OUT);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(sectionWithStatusCard());

        // INITIALIZING lets us load snippets still.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.INITIALIZING);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(sectionWithStatusCard());

        // The adapter should now be waiting for new snippets and the fourth one should appear.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(4));
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
                CategoryStatus.SIGNED_OUT);
        assertFalse(progress.isVisible());
    }

    /**
     * Tests that the entire section disappears if its status switches to LOADING_ERROR or
     * CATEGORY_EXPLICITLY_DISABLED. Also tests that they are not shown when the NTP reloads.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionClearingWhenUnavailable() {
        List<SnippetArticleListItem> snippets = createDummySnippets(5);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(5));

        // When the category goes away with a hard error, the section is cleared from the UI.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES,
                CategoryStatus.LOADING_ERROR);
        assertItemsFor();

        // Same when loading a new NTP.
        mNtpAdapter = new NewTabPageAdapter(null, null, mSnippetsSource, null);
        assertItemsFor();

        // Same for CATEGORY_EXPLICITLY_DISABLED.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(5));
        mSnippetsSource.setStatusForCategory(
                KnownCategories.ARTICLES, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertItemsFor();

        // Same when loading a new NTP.
        mNtpAdapter = new NewTabPageAdapter(null, null, mSnippetsSource, null);
        assertItemsFor();
    }

    /**
     * Tests that the UI remains untouched if a category switches to NOT_PROVIDED.
     */
    @Test
    @Feature({"Ntp"})
    public void testUIUntouchedWhenNotProvided() {
        List<SnippetArticleListItem> snippets = createDummySnippets(4);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(4));

        // When the category switches to NOT_PROVIDED, UI stays the same.
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.NOT_PROVIDED);
        mSnippetsSource.silentlyRemoveCategory(KnownCategories.ARTICLES);
        assertItemsFor(section(4));

        // But it disappears when loading a new NTP.
        mNtpAdapter = new NewTabPageAdapter(null, null, mSnippetsSource, null);
        assertItemsFor();
    }

    @Test
    @Feature({"Ntp"})
    public void testMoreButton() {
        List<SnippetArticleListItem> articles = createDummySnippets(3);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, articles);
        assertItemsFor(section(3));

        List<SnippetArticleListItem> bookmarks = createDummySnippets(10);
        mSnippetsSource.setInfoForCategory(KnownCategories.BOOKMARKS,
                new SuggestionsCategoryInfo("Bookmarks", ContentSuggestionsCardLayout.MINIMAL_CARD,
                                                   true));
        mSnippetsSource.setStatusForCategory(KnownCategories.BOOKMARKS, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.BOOKMARKS, bookmarks);
        assertItemsFor(sectionWithMoreButton(10), section(3));
    }

    private List<SnippetArticleListItem> createDummySnippets(int count) {
        List<SnippetArticleListItem> snippets = new ArrayList<>();
        for (int index = 0; index < count; index++) {
            snippets.add(new SnippetArticleListItem("https://site.com/url" + index, "title" + index,
                    "pub" + index, "txt" + index, "https://site.com/url" + index,
                    "https://amp.site.com/url" + index, 0, 0, 0));
        }
        return snippets;
    }
}
