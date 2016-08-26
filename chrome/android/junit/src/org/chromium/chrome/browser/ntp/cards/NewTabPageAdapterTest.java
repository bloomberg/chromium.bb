// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.FakeSuggestionsSource;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link NewTabPageAdapter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageAdapterTest {
    @Before
    public void init() {
        org.robolectric.shadows.ShadowLog.stream = System.out;
    }

    private FakeSuggestionsSource mSnippetsSource = new FakeSuggestionsSource();
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

    /**
     * To be used with {@link #assertItemsFor(int...)}, for a section that is hidden.
     * @return The number of items that should be used by the adapter to represent this section.
     */
    private int sectionHidden() {
        return 0; // The section returns no items.
    }

    /**
     * To be used with {@link #assertItemsFor(int...)}, for a section with button that has no
     * suggestions and instead displays a status card.
     * @return The number of items that should be used by the adapter to represent this section.
     */
    private int sectionWithStatusCardAndMoreButton() {
        return 4; // Header, status card, More button, progress indicator.
    }

    @Before
    public void setUp() {
        RecordHistogram.disableForTests();
        RecordUserAction.disableForTests();

        mSnippetsSource = new FakeSuggestionsSource();
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.INITIALIZING);
        mSnippetsSource.setInfoForCategory(
                KnownCategories.ARTICLES,
                new SuggestionsCategoryInfo(
                        "Articles for you", ContentSuggestionsCardLayout.FULL_CARD, false, true));
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
        assertEquals(NewTabPageItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(NewTabPageItem.VIEW_TYPE_STATUS, mNtpAdapter.getItemViewType(2));
        assertEquals(NewTabPageItem.VIEW_TYPE_PROGRESS, mNtpAdapter.getItemViewType(3));
        assertEquals(NewTabPageItem.VIEW_TYPE_SPACING, mNtpAdapter.getItemViewType(4));

        List<SnippetArticle> snippets = createDummySnippets(3);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);

        List<NewTabPageItem> loadedItems = new ArrayList<>(mNtpAdapter.getItems());
        assertEquals(NewTabPageItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        // From the loadedItems, cut out aboveTheFold and header from the front,
        // and bottom spacer from the back.
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(NewTabPageItem.VIEW_TYPE_SPACING,
                mNtpAdapter.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES,
                Arrays.asList(new SnippetArticle[] {new SnippetArticle(
                        0, "foo", "title1", "pub1", "txt1", "foo", "bar", 0, 0, 0,
                        ContentSuggestionsCardLayout.FULL_CARD)}));
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
                KnownCategories.ARTICLES, new ArrayList<SnippetArticle>());
        assertItemsFor(sectionWithStatusCard());
        assertEquals(NewTabPageItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        assertEquals(NewTabPageItem.VIEW_TYPE_STATUS, mNtpAdapter.getItemViewType(2));
        assertEquals(NewTabPageItem.VIEW_TYPE_PROGRESS, mNtpAdapter.getItemViewType(3));
        assertEquals(NewTabPageItem.VIEW_TYPE_SPACING, mNtpAdapter.getItemViewType(4));

        // We should load new snippets when we get notified about them.
        List<SnippetArticle> snippets = createDummySnippets(5);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        List<NewTabPageItem> loadedItems = new ArrayList<>(mNtpAdapter.getItems());
        assertEquals(NewTabPageItem.VIEW_TYPE_ABOVE_THE_FOLD, mNtpAdapter.getItemViewType(0));
        assertEquals(NewTabPageItem.VIEW_TYPE_HEADER, mNtpAdapter.getItemViewType(1));
        // From the loadedItems, cut out aboveTheFold and header from the front,
        // and bottom spacer from the back.
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(NewTabPageItem.VIEW_TYPE_SPACING,
                mNtpAdapter.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES,
                Arrays.asList(new SnippetArticle[] {new SnippetArticle(
                        0, "foo", "title1", "pub1", "txt1", "foo", "bar", 0, 0, 0,
                        ContentSuggestionsCardLayout.FULL_CARD)}));
        assertEquals(loadedItems, mNtpAdapter.getItems());
    }

    /**
     * Tests that the adapter clears the snippets when asked to.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetClearing() {
        List<SnippetArticle> snippets = createDummySnippets(4);
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
        List<SnippetArticle> snippets = createDummySnippets(3);

        // By default, status is INITIALIZING, so we can load snippets
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(3));

        // If we have snippets, we should not load the new list (i.e. the extra item does *not*
        // appear).
        snippets.add(new SnippetArticle(0, "https://site.com/url1", "title1", "pub1", "txt1",
                "https://site.com/url1", "https://amp.site.com/url1", 0, 0, 0,
                ContentSuggestionsCardLayout.FULL_CARD));
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
        ProgressItem progress = (ProgressItem) mNtpAdapter.getItems().get(progressPos);

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
        List<SnippetArticle> snippets = createDummySnippets(5);
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
        mNtpAdapter = new NewTabPageAdapter(null, null, mSnippetsSource, null);
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
        List<SnippetArticle> snippets = createDummySnippets(4);
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
    public void testSectionVisibleIfEmpty() {
        final int category = 42;
        final int sectionIdx = 1; // section 0 is the above-the-fold item, we test the one after.
        final List<SnippetArticle> articles = Collections.unmodifiableList(createDummySnippets(3));
        FakeSuggestionsSource suggestionsSource;
        SuggestionsSection section;

        // Part 1: VisibleIfEmpty = true
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                category, new SuggestionsCategoryInfo(
                                  "", ContentSuggestionsCardLayout.MINIMAL_CARD, false, true));

        // 1.1 - Initial state
        mNtpAdapter = new NewTabPageAdapter(null, null, suggestionsSource, null);
        assertItemsFor(sectionWithStatusCard());

        // 1.2 - With suggestions
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor(section(3));

        // 1.3 - When all suggestions are dismissed
        assertEquals(SuggestionsSection.class, mNtpAdapter.getGroups().get(sectionIdx).getClass());
        section = (SuggestionsSection) mNtpAdapter.getGroups().get(sectionIdx);
        assertEquals(section(3), section.getItems().size());
        section.removeSuggestion(articles.get(0));
        section.removeSuggestion(articles.get(1));
        section.removeSuggestion(articles.get(2));
        assertItemsFor(sectionWithStatusCard());

        // Part 2: VisibleIfEmpty = false
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                category, new SuggestionsCategoryInfo(
                                  "", ContentSuggestionsCardLayout.MINIMAL_CARD, false, false));

        // 2.1 - Initial state
        mNtpAdapter = new NewTabPageAdapter(null, null, suggestionsSource, null);
        assertItemsFor();

        // 2.2 - With suggestions
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor();

        // 2.3 - When all suggestions are dismissed - N/A, suggestions don't get added.
    }

    /**
     * Tests that the more button is shown for sections that declare it.
     */
    @Test
    @Feature({"Ntp"})
    public void testMoreButton() {
        final int category = 42;
        final int sectionIdx = 1; // section 0 is the above the fold, we test the one after.
        final List<SnippetArticle> articles = Collections.unmodifiableList(createDummySnippets(3));
        FakeSuggestionsSource suggestionsSource;
        SuggestionsSection section;

        // Part 1: ShowMoreButton = true
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                category, new SuggestionsCategoryInfo(
                                  "", ContentSuggestionsCardLayout.MINIMAL_CARD, true, true));

        // 1.1 - Initial state.
        mNtpAdapter = new NewTabPageAdapter(null, null, suggestionsSource, null);
        assertItemsFor(sectionWithStatusCardAndMoreButton());

        // 1.2 - With suggestions.
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor(sectionWithMoreButton(3));

        // 1.3 - When all suggestions are dismissed.
        assertEquals(SuggestionsSection.class, mNtpAdapter.getGroups().get(sectionIdx).getClass());
        section = (SuggestionsSection) mNtpAdapter.getGroups().get(sectionIdx);
        assertEquals(sectionWithMoreButton(3), section.getItems().size());
        section.removeSuggestion(articles.get(0));
        section.removeSuggestion(articles.get(1));
        section.removeSuggestion(articles.get(2));
        assertItemsFor(sectionWithStatusCardAndMoreButton());

        // Part 1: ShowMoreButton = false
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                category, new SuggestionsCategoryInfo(
                                  "", ContentSuggestionsCardLayout.MINIMAL_CARD, false, true));

        // 2.1 - Initial state.
        mNtpAdapter = new NewTabPageAdapter(null, null, suggestionsSource, null);
        assertItemsFor(sectionWithStatusCard());

        // 2.2 - With suggestions.
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor(section(3));

        // 2.3 - When all suggestions are dismissed.
        assertEquals(SuggestionsSection.class, mNtpAdapter.getGroups().get(sectionIdx).getClass());
        section = (SuggestionsSection) mNtpAdapter.getGroups().get(sectionIdx);
        assertEquals(section(3), section.getItems().size());
        section.removeSuggestion(articles.get(0));
        section.removeSuggestion(articles.get(1));
        section.removeSuggestion(articles.get(2));
        assertItemsFor(sectionWithStatusCard());
    }

    /**
     * Tests that invalidated suggestions are immediately removed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionInvalidated() {
        List<SnippetArticle> articles = createDummySnippets(3);
        mSnippetsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSnippetsSource.setSuggestionsForCategory(KnownCategories.ARTICLES, articles);
        assertItemsFor(section(3));
        assertEquals(articles, mNtpAdapter.getItems().subList(2, 5));

        SnippetArticle removed = articles.remove(1);
        mSnippetsSource.fireSuggestionInvalidated(KnownCategories.ARTICLES, removed.mId);
        assertEquals(articles, mNtpAdapter.getItems().subList(2, 4));
    }

    /**
     * Tests that the order of the categories is kept.
     */
    @Test
    @Feature({"Ntp"})
    public void testCategoryOrder() {
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        registerCategory(suggestionsSource, KnownCategories.ARTICLES, 0);
        registerCategory(suggestionsSource, KnownCategories.BOOKMARKS, 0);
        registerCategory(suggestionsSource, KnownCategories.PHYSICAL_WEB_PAGES, 0);
        registerCategory(suggestionsSource, KnownCategories.DOWNLOADS, 0);

        NewTabPageAdapter ntpAdapter = new NewTabPageAdapter(null, null, suggestionsSource, null);
        List<ItemGroup> groups = ntpAdapter.getGroups();

        assertEquals(6, groups.size());
        assertEquals(AboveTheFoldItem.class, groups.get(0).getClass());
        assertEquals(SuggestionsSection.class, groups.get(1).getClass());
        assertEquals(KnownCategories.ARTICLES, getCategory(groups.get(1)));
        assertEquals(SuggestionsSection.class, groups.get(2).getClass());
        assertEquals(KnownCategories.BOOKMARKS, getCategory(groups.get(2)));
        assertEquals(SuggestionsSection.class, groups.get(3).getClass());
        assertEquals(KnownCategories.PHYSICAL_WEB_PAGES, getCategory(groups.get(3)));
        assertEquals(SuggestionsSection.class, groups.get(4).getClass());
        assertEquals(KnownCategories.DOWNLOADS, getCategory(groups.get(4)));

        // With a different order.
        suggestionsSource = new FakeSuggestionsSource();
        registerCategory(suggestionsSource, KnownCategories.ARTICLES, 0);
        registerCategory(suggestionsSource, KnownCategories.PHYSICAL_WEB_PAGES, 0);
        registerCategory(suggestionsSource, KnownCategories.DOWNLOADS, 0);
        registerCategory(suggestionsSource, KnownCategories.BOOKMARKS, 0);

        ntpAdapter = new NewTabPageAdapter(null, null, suggestionsSource, null);
        groups = ntpAdapter.getGroups();

        assertEquals(6, groups.size());
        assertEquals(AboveTheFoldItem.class, groups.get(0).getClass());
        assertEquals(SuggestionsSection.class, groups.get(1).getClass());
        assertEquals(KnownCategories.ARTICLES, getCategory(groups.get(1)));
        assertEquals(SuggestionsSection.class, groups.get(2).getClass());
        assertEquals(KnownCategories.PHYSICAL_WEB_PAGES, getCategory(groups.get(2)));
        assertEquals(SuggestionsSection.class, groups.get(3).getClass());
        assertEquals(KnownCategories.DOWNLOADS, getCategory(groups.get(3)));
        assertEquals(SuggestionsSection.class, groups.get(4).getClass());
        assertEquals(KnownCategories.BOOKMARKS, getCategory(groups.get(4)));

        // With unknown categories.
        suggestionsSource = new FakeSuggestionsSource();
        registerCategory(suggestionsSource, KnownCategories.ARTICLES, 0);
        registerCategory(suggestionsSource, KnownCategories.PHYSICAL_WEB_PAGES, 0);
        registerCategory(suggestionsSource, KnownCategories.DOWNLOADS, 0);

        ntpAdapter = new NewTabPageAdapter(null, null, suggestionsSource, null);

        // The adapter is already initialised, it will not accept new categories anymore.
        registerCategory(suggestionsSource, 42, 1);
        registerCategory(suggestionsSource, KnownCategories.BOOKMARKS, 1);

        groups = ntpAdapter.getGroups();

        assertEquals(5, groups.size());
        assertEquals(AboveTheFoldItem.class, groups.get(0).getClass());
        assertEquals(SuggestionsSection.class, groups.get(1).getClass());
        assertEquals(KnownCategories.ARTICLES, getCategory(groups.get(1)));
        assertEquals(SuggestionsSection.class, groups.get(2).getClass());
        assertEquals(KnownCategories.PHYSICAL_WEB_PAGES, getCategory(groups.get(2)));
        assertEquals(SuggestionsSection.class, groups.get(3).getClass());
        assertEquals(KnownCategories.DOWNLOADS, getCategory(groups.get(3)));
    }

    private List<SnippetArticle> createDummySnippets(int count) {
        List<SnippetArticle> snippets = new ArrayList<>();
        for (int index = 0; index < count; index++) {
            snippets.add(new SnippetArticle(0, "https://site.com/url" + index, "title" + index,
                    "pub" + index, "txt" + index, "https://site.com/url" + index,
                    "https://amp.site.com/url" + index, 0, 0, 0,
                    ContentSuggestionsCardLayout.FULL_CARD));
        }
        return snippets;
    }

    /** Registers the category with hasMoreButton=false and showIfEmpty=true*/
    private void registerCategory(FakeSuggestionsSource suggestionsSource,
            @CategoryInt int category, int suggestionCount) {
        // FakeSuggestionSource does not provide snippets if the category's status is not available.
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        // Important: showIfEmpty flag to true.
        suggestionsSource.setInfoForCategory(
                category, new SuggestionsCategoryInfo(
                                  "", ContentSuggestionsCardLayout.FULL_CARD, false, true));
        suggestionsSource.setSuggestionsForCategory(category, createDummySnippets(suggestionCount));
    }

    private int getCategory(ItemGroup itemGroup) {
        return ((SuggestionsSection) itemGroup).getCategory();
    }
}
