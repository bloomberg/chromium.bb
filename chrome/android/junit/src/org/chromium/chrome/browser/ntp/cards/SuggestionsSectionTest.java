// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createDummySuggestions;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.CategoryInfoBuilder;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link SuggestionsSection}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionsSectionTest {

    @Mock private NodeParent mParent;
    @Mock private NewTabPageManager mManager;
    private FakeOfflinePageBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBridge = new FakeOfflinePageBridge();
    }

    @Test
    @Feature({"Ntp"})
    public void testDismissSibling() {
        List<SnippetArticle> snippets = createDummySuggestions(3);
        SuggestionsSection section = createSectionWithReloadAction(true);

        section.setStatus(CategoryStatus.AVAILABLE);
        assertNotNull(section.getActionItem());

        // Without snippets.
        assertEquals(ItemViewType.ACTION, section.getItemViewType(2));
        assertEquals(-1, section.getDismissSiblingPosDelta(2));
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
        assertEquals(1, section.getDismissSiblingPosDelta(1));

        // With snippets.
        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(1));
        assertEquals(0, section.getDismissSiblingPosDelta(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testAddSuggestionsNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSectionWithReloadAction(false);
        // Simulate initialisation by the adapter. Here we don't care about the notifications, since
        // the RecyclerView will be updated through notifyDataSetChanged.
        section.setStatus(CategoryStatus.AVAILABLE);
        reset(mParent);

        assertEquals(2, section.getItemCount()); // When empty, we have the header and status card.

        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);
        verify(mParent).onItemRangeInserted(section, 1, suggestionCount);
        verify(mParent).onItemRangeRemoved(section, 1 + suggestionCount, 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testSetStatusNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);
        SuggestionsSection section = createSectionWithReloadAction(false);

        // Simulate initialisation by the adapter. Here we don't care about the notifications, since
        // the RecyclerView will be updated through notifyDataSetChanged.
        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);
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
        verifyNoMoreInteractions(mParent);
    }

    @Test(expected = IndexOutOfBoundsException.class)
    @Feature({"Ntp"})
    public void testRemoveUnknownSuggestion() {
        SuggestionsSection section = createSectionWithReloadAction(false);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.removeSuggestion(createDummySuggestions(1).get(0));
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotification() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSectionWithReloadAction(false);
        section.setStatus(CategoryStatus.AVAILABLE);
        reset(mParent);

        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(1));
        verify(mParent).onItemRangeRemoved(section, 2, 1);

        section.removeSuggestion(snippets.get(0));
        verify(mParent).onItemRangeRemoved(section, 1, 1);
        verify(mParent).onItemRangeInserted(section, 1, 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotificationWithButton() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsCategoryInfo info =
                new CategoryInfoBuilder(42)
                .withMoreAction()
                .withReloadAction()
                .showIfEmpty()
                .build();
        SuggestionsSection section = createSection(info);
        section.setStatus(CategoryStatus.AVAILABLE);
        reset(mParent);
        assertEquals(3, section.getItemCount()); // We have the header and status card and a button.

        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(0));
        verify(mParent).onItemRangeRemoved(section, 1, 1);

        section.removeSuggestion(snippets.get(1));
        verify(mParent, times(2)).onItemRangeRemoved(section, 1, 1);
        verify(mParent).onItemRangeInserted(section, 1, 1); // Only the status card is added.
    }

    @Test
    @Feature({"Ntp"})
    public void testOfflineStatus() {
        final int suggestionCount = 3;
        final List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);
        assertNull(snippets.get(0).getOfflinePageOfflineId());
        assertNull(snippets.get(1).getOfflinePageOfflineId());
        assertNull(snippets.get(2).getOfflinePageOfflineId());

        final OfflinePageItem item0 = createOfflinePageItem(snippets.get(0).mUrl, 0L);
        final OfflinePageItem item1 = createOfflinePageItem(snippets.get(1).mUrl, 1L);

        mBridge.setIsOfflinePageModelLoaded(true);
        mBridge.setItems(Arrays.asList(item0, item1));

        SuggestionsSection section = createSectionWithReloadAction(true);
        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);

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
    public void testViewAllActionPriority() {
        // When all the actions are enabled, ViewAll always has the priority and is shown.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(42)
                        .withMoreAction()
                        .withReloadAction()
                        .withViewAllAction()
                        .showIfEmpty()
                        .build());
        SuggestionsSection section = createSection(info);

        assertTrue(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_VIEW_ALL);

        section.addSuggestions(createDummySuggestions(3), CategoryStatus.AVAILABLE);

        assertTrue(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_VIEW_ALL);
    }

    @Test
    @Feature({"Ntp"})
    public void testReloadAndFetchMoreActionPriority() {
        // When both Reload and FetchMore are enabled, FetchMore runs when we have suggestions, and
        // Reload when we don't.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(42)
                        .withMoreAction()
                        .withReloadAction()
                        .showIfEmpty()
                        .build());
        SuggestionsSection section = createSection(info);

        assertTrue(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_RELOAD);

        section.addSuggestions(createDummySuggestions(3), CategoryStatus.AVAILABLE);

        assertTrue(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_FETCH_MORE);
    }

    @Test
    @Feature({"Ntp"})
    public void testReloadActionPriority() {
        // When only Reload is enabled, it only shows when we have no suggestions.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(42).withReloadAction().showIfEmpty().build());
        SuggestionsSection section = createSection(info);

        assertTrue(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_RELOAD);

        section.addSuggestions(createDummySuggestions(3), CategoryStatus.AVAILABLE);

        assertFalse(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_NONE);
    }

    @Test
    @Feature({"Ntp"})
    public void testFetchMoreActionPriority() {
        // When only FetchMore is enabled, it only shows when we have suggestions.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(42).withMoreAction().showIfEmpty().build());
        SuggestionsSection section = createSection(info);

        assertFalse(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_NONE);

        section.addSuggestions(createDummySuggestions(3), CategoryStatus.AVAILABLE);

        assertTrue(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_FETCH_MORE);
    }

    @Test
    @Feature({"Ntp"})
    public void testNoAction() {
        // Test where no action is enabled.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info = spy(new CategoryInfoBuilder(42).showIfEmpty().build());
        SuggestionsSection section = createSection(info);

        assertFalse(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_NONE);

        section.addSuggestions(createDummySuggestions(3), CategoryStatus.AVAILABLE);

        assertFalse(section.getActionItem().isVisible());
        verifyAction(section, ActionItem.ACTION_NONE);
    }

    @Test
    @Feature({"Ntp"})
    public void testFetchMoreProgressDisplay() {
        final int suggestionCount = 3;
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(42).withMoreAction().showIfEmpty().build());
        SuggestionsSection section = createSection(info);
        section.addSuggestions(createDummySuggestions(suggestionCount), CategoryStatus.AVAILABLE);
        assertFalse(section.getProgressItemForTesting().isVisible());

        // Tap the button
        verifyAction(section, ActionItem.ACTION_FETCH_MORE);
        assertTrue(section.getProgressItemForTesting().isVisible());

        // Simulate receiving suggestions.
        section.addSuggestions(createDummySuggestions(suggestionCount), CategoryStatus.AVAILABLE);
        assertFalse(section.getProgressItemForTesting().isVisible());
    }

    private SuggestionsSection createSectionWithReloadAction(boolean hasReloadAction) {
        CategoryInfoBuilder builder = new CategoryInfoBuilder(42).showIfEmpty();
        if (hasReloadAction) builder.withReloadAction();
        return createSection(builder.build());
    }

    private SuggestionsSection createSection(SuggestionsCategoryInfo info) {
        return new SuggestionsSection(mParent, info, mManager, mBridge);
    }

    private OfflinePageItem createOfflinePageItem(String url, long offlineId) {
        return new OfflinePageItem(url, offlineId, "", "", "", 0, 0, 0, 0);
    }

    private static void verifyAction(SuggestionsSection section, @ActionItem.Action int action) {
        SuggestionsSource suggestionsSource = mock(SuggestionsSource.class);
        NewTabPageManager manager = mock(NewTabPageManager.class);
        when(manager.getSuggestionsSource()).thenReturn(suggestionsSource);

        if (action != ActionItem.ACTION_NONE) {
            section.getActionItem().performAction(manager);
        }

        verify(section.getCategoryInfo(),
                (action == ActionItem.ACTION_VIEW_ALL ? times(1) : never()))
                .performViewAllAction(manager);
        verify(suggestionsSource,
                action == ActionItem.ACTION_RELOAD || action == ActionItem.ACTION_FETCH_MORE
                        ? times(1)
                        : never())
                .fetchSuggestions(anyInt(), any(String[].class));
    }
}
