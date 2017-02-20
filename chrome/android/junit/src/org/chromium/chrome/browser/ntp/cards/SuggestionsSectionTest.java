// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsUnitTestUtils.bindViewHolders;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.createDummySuggestions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.DisableHistogramsRule;
import org.chromium.chrome.browser.EnableFeatures;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.UpdateLayoutParamsCallback;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.suggestions.SuggestionsMetricsReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.CategoryInfoBuilder;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * Unit tests for {@link SuggestionsSection}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionsSectionTest {
    @Rule
    public EnableFeatures.Processor mEnableFeatureProcessor = new EnableFeatures.Processor();

    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    private static final int TEST_CATEGORY_ID = 42;
    @Mock
    private SuggestionsSection.Delegate mDelegate;
    @Mock
    private NodeParent mParent;
    @Mock
    private SuggestionsUiDelegate mUiDelegate;
    private FakeOfflinePageBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBridge = new FakeOfflinePageBridge();

        // Set empty variation params for the test.
        CardsVariationParameters.setTestVariationParams(new HashMap<String, String>());
    }

    @Test
    @Feature({"Ntp"})
    public void testDismissSibling() {
        List<SnippetArticle> snippets = createDummySuggestions(3, TEST_CATEGORY_ID);
        SuggestionsSection section = createSectionWithFetchAction(true);

        section.setStatus(CategoryStatus.AVAILABLE);
        assertNotNull(section.getActionItemForTesting());

        // Without snippets.
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(0));
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
        assertEquals(setOf(1, 2), section.getItemDismissalGroup(1));
        assertEquals(ItemViewType.ACTION, section.getItemViewType(2));
        assertEquals(setOf(1, 2), section.getItemDismissalGroup(2));

        // With snippets.
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(0));
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(1));
        assertEquals(Collections.singleton(1), section.getItemDismissalGroup(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetDismissalGroupWithoutHeader() {
        SuggestionsSection section = createSectionWithFetchAction(true);
        section.setHeaderVisibility(false);

        assertEquals(ItemViewType.STATUS, section.getItemViewType(0));
        assertEquals(setOf(0, 1), section.getItemDismissalGroup(0));

        assertEquals(ItemViewType.ACTION, section.getItemViewType(1));
        assertEquals(setOf(0, 1), section.getItemDismissalGroup(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetDismissalGroupWithoutAction() {
        SuggestionsSection section = createSectionWithFetchAction(false);

        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
        assertEquals(Collections.singleton(1), section.getItemDismissalGroup(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetDismissalGroupActionAndHeader() {
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setHeaderVisibility(false);

        assertEquals(ItemViewType.STATUS, section.getItemViewType(0));
        assertEquals(Collections.singleton(0), section.getItemDismissalGroup(0));
    }

    @Test
    @Feature({"Ntp"})
    public void testAddSuggestionsNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);

        SuggestionsSection section = createSectionWithFetchAction(false);
        // Simulate initialisation by the adapter. Here we don't care about the notifications, since
        // the RecyclerView will be updated through notifyDataSetChanged.
        section.setStatus(CategoryStatus.AVAILABLE);
        reset(mParent);

        assertEquals(2, section.getItemCount()); // When empty, we have the header and status card.
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        verify(mParent).onItemRangeInserted(section, 1, suggestionCount);
        verify(mParent).onItemRangeRemoved(section, 1 + suggestionCount, 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testSetStatusNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);
        SuggestionsSection section = createSectionWithFetchAction(false);

        // Simulate initialisation by the adapter. Here we don't care about the notifications, since
        // the RecyclerView will be updated through notifyDataSetChanged.
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        reset(mParent);

        // We don't clear suggestions when the status is AVAILABLE.
        section.setStatus(CategoryStatus.AVAILABLE);
        verifyNoMoreInteractions(mParent);

        // We clear existing suggestions when the status is not AVAILABLE, and show the status card.
        section.setStatus(CategoryStatus.SIGNED_OUT);
        verify(mParent).onItemRangeRemoved(section, 1, suggestionCount);
        verify(mParent).onItemRangeInserted(section, 1, 1);

        // A loading state item triggers showing the loading item.
        section.setStatus(CategoryStatus.AVAILABLE_LOADING);
        verify(mParent).onItemRangeInserted(section, 2, 1);

        section.setStatus(CategoryStatus.AVAILABLE);
        verify(mParent).onItemRangeRemoved(section, 2, 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveUnknownSuggestion() {
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.removeSuggestionById("foobar");
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotification() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);

        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setStatus(CategoryStatus.AVAILABLE);
        reset(mParent);

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);

        section.removeSuggestionById(snippets.get(1).mIdWithinCategory);
        verify(mParent).onItemRangeRemoved(section, 2, 1);

        section.removeSuggestionById(snippets.get(0).mIdWithinCategory);
        verify(mParent).onItemRangeRemoved(section, 1, 1);
        verify(mParent).onItemRangeInserted(section, 1, 1);

        assertEquals(2, section.getItemCount());
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotificationWithButton() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);

        SuggestionsCategoryInfo info =
                new CategoryInfoBuilder(TEST_CATEGORY_ID).withFetchAction().showIfEmpty().build();
        SuggestionsSection section = createSection(info);
        section.setStatus(CategoryStatus.AVAILABLE);
        reset(mParent);
        assertEquals(3, section.getItemCount()); // We have the header and status card and a button.

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        assertEquals(4, section.getItemCount());

        section.removeSuggestionById(snippets.get(0).mIdWithinCategory);
        verify(mParent).onItemRangeRemoved(section, 1, 1);

        section.removeSuggestionById(snippets.get(1).mIdWithinCategory);
        verify(mParent, times(2)).onItemRangeRemoved(section, 1, 1);
        verify(mParent).onItemRangeInserted(section, 1, 1); // Only the status card is added.
        assertEquals(3, section.getItemCount());
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
        assertEquals(ItemViewType.ACTION, section.getItemViewType(2));
    }

    @Test
    @Feature({"Ntp"})
    public void testDismissSection() {
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setStatus(CategoryStatus.AVAILABLE);
        reset(mParent);
        assertEquals(2, section.getItemCount());

        @SuppressWarnings("unchecked")
        Callback<String> callback = mock(Callback.class);
        section.dismissItem(1, callback);
        verify(mDelegate).dismissSection(section);
        verify(callback).onResult(section.getHeaderText());
    }

    @Test
    @Feature({"Ntp"})
    public void testOfflineStatus() {
        final int suggestionCount = 3;
        final List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);
        assertNull(snippets.get(0).getOfflinePageOfflineId());
        assertNull(snippets.get(1).getOfflinePageOfflineId());
        assertNull(snippets.get(2).getOfflinePageOfflineId());

        final OfflinePageItem item0 = createOfflinePageItem(snippets.get(0).mUrl, 0L);
        final OfflinePageItem item1 = createOfflinePageItem(snippets.get(1).mUrl, 1L);

        mBridge.setIsOfflinePageModelLoaded(true);
        mBridge.setItems(Arrays.asList(item0, item1));

        SuggestionsSection section = createSectionWithFetchAction(true);
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);

        // Check that we pick up the correct information.
        assertEquals(Long.valueOf(0L), snippets.get(0).getOfflinePageOfflineId());
        assertEquals(Long.valueOf(1L), snippets.get(1).getOfflinePageOfflineId());
        assertNull(snippets.get(2).getOfflinePageOfflineId());

        final OfflinePageItem item2 = createOfflinePageItem(snippets.get(2).mUrl, 2L);

        mBridge.setItems(Arrays.asList(item1, item2));

        // Check that a change in OfflinePageBridge state forces an update.
        mBridge.fireOfflinePageModelLoaded();
        assertNull(snippets.get(0).getOfflinePageOfflineId());
        assertEquals(Long.valueOf(1L), snippets.get(1).getOfflinePageOfflineId());
        assertEquals(Long.valueOf(2L), snippets.get(2).getOfflinePageOfflineId());
    }

    @Test
    @Feature({"Ntp"})
    public void testOfflineStatusIgnoredIfDetached() {
        final int suggestionCount = 2;
        final List<SnippetArticle> suggestions = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);
        assertNull(suggestions.get(0).getOfflinePageOfflineId());
        assertNull(suggestions.get(1).getOfflinePageOfflineId());

        final OfflinePageItem item0 = createOfflinePageItem(suggestions.get(0).mUrl, 0L);
        mBridge.setIsOfflinePageModelLoaded(true);
        mBridge.setItems(Arrays.asList(item0));

        SuggestionsSection section = createSectionWithSuggestions(suggestions);

        // The offline status should propagate before detaching.
        assertEquals(Long.valueOf(0L), suggestions.get(0).getOfflinePageOfflineId());
        assertNull(suggestions.get(1).getOfflinePageOfflineId());

        section.detach();

        final OfflinePageItem item1 = createOfflinePageItem(suggestions.get(1).mUrl, 1L);
        mBridge.setItems(Arrays.asList(item0, item1));
        // Check that a change in OfflinePageBridge state forces an update.
        mBridge.fireOfflinePageModelLoaded();

        // The offline status should not change any more.
        assertEquals(Long.valueOf(0L), suggestions.get(0).getOfflinePageOfflineId());
        assertNull(suggestions.get(1).getOfflinePageOfflineId());
    }

    @Test
    @Feature({"Ntp"})
    public void testViewAllActionPriority() {
        // When all the actions are enabled, ViewAll always has the priority and is shown.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info = spy(new CategoryInfoBuilder(TEST_CATEGORY_ID)
                                                   .withFetchAction()
                                                   .withViewAllAction()
                                                   .showIfEmpty()
                                                   .build());
        SuggestionsSection section = createSection(info);

        assertTrue(section.getActionItemForTesting().isVisible());
        verifyAction(section, ActionItem.ACTION_VIEW_ALL);
    }

    @Test
    @Feature({"Ntp"})
    public void testFetchActionPriority() {
        // When only FetchMore is shown when enabled.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info = spy(
                new CategoryInfoBuilder(TEST_CATEGORY_ID).withFetchAction().showIfEmpty().build());
        SuggestionsSection section = createSection(info);

        assertTrue(section.getActionItemForTesting().isVisible());
        verifyAction(section, ActionItem.ACTION_FETCH);
    }

    @Test
    @Feature({"Ntp"})
    public void testNoAction() {
        // Test where no action is enabled.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(TEST_CATEGORY_ID).showIfEmpty().build());
        SuggestionsSection section = createSection(info);

        assertFalse(section.getActionItemForTesting().isVisible());
        verifyAction(section, ActionItem.ACTION_NONE);
    }

    @Test
    @Feature({"Ntp"})
    public void testFetchMoreProgressDisplay() {
        final int suggestionCount = 3;
        SuggestionsCategoryInfo info = spy(
                new CategoryInfoBuilder(TEST_CATEGORY_ID).withFetchAction().showIfEmpty().build());
        SuggestionsSection section = createSection(info);
        section.setSuggestions(createDummySuggestions(suggestionCount, TEST_CATEGORY_ID),
                CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        assertFalse(section.getProgressItemForTesting().isVisible());

        // Tap the button
        verifyAction(section, ActionItem.ACTION_FETCH);
        assertTrue(section.getProgressItemForTesting().isVisible());

        // Simulate receiving suggestions.
        section.setSuggestions(createDummySuggestions(suggestionCount, TEST_CATEGORY_ID),
                CategoryStatus.AVAILABLE, /* replaceExisting = */ false);
        assertFalse(section.getProgressItemForTesting().isVisible());
    }

    /**
     * Tests that the UI updates on updated suggestions.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionUpdatesOnNewSuggestions() {
        SuggestionsSection section =
                createSectionWithSuggestions(createDummySuggestions(4, TEST_CATEGORY_ID));
        assertEquals(4, section.getSuggestionsCount());

        section.setSuggestions(createDummySuggestions(3, TEST_CATEGORY_ID),
                CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        verify(mParent).onItemRangeRemoved(section, 1, 4);
        verify(mParent).onItemRangeInserted(section, 1, 3);
        assertEquals(3, section.getSuggestionsCount());
    }

    /**
     * Tests that the UI does not update when updating is disabled by a parameter.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionDoesNotUpdateOnNewSuggestionsWhenDisabled() {
        // Override variation params for the test.
        HashMap<String, String> params = new HashMap<String, String>();
        params.put("ignore_updates_for_existing_suggestions", "true");
        CardsVariationParameters.setTestVariationParams(params);

        SuggestionsSection section =
                createSectionWithSuggestions(createDummySuggestions(4, TEST_CATEGORY_ID));
        assertEquals(4, section.getSuggestionsCount());

        section.setSuggestions(createDummySuggestions(3, TEST_CATEGORY_ID),
                CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        verify(mParent, never()).onItemRangeRemoved(any(TreeNode.class), anyInt(), anyInt());
        verify(mParent, never()).onItemRangeInserted(any(TreeNode.class), anyInt(), anyInt());
        assertEquals(4, section.getSuggestionsCount());
    }

    /**
     * Tests that the UI does not update the first item of the section if it has been viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionDoesNotUpdateFirstSuggestionOnNewSuggestionsWhenSeen() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        // Copy the list when passing to the section - it may alter it but we later need it.
        SuggestionsSection section =
                createSectionWithSuggestions(new ArrayList<>(snippets));
        assertEquals(4, section.getSuggestionsCount());

        // Bind the first suggestion - indicate that it is being viewed.
        // Indices in section are off-by-one (index 0 is the header).
        bindViewHolders(section, 1, 2);

        List<SnippetArticle> newSnippets =
                createDummySuggestions(3, TEST_CATEGORY_ID, "new");
        // Copy the list when passing to the section - it may alter it but we later need it.
        section.setSuggestions(new ArrayList<>(newSnippets), CategoryStatus.AVAILABLE,
                /* replaceExisting = */ true);
        verify(mParent).onItemRangeRemoved(section, 2, 3);
        verify(mParent).onItemRangeInserted(section, 2, 2);
        assertEquals(3, section.getSuggestionsCount());
        assertEquals(snippets.get(0), section.getSuggestionAt(1));
        assertNotEquals(snippets.get(1), section.getSuggestionAt(2));
        assertEquals(newSnippets.get(0), section.getSuggestionAt(2));
        assertNotEquals(snippets.get(2), section.getSuggestionAt(3));
        assertEquals(newSnippets.get(1), section.getSuggestionAt(3));
    }

    /**
     * Tests that the UI does not update the first two items of the section if they have been
     * viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionDoesNotUpdateFirstTwoSuggestionOnNewSuggestionsWhenSeen() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        // Copy the list when passing to the section - it may alter it but we later need it.
        SuggestionsSection section =
                createSectionWithSuggestions(new ArrayList<>(snippets));
        assertEquals(4, section.getSuggestionsCount());

        // Bind the first two suggestions - indicate that they are being viewed.
        // Indices in section are off-by-one (index 0 is the header).
        bindViewHolders(section, 1, 3);

        List<SnippetArticle> newSnippets =
                createDummySuggestions(3, TEST_CATEGORY_ID, "new");
        // Copy the list when passing to the section - it may alter it but we later need it.
        section.setSuggestions(new ArrayList<>(newSnippets), CategoryStatus.AVAILABLE,
                /* replaceExisting = */ true);
        verify(mParent).onItemRangeRemoved(section, 3, 2);
        verify(mParent).onItemRangeInserted(section, 3, 1);
        assertEquals(3, section.getSuggestionsCount());
        assertEquals(snippets.get(0), section.getSuggestionAt(1));
        assertEquals(snippets.get(1), section.getSuggestionAt(2));
        assertNotEquals(snippets.get(2), section.getSuggestionAt(3));
        assertEquals(newSnippets.get(0), section.getSuggestionAt(3));
    }

    /**
     * Tests that the UI does not update any items of the section if the new list is shorter than
     * what has been viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionDoesNotUpdateOnNewSuggestionsWhenNewListIsShorter() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        // Copy the list when passing to the section - it may alter it but we later need it.
        SuggestionsSection section =
                createSectionWithSuggestions(new ArrayList<>(snippets));
        assertEquals(4, section.getSuggestionsCount());

        // Bind the first two suggestions - indicate that they are being viewed.
        // Indices in section are off-by-one (index 0 is the header).
        bindViewHolders(section, 1, 3);

        section.setSuggestions(createDummySuggestions(1, TEST_CATEGORY_ID),
                CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        // Even though the new list has just one suggestion, we need to keep the two seen ones
        // around.
        verify(mParent).onItemRangeRemoved(section, 3, 2);
        verify(mParent, never()).onItemRangeInserted(any(TreeNode.class), anyInt(), anyInt());
        assertEquals(2, section.getSuggestionsCount());
        assertEquals(snippets.get(0), section.getSuggestionAt(1));
        assertEquals(snippets.get(1), section.getSuggestionAt(2));
    }

    /**
     * Tests that the UI does not update any items of the section if the current list is shorter
     * than what has been viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionDoesNotUpdateOnNewSuggestionsWhenCurrentListIsShorter() {
        List<SnippetArticle> snippets = createDummySuggestions(3, TEST_CATEGORY_ID, "old");
        // Copy the list when passing to the section - it may alter it but we later need it.
        SuggestionsSection section =
                createSectionWithSuggestions(new ArrayList<>(snippets));
        assertEquals(3, section.getSuggestionsCount());

        // Bind the first two suggestions - indicate that they are being viewed.
        // Indices in section are off-by-one (index 0 is the header).
        bindViewHolders(section, 1, 3);

        // Remove last two items.
        section.removeSuggestionById(section.getSuggestionAt(3).mIdWithinCategory);
        section.removeSuggestionById(section.getSuggestionAt(2).mIdWithinCategory);
        reset(mParent);

        assertEquals(1, section.getSuggestionsCount());

        section.setSuggestions(createDummySuggestions(4, TEST_CATEGORY_ID),
                CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        // We do not touch the current list if all has been seen.
        verify(mParent, never()).onItemRangeRemoved(any(TreeNode.class), anyInt(), anyInt());
        verify(mParent, never()).onItemRangeInserted(any(TreeNode.class), anyInt(), anyInt());
        assertEquals(1, section.getSuggestionsCount());
        assertEquals(snippets.get(0), section.getSuggestionAt(1));
    }

    /**
     * Tests that the UI does not update when the section has been viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionDoesNotUpdateOnNewSuggestionsWhenAllSeen() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        SuggestionsSection section = createSectionWithSuggestions(snippets);
        assertEquals(4, section.getSuggestionsCount());

        // Bind all the suggestions - indicate that they are being viewed.
        bindViewHolders(section);

        section.setSuggestions(createDummySuggestions(3, TEST_CATEGORY_ID),
                CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        verify(mParent, never()).onItemRangeRemoved(any(TreeNode.class), anyInt(), anyInt());
        verify(mParent, never()).onItemRangeInserted(any(TreeNode.class), anyInt(), anyInt());

        // All old snippets should be in place.
        verifySnippets(section, snippets);
    }

    /**
     * Tests that the UI does not update when anything has been appended.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionDoesNotUpdateOnNewSuggestionsWhenAppended() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        SuggestionsSection section = createSectionWithSuggestions(snippets);

        // Append another 3 suggestions.
        List<SnippetArticle> appendedSnippets =
                createDummySuggestions(3, TEST_CATEGORY_ID, "appended");
        section.setSuggestions(
                appendedSnippets, CategoryStatus.AVAILABLE, /* replaceExisting = */ false);

        // All 7 snippets should be in place.
        snippets.addAll(appendedSnippets);
        verifySnippets(section, snippets);

        // Try to replace them with another list. Should have no effect.
        List<SnippetArticle> newSnippets =
                createDummySuggestions(5, TEST_CATEGORY_ID, "new");
        section.setSuggestions(newSnippets, CategoryStatus.AVAILABLE,
                /* replaceExisting = */ true);

        // All previous snippets should be in place.
        verifySnippets(section, snippets);
    }

    @Test
    @Feature({"Ntp"})
    public void testCardIsNotifiedWhenBecomingFirst() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, /* categoryId = */ 42);
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setSuggestions(suggestions, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        reset(mParent);

        // Remove the first card. The second one should get the update.
        section.removeSuggestionById(suggestions.get(0).mIdWithinCategory);
        verify(mParent).onItemRangeChanged(
                same(section), eq(1), eq(1), any(UpdateLayoutParamsCallback.class));
    }

    @Test
    @Feature({"Ntp"})
    public void testCardIsNotifiedWhenBecomingLast() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, /* categoryId = */ 42);
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setSuggestions(suggestions, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        reset(mParent);

        // Remove the last card. The penultimate one should get the update.
        section.removeSuggestionById(suggestions.get(4).mIdWithinCategory);
        verify(mParent).onItemRangeChanged(
                same(section), eq(4), eq(1), any(UpdateLayoutParamsCallback.class));
    }

    @Test
    @Feature({"Ntp"})
    public void testCardIsNotifiedWhenBecomingSoleCard() {
        List<SnippetArticle> suggestions = createDummySuggestions(2, /* categoryId = */ 42);
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setSuggestions(suggestions, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        reset(mParent);

        // Remove the last card. The penultimate one should get the update.
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        verify(mParent).onItemRangeChanged(
                same(section), eq(1), eq(1), any(UpdateLayoutParamsCallback.class));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetItemDismissalGroupWithSuggestions() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, TEST_CATEGORY_ID);
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setSuggestions(suggestions, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);

        assertThat(section.getItemDismissalGroup(1).size(), is(1));
        assertThat(section.getItemDismissalGroup(1), contains(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetItemDismissalGroupWithActionItem() {
        SuggestionsSection section = createSectionWithFetchAction(true);
        assertThat(section.getItemDismissalGroup(1).size(), is(2));
        assertThat(section.getItemDismissalGroup(1), contains(1, 2));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetItemDismissalGroupWithoutActionItem() {
        SuggestionsSection section = createSectionWithFetchAction(false);
        assertThat(section.getItemDismissalGroup(1).size(), is(1));
        assertThat(section.getItemDismissalGroup(1), contains(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testCardIsNotifiedWhenNotTheLastAnymore() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, /* categoryId = */ 42);
        SuggestionsSection section = createSectionWithFetchAction(false);

        section.setSuggestions(suggestions, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);
        reset(mParent);

        section.setSuggestions(createDummySuggestions(2, /* categoryId = */ 42, "new"),
                CategoryStatus.AVAILABLE, /* replaceExisting = */ false);
        verify(mParent).onItemRangeChanged(
                same(section), eq(5), eq(1), any(UpdateLayoutParamsCallback.class));
    }

    private SuggestionsSection createSectionWithSuggestions(List<SnippetArticle> snippets) {
        SuggestionsSection section = createSectionWithFetchAction(true);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE, /* replaceExisting = */ true);

        // Reset any notification invocations on the parent from setting the initial list
        // of suggestions.
        reset(mParent);
        return section;
    }

    @SafeVarargs
    private static <T> Set<T> setOf(T... elements) {
        Set<T> set = new TreeSet<T>();
        set.addAll(Arrays.asList(elements));
        return set;
    }

    private SuggestionsSection createSectionWithFetchAction(boolean hasReloadAction) {
        CategoryInfoBuilder builder = new CategoryInfoBuilder(TEST_CATEGORY_ID).showIfEmpty();
        if (hasReloadAction) builder.withFetchAction();
        return createSection(builder.build());
    }

    private SuggestionsSection createSection(SuggestionsCategoryInfo info) {
        SuggestionsSection section = new SuggestionsSection(
                mDelegate, mUiDelegate, mock(SuggestionsRanker.class), mBridge, info);
        section.setParent(mParent);
        return section;
    }

    private OfflinePageItem createOfflinePageItem(String url, long offlineId) {
        return new OfflinePageItem(url, offlineId, "", "", "", 0, 0, 0, 0);
    }

    private static void verifyAction(SuggestionsSection section, @ActionItem.Action int action) {
        SuggestionsSource suggestionsSource = mock(SuggestionsSource.class);
        SuggestionsUiDelegate manager = mock(SuggestionsUiDelegate.class);
        SuggestionsNavigationDelegate navDelegate = mock(SuggestionsNavigationDelegate.class);
        when(manager.getSuggestionsSource()).thenReturn(suggestionsSource);
        when(manager.getNavigationDelegate()).thenReturn(navDelegate);
        when(manager.getMetricsReporter()).thenReturn(mock(SuggestionsMetricsReporter.class));

        if (action != ActionItem.ACTION_NONE) {
            section.getActionItemForTesting().performAction(manager);
        }

        verify(section.getCategoryInfo(),
                (action == ActionItem.ACTION_VIEW_ALL ? times(1) : never()))
                .performViewAllAction(navDelegate);
        verify(suggestionsSource, (action == ActionItem.ACTION_FETCH ? times(1) : never()))
                .fetchSuggestions(anyInt(), any(String[].class));
    }

    private static void verifySnippets(SuggestionsSection section, List<SnippetArticle> snippets) {
        assertEquals(snippets.size(), section.getSuggestionsCount());
        // Indices in section are off-by-one (index 0 is the header).
        int index = 1;
        for (SnippetArticle snippet : snippets) {
            assertEquals(snippet, section.getSuggestionAt(index++));
        }
    }
}
