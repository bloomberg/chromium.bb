// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.annotation.Nullable;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem.OnMenuItemClickListener;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import static org.chromium.base.test.util.Matchers.greaterThanOrEqualTo;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils
        .createDummySuggestions;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createInfo;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.favicon.FaviconHelper.IconAvailabilityCallback;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.ntp.MostVisitedItem;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.FakeSuggestionsSource;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.profiles.MostVisitedSites.MostVisitedURLsObserver;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link NewTabPageAdapter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageAdapterTest {
    private FakeSuggestionsSource mSource;
    private NewTabPageAdapter mAdapter;
    private SigninManager mMockSigninManager;

    /**
     * Stores information about a section that should be present in the adapter.
     */
    private static class SectionDescriptor {
        public final boolean mMoreButton;
        public final boolean mStatusCard;
        public final int mNumSuggestions;

        public SectionDescriptor(boolean moreButton, boolean statusCard, int numSuggestions) {
            mMoreButton = moreButton;
            mStatusCard = statusCard;
            mNumSuggestions = numSuggestions;
            if (statusCard) {
                assertEquals(0, numSuggestions);
            } else {
                assertTrue(numSuggestions > 0);
            }
        }
    }

    /**
     * Checks the list of items from the adapter against a sequence of expectation, which is
     * expressed as a sequence of calls to the {@link #expect} methods.
     */
    private static class ItemsMatcher { // TODO(pke): Find better name.
        private final NewTabPageAdapter mAdapter;
        private int mCurrentIndex;

        public ItemsMatcher(NewTabPageAdapter adapter, int startingIndex) {
            mAdapter = adapter;
            mCurrentIndex = startingIndex;
        }

        public void expect(@NewTabPageItem.ViewType int expectedItemType) {
            if (mCurrentIndex >= mAdapter.getItemCount()) {
                fail("Expected item of type " + expectedItemType + " but encountered end of list");
            }
            @NewTabPageItem.ViewType int itemType = mAdapter.getItemViewType(mCurrentIndex);
            assertEquals("Type mismatch at position " + mCurrentIndex, expectedItemType, itemType);
            mCurrentIndex++;
        }

        public void expect(SectionDescriptor descriptor) {
            expect(NewTabPageItem.VIEW_TYPE_HEADER);
            if (descriptor.mStatusCard) {
                expect(NewTabPageItem.VIEW_TYPE_STATUS);
                expect(NewTabPageItem.VIEW_TYPE_ACTION);
                expect(NewTabPageItem.VIEW_TYPE_PROGRESS);
            } else {
                for (int i = 1; i <= descriptor.mNumSuggestions; i++) {
                    expect(NewTabPageItem.VIEW_TYPE_SNIPPET);
                }
                if (descriptor.mMoreButton) {
                    expect(NewTabPageItem.VIEW_TYPE_ACTION);
                }
            }
        }

        public void expectPosition(int expectedPosition) {
            assertEquals(expectedPosition, mCurrentIndex);
        }
    }

    /**
     * Asserts that the given itemGroup is a {@link SuggestionsSection} that matches the given
     * {@link SectionDescriptor}.
     * @param descriptor The section descriptor to match against.
     * @param itemGroup The items from the adapter.
     */
    private void assertMatches(SectionDescriptor descriptor, ItemGroup itemGroup) {
        int offset = mAdapter.getGroupPositionOffset(itemGroup);
        ItemsMatcher matcher = new ItemsMatcher(mAdapter, offset);
        matcher.expect(descriptor);
        matcher.expectPosition(offset + itemGroup.getItems().size());
    }

    /**
     * Asserts that {@link #mAdapter}.{@link NewTabPageAdapter#getItemCount()} corresponds to an
     * NTP with the given sections in it.
     * @param descriptors A list of descriptors, each describing a section that should be present on
     *                    the UI.
     */
    private void assertItemsFor(SectionDescriptor... descriptors) {
        ItemsMatcher matcher = new ItemsMatcher(mAdapter, 0);
        matcher.expect(NewTabPageItem.VIEW_TYPE_ABOVE_THE_FOLD);
        for (SectionDescriptor descriptor : descriptors) matcher.expect(descriptor);
        if (descriptors.length > 0) {
            matcher.expect(NewTabPageItem.VIEW_TYPE_FOOTER);
            matcher.expect(NewTabPageItem.VIEW_TYPE_SPACING);
        }
        matcher.expectPosition(mAdapter.getItemCount());
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section with
     * {@code numSuggestions} cards in it.
     * @param numSuggestions The number of suggestions in the section. If there are zero, use either
     *                       no section at all (if it is not displayed) or
     *                       {@link #sectionWithStatusCard()}.
     * @return A descriptor for the section.
     */
    private SectionDescriptor section(int numSuggestions) {
        assert numSuggestions > 0;
        return new SectionDescriptor(false, false, numSuggestions);
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section with
     * {@code numSuggestions} cards and a more-button.
     * @param numSuggestions The number of suggestions in the section. If this is zero, the
     *                       more-button is still shown.
     *                       TODO(pke): In the future, we additionally show an empty-card if
     *                       numSuggestions is zero.
     * @return A descriptor for the section.
     */
    private SectionDescriptor sectionWithMoreButton(int numSuggestions) {
        return new SectionDescriptor(true, false, numSuggestions);
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section that has no
     * suggestions, but a status card to be displayed.
     * @return A descriptor for the section.
     */
    private SectionDescriptor sectionWithStatusCard() {
        return new SectionDescriptor(false, true, 0);
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section with button that
     * has no suggestions and instead displays a status card.
     * @return A descriptor for the section.
     */
    private SectionDescriptor sectionWithStatusCardAndMoreButton() {
        return new SectionDescriptor(true, true, 0);
    }

    @Before
    public void setUp() {
        // Initialise the sign in state. We will be signed in by default in the tests.
        assertFalse(ChromePreferenceManager.getInstance(RuntimeEnvironment.application)
                            .getNewTabPageSigninPromoDismissed());
        mMockSigninManager = mock(SigninManager.class);
        SigninManager.setInstanceForTesting(mMockSigninManager);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(true);

        RecordHistogram.disableForTests();
        RecordUserAction.disableForTests();

        @CategoryInt
        final int category = KnownCategories.ARTICLES;
        mSource = new FakeSuggestionsSource();
        mSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        mSource.setInfoForCategory(category, createInfo(category, false, true));
        mAdapter = NewTabPageAdapter.create(new MockNewTabPageManager(mSource), null, null);
    }

    @After
    public void tearDown() {
        SigninManager.setInstanceForTesting(null);
        ChromePreferenceManager.getInstance(RuntimeEnvironment.application)
                .setNewTabPageSigninPromoDismissed(false);
    }

    /**
     * Tests the content of the adapter under standard conditions: on start and after a suggestions
     * fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoading() {
        assertItemsFor(sectionWithStatusCard());

        final int numSuggestions = 3;
        List<SnippetArticle> suggestions = createDummySuggestions(numSuggestions);
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);

        assertItemsFor(section(numSuggestions));

        // The adapter should ignore any new incoming data.
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES,
                Arrays.asList(new SnippetArticle[] {new SnippetArticle(0, "foo", "title1", "pub1",
                        "txt1", "foo", "bar", 0, 0, 0, ContentSuggestionsCardLayout.FULL_CARD)}));

        assertItemsFor(section(numSuggestions));
    }

    /**
     * Tests that the adapter keeps listening for suggestion updates if it didn't get anything from
     * a previous fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoadingInitiallyEmpty() {
        // If we don't get anything, we should be in the same situation as the initial one.
        mSource.setSuggestionsForCategory(
                KnownCategories.ARTICLES, new ArrayList<SnippetArticle>());
        assertItemsFor(sectionWithStatusCard());

        // We should load new suggestions when we get notified about them.
        final int numSuggestions = 5;
        List<SnippetArticle> suggestions = createDummySuggestions(numSuggestions);
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);

        assertItemsFor(section(numSuggestions));

        // The adapter should ignore any new incoming data.
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES,
                Arrays.asList(new SnippetArticle[] {new SnippetArticle(0, "foo", "title1", "pub1",
                        "txt1", "foo", "bar", 0, 0, 0, ContentSuggestionsCardLayout.FULL_CARD)}));
        assertItemsFor(section(numSuggestions));
    }

    /**
     * Tests that the adapter clears the suggestions when asked to.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionClearing() {
        List<SnippetArticle> suggestions = createDummySuggestions(4);
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);
        assertItemsFor(section(4));

        // If we get told that the category is enabled, we just leave the current suggestions do not
        // clear them.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        assertItemsFor(section(4));

        // When the category is disabled, the suggestions are cleared and we should go back to
        // the situation with the status card.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.SIGNED_OUT);
        assertItemsFor(sectionWithStatusCard());

        // The adapter should now be waiting for new suggestions.
        suggestions = createDummySuggestions(6);
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);
        assertItemsFor(section(6));
    }

    /**
     * Tests that the adapter loads suggestions only when the status is favorable.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoadingBlock() {
        List<SnippetArticle> suggestions = createDummySuggestions(3);

        // By default, status is INITIALIZING, so we can load suggestions.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);
        assertItemsFor(section(3));

        // If we have snippets, we should not load the new list (i.e. the extra item does *not*
        // appear).
        suggestions.add(new SnippetArticle(0, "https://site.com/url1", "title1", "pub1", "txt1",
                "https://site.com/url1", "https://amp.site.com/url1", 0, 0, 0,
                ContentSuggestionsCardLayout.FULL_CARD));
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);
        assertItemsFor(section(3));

        // When snippets are disabled, we should not be able to load them.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.SIGNED_OUT);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);
        assertItemsFor(sectionWithStatusCard());

        // INITIALIZING lets us load snippets still.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.INITIALIZING);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);
        assertItemsFor(sectionWithStatusCard());

        // The adapter should now be waiting for new snippets and the fourth one should appear.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, suggestions);
        assertItemsFor(section(4));
    }

    /**
     * Tests how the loading indicator reacts to status changes.
     */
    @Test
    @Feature({"Ntp"})
    public void testProgressIndicatorDisplay() {
        int progressPos = mAdapter.getLastContentItemPosition() - 1;
        SuggestionsSection section = (SuggestionsSection) mAdapter.getGroup(progressPos);
        List<NewTabPageItem> items = section.getItems();
        ProgressItem progress = (ProgressItem) items.get(items.size() - 1);

        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.INITIALIZING);
        assertTrue(progress.isVisible());

        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        assertFalse(progress.isVisible());

        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE_LOADING);
        assertTrue(progress.isVisible());

        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.SIGNED_OUT);
        assertFalse(progress.isVisible());
    }

    /**
     * Tests that the entire section disappears if its status switches to LOADING_ERROR or
     * CATEGORY_EXPLICITLY_DISABLED. Also tests that they are not shown when the NTP reloads.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionClearingWhenUnavailable() {
        List<SnippetArticle> snippets = createDummySuggestions(5);
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(5));

        // When the category goes away with a hard error, the section is cleared from the UI.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.LOADING_ERROR);
        assertItemsFor();

        // Same when loading a new NTP.
        mAdapter = NewTabPageAdapter.create(new MockNewTabPageManager(mSource), null, null);
        assertItemsFor();

        // Same for CATEGORY_EXPLICITLY_DISABLED.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        mAdapter = NewTabPageAdapter.create(new MockNewTabPageManager(mSource), null, null);
        assertItemsFor(section(5));
        mSource.setStatusForCategory(
                KnownCategories.ARTICLES, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertItemsFor();

        // Same when loading a new NTP.
        mAdapter = NewTabPageAdapter.create(new MockNewTabPageManager(mSource), null, null);
        assertItemsFor();
    }

    /**
     * Tests that the UI remains untouched if a category switches to NOT_PROVIDED.
     */
    @Test
    @Feature({"Ntp"})
    public void testUIUntouchedWhenNotProvided() {
        List<SnippetArticle> snippets = createDummySuggestions(4);
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, snippets);
        assertItemsFor(section(4));

        // When the category switches to NOT_PROVIDED, UI stays the same.
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.NOT_PROVIDED);
        mSource.silentlyRemoveCategory(KnownCategories.ARTICLES);
        assertItemsFor(section(4));

        // But it disappears when loading a new NTP.
        mAdapter = NewTabPageAdapter.create(new MockNewTabPageManager(mSource), null, null);
        assertItemsFor();
    }

    /** Tests whether a section stays visible if empty, if required. */
    @Test
    @Feature({"Ntp"})
    public void testSectionVisibleIfEmpty() {
        @CategoryInt
        final int category = 42;
        final int sectionIdx = 1; // section 0 is the above-the-fold item, we test the one after.
        final List<SnippetArticle> articles =
                Collections.unmodifiableList(createDummySuggestions(3));
        FakeSuggestionsSource suggestionsSource;

        // Part 1: VisibleIfEmpty = true
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(category, createInfo(category, false, true));

        // 1.1 - Initial state
        mAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(suggestionsSource), null, null);
        assertItemsFor(sectionWithStatusCard());

        // 1.2 - With suggestions
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor(section(3));

        // 1.3 - When all suggestions are dismissed
        assertEquals(SuggestionsSection.class, mAdapter.getGroups().get(sectionIdx).getClass());
        SuggestionsSection section42 = (SuggestionsSection) mAdapter.getGroups().get(sectionIdx);
        assertMatches(section(3), section42);
        section42.removeSuggestion(articles.get(0));
        section42.removeSuggestion(articles.get(1));
        section42.removeSuggestion(articles.get(2));
        assertItemsFor(sectionWithStatusCard());

        // Part 2: VisibleIfEmpty = false
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(category, createInfo(category, false, false));

        // 2.1 - Initial state
        mAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(suggestionsSource), null, null);
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
        @CategoryInt
        final int category = 42;
        final int sectionIdx = 1; // section 0 is the above the fold, we test the one after.
        final List<SnippetArticle> articles =
                Collections.unmodifiableList(createDummySuggestions(3));
        FakeSuggestionsSource suggestionsSource;
        SuggestionsSection section42;

        // Part 1: ShowMoreButton = true
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(category, createInfo(category, true, true));

        // 1.1 - Initial state.
        mAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(suggestionsSource), null, null);
        assertItemsFor(sectionWithStatusCardAndMoreButton());

        // 1.2 - With suggestions.
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor(sectionWithMoreButton(3));

        // 1.3 - When all suggestions are dismissed.
        assertEquals(SuggestionsSection.class, mAdapter.getGroups().get(sectionIdx).getClass());
        section42 = (SuggestionsSection) mAdapter.getGroups().get(sectionIdx);
        assertMatches(sectionWithMoreButton(3), section42);
        section42.removeSuggestion(articles.get(0));
        section42.removeSuggestion(articles.get(1));
        section42.removeSuggestion(articles.get(2));
        assertItemsFor(sectionWithStatusCardAndMoreButton());

        // Part 1: ShowMoreButton = false
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(category, createInfo(category, false, true));

        // 2.1 - Initial state.
        mAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(suggestionsSource), null, null);
        assertItemsFor(sectionWithStatusCard());

        // 2.2 - With suggestions.
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor(section(3));

        // 2.3 - When all suggestions are dismissed.
        assertEquals(SuggestionsSection.class, mAdapter.getGroups().get(sectionIdx).getClass());
        section42 = (SuggestionsSection) mAdapter.getGroups().get(sectionIdx);
        assertMatches(section(3), section42);
        section42.removeSuggestion(articles.get(0));
        section42.removeSuggestion(articles.get(1));
        section42.removeSuggestion(articles.get(2));
        assertItemsFor(sectionWithStatusCard());
    }

    private void assertArticlesEqual(List<SnippetArticle> articles, int start, int end) {
        assertThat(mAdapter.getItemCount(), greaterThanOrEqualTo(end));
        for (int i = start; i < end; i++) {
            assertEquals(articles.get(i - start), mAdapter.getSuggestionAt(i));
        }
    }

    /**
     * Tests that invalidated suggestions are immediately removed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionInvalidated() {
        List<SnippetArticle> articles = createDummySuggestions(3);
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, articles);
        assertItemsFor(section(3));
        assertArticlesEqual(articles, 2, 5);

        SnippetArticle removed = articles.remove(1);
        mSource.fireSuggestionInvalidated(KnownCategories.ARTICLES, removed.mIdWithinCategory);
        assertArticlesEqual(articles, 2, 4);
    }

    /**
     * Tests that the UI handles dynamically added (server-side) categories correctly.
     */
    @Test
    @Feature({"Ntp"})
    public void testDynamicCategories() {
        List<SnippetArticle> articles = createDummySuggestions(3);
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, articles);
        assertItemsFor(section(3));

        int dynamicCategory1 = 1010;
        List<SnippetArticle> dynamics1 = createDummySuggestions(5);
        mSource.setInfoForCategory(dynamicCategory1, createInfo(dynamicCategory1, true, false));
        mSource.setStatusForCategory(dynamicCategory1, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(dynamicCategory1, dynamics1);
        mAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(mSource), null, null); // Reload
        assertItemsFor(section(3), sectionWithMoreButton(5));

        int dynamicCategory2 = 1011;
        List<SnippetArticle> dynamics2 = createDummySuggestions(11);
        mSource.setInfoForCategory(dynamicCategory2, createInfo(dynamicCategory1, false, false));
        mSource.setStatusForCategory(dynamicCategory2, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(dynamicCategory2, dynamics2);
        mAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(mSource), null, null); // Reload
        assertItemsFor(section(3), sectionWithMoreButton(5), section(11));
    }

    /**
     * Tests that the order of the categories is kept.
     */
    @Test
    @Feature({"Ntp"})
    public void testCategoryOrder() {
        final int basicGroupCount = 4; // above-the-fold, sign in promo, footer, spacer.
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        registerCategory(suggestionsSource, KnownCategories.ARTICLES, 0);
        registerCategory(suggestionsSource, KnownCategories.BOOKMARKS, 0);
        registerCategory(suggestionsSource, KnownCategories.PHYSICAL_WEB_PAGES, 0);
        registerCategory(suggestionsSource, KnownCategories.DOWNLOADS, 0);

        NewTabPageAdapter ntpAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(suggestionsSource), null, null);
        List<ItemGroup> groups = ntpAdapter.getGroups();

        assertEquals(basicGroupCount + 4, groups.size());
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

        ntpAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(suggestionsSource), null, null);
        groups = ntpAdapter.getGroups();

        assertEquals(basicGroupCount + 4, groups.size());
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

        ntpAdapter =
                NewTabPageAdapter.create(new MockNewTabPageManager(suggestionsSource), null, null);

        // The adapter is already initialised, it will not accept new categories anymore.
        registerCategory(suggestionsSource, 42, 1);
        registerCategory(suggestionsSource, KnownCategories.BOOKMARKS, 1);

        groups = ntpAdapter.getGroups();

        assertEquals(basicGroupCount + 3, groups.size());
        assertEquals(AboveTheFoldItem.class, groups.get(0).getClass());
        assertEquals(SuggestionsSection.class, groups.get(1).getClass());
        assertEquals(KnownCategories.ARTICLES, getCategory(groups.get(1)));
        assertEquals(SuggestionsSection.class, groups.get(2).getClass());
        assertEquals(KnownCategories.PHYSICAL_WEB_PAGES, getCategory(groups.get(2)));
        assertEquals(SuggestionsSection.class, groups.get(3).getClass());
        assertEquals(KnownCategories.DOWNLOADS, getCategory(groups.get(3)));
    }

    @Test
    @Feature({"Ntp"})
    public void testChangeNotifications() {
        FakeSuggestionsSource suggestionsSource = spy(new FakeSuggestionsSource());
        // Allow using dismissSuggestion() instead of throwing UnsupportedOperationException.
        doNothing().when(suggestionsSource).dismissSuggestion(any(SnippetArticle.class));

        registerCategory(suggestionsSource, KnownCategories.ARTICLES, 3);
        NewTabPageAdapter adapter = spy(
                new NewTabPageAdapter(new MockNewTabPageManager(suggestionsSource), null, null));
        adapter.finishInitialization();
        doNothing().when(adapter).announceItemRemoved(anyString());

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2-4 | Sugg*3
        // 5   | Footer
        // 6   | Spacer

        adapter.dismissItem(3); // Dismiss the second suggestion of the second section.
        verify(adapter).notifyItemRemoved(3);

        // Make sure the call with the updated position works properly.
        adapter.dismissItem(3);
        verify(adapter, times(2)).notifyItemRemoved(3);

        // Dismiss the last suggestion in the section. We should now show the status card.
        adapter.dismissItem(2);
        verify(adapter).notifyItemRemoved(2);
        verify(adapter).notifyItemInserted(2);
        verify(adapter).notifyItemInserted(3);
        verify(adapter).notifyItemInserted(4);

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2   | Status
        // 3   | Action
        // 4   | Progress Indicator
        // 5   | Footer
        // 6   | Spacer

        final int newSuggestionCount = 7;
        final int changedCount = 3; // status, action and progress will be replaced by articles.
        suggestionsSource.setSuggestionsForCategory(
                KnownCategories.ARTICLES, createDummySuggestions(newSuggestionCount));
        adapter.onNewSuggestions(KnownCategories.ARTICLES);
        verify(adapter).notifyItemRangeChanged(2, changedCount);
        verify(adapter).notifyItemRangeInserted(
                2 + changedCount, newSuggestionCount - changedCount);

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2-8 | Sugg*7
        // 9   | Footer
        // 10  | Spacer

        suggestionsSource.setSuggestionsForCategory(
                KnownCategories.ARTICLES, createDummySuggestions(0));
        adapter.onCategoryStatusChanged(KnownCategories.ARTICLES, CategoryStatus.SIGNED_OUT);
        verify(adapter, times(2)).notifyItemRangeChanged(2, changedCount);
        verify(adapter).notifyItemRangeRemoved(2 + changedCount, newSuggestionCount - changedCount);
    }

    @Test
    @Feature({"Ntp"})
    public void testSigninPromo() {
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);
        MockNewTabPageManager ntpManager = new MockNewTabPageManager(mSource);
        NewTabPageAdapter adapter = NewTabPageAdapter.create(ntpManager, null, null);

        assertEquals(5, adapter.getGroups().size());
        ItemGroup signinPromoGroup = adapter.getGroup(5);

        // Adapter content:
        // Idx | Item               | Group Index
        // ----|--------------------|-------------
        // 0   | Above-the-fold     | 0
        // 1   | Header             | 1
        // 2   | Status             | 1
        // 3   | Action             | 1
        // 4   | Progress Indicator | 1
        // 5   | Sign in promo      | 2
        // 6   | Footer             | 3
        // 7   | Spacer             | 4

        assertEquals(1, signinPromoGroup.getItems().size());
        assertEquals(NewTabPageItem.VIEW_TYPE_PROMO, signinPromoGroup.getItems().get(0).getType());

        ntpManager.mSignInStateObserver.onSignedIn();
        assertEquals(0, signinPromoGroup.getItems().size());

        ntpManager.mSignInStateObserver.onSignedOut();
        assertEquals(1, signinPromoGroup.getItems().size());
    }

    @Test
    @Feature({"Ntp"})
    public void testSigninPromoDismissal() {
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);
        ChromePreferenceManager.getInstance(RuntimeEnvironment.application)
                .setNewTabPageSigninPromoDismissed(false);
        MockNewTabPageManager ntpManager = new MockNewTabPageManager(mSource);
        NewTabPageAdapter adapter = NewTabPageAdapter.create(ntpManager, null, null);
        final int signInPromoIndex = 5;

        assertEquals(5, adapter.getGroups().size());
        ItemGroup signinPromoGroup = adapter.getGroup(signInPromoIndex);

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2   | Status
        // 3   | Action
        // 4   | Progress Indicator
        // 5   | Sign in promo
        // 6   | Footer
        // 7   | Spacer

        assertEquals(NewTabPageItem.VIEW_TYPE_PROMO, signinPromoGroup.getItems().get(0).getType());

        adapter.dismissItem(signInPromoIndex);
        assertTrue(signinPromoGroup.getItems().isEmpty());
        assertTrue(ChromePreferenceManager.getInstance(RuntimeEnvironment.application)
                           .getNewTabPageSigninPromoDismissed());

        adapter = NewTabPageAdapter.create(ntpManager, null, null);
        assertEquals(5, adapter.getGroups().size());
        // The items below the signin promo move up, footer is now at the position of the promo.
        assertEquals(NewTabPageItem.VIEW_TYPE_FOOTER, adapter.getItemViewType(signInPromoIndex));
    }

    /** Registers the category with hasMoreButton=false and showIfEmpty=true*/
    private void registerCategory(FakeSuggestionsSource suggestionsSource,
            @CategoryInt int category, int suggestionCount) {
        // FakeSuggestionSource does not provide suggestions if the category's status is not
        // AVAILABLE.
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        // Important: showIfEmpty flag to true.
        suggestionsSource.setInfoForCategory(category, createInfo(category, false, true));
        suggestionsSource.setSuggestionsForCategory(
                category, createDummySuggestions(suggestionCount));
    }

    private int getCategory(ItemGroup itemGroup) {
        return ((SuggestionsSection) itemGroup).getCategory();
    }

    private static class MockNewTabPageManager implements NewTabPageManager {
        SuggestionsSource mSuggestionsSource;
        SignInStateObserver mSignInStateObserver;

        public MockNewTabPageManager(SuggestionsSource suggestionsSource) {
            mSuggestionsSource = suggestionsSource;
        }

        @Override
        public void openMostVisitedItem(MostVisitedItem item) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void onCreateContextMenu(ContextMenu menu, OnMenuItemClickListener listener) {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean onMenuItemClick(int menuId, MostVisitedItem item) {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isLocationBarShownInNTP() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isVoiceSearchEnabled() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isFakeOmniboxTextEnabledTablet() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isOpenInNewWindowEnabled() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean isOpenInIncognitoEnabled() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void navigateToBookmarks() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void navigateToRecentTabs() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void navigateToDownloadManager() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void trackSnippetsPageImpression(int[] categories, int[] suggestionsPerCategory) {
        }

        @Override
        public void trackSnippetImpression(SnippetArticle article) {
        }

        @Override
        public void trackSnippetMenuOpened(SnippetArticle article) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void trackSnippetCategoryActionImpression(int category, int position) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void trackSnippetCategoryActionClick(int category, int position) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void openSnippet(int windowOpenDisposition, SnippetArticle article) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void focusSearchBox(boolean beginVoiceSearch, String pastedText) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void setMostVisitedURLsObserver(MostVisitedURLsObserver observer, int numResults) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void getLocalFaviconImageForURL(String url, int size,
                FaviconImageCallback faviconCallback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void getLargeIconForUrl(String url, int size, LargeIconCallback callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void ensureIconIsAvailable(String pageUrl, String iconUrl, boolean isLargeIcon,
                boolean isTemporary, IconAvailabilityCallback callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void getUrlsAvailableOffline(Set<String> pageUrls, Callback<Set<String>> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void onLogoClicked(boolean isAnimatedLogoShowing) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void getSearchProviderLogo(LogoObserver logoObserver) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void onLoadingComplete(MostVisitedItem[] mostVisitedItems) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void addContextMenuCloseCallback(Callback<Menu> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void removeContextMenuCloseCallback(Callback<Menu> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void onLearnMoreClicked() {
            throw new UnsupportedOperationException();
        }

        @Override
        @Nullable public SuggestionsSource getSuggestionsSource() {
            return mSuggestionsSource;
        }

        @Override
        public void closeContextMenu() {
            throw new UnsupportedOperationException();
        }

        @Override
        public void registerSignInStateObserver(SignInStateObserver signInStateObserver) {
            mSignInStateObserver = signInStateObserver;
        }

        @Override
        public boolean isCurrentPage() {
            return true;
        }
    }
}
