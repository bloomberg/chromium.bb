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
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createInfo;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createSection;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link SuggestionsSection}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionsSectionTest {

    @Mock private NodeParent mParent;
    @Mock private OfflinePageDownloadBridge mBridge;
    @Mock private NewTabPageManager mManager;

    // This is a member so we can initialize it with the annotation and capture the generic type.
    @Captor ArgumentCaptor<Callback<Set<String>>> mCallbacks;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @Feature({"Ntp"})
    public void testDismissSibling() {
        List<SnippetArticle> snippets = createDummySuggestions(3);
        SuggestionsSection section;

        section = ContentSuggestionsTestUtils.createSection(true, mParent, mManager, mBridge);
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

        SuggestionsSection section = createSection(false, mParent, mManager, mBridge);
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
        SuggestionsSection section = createSection(false, mParent, mManager, mBridge);

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
        SuggestionsSection section = createSection(false, mParent, mManager, mBridge);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.removeSuggestion(createDummySuggestions(1).get(0));
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotification() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, mParent, mManager, mBridge);
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

        SuggestionsSection section = new SuggestionsSection(mParent,
                createInfo(42, /*hasMoreAction=*/true, /*hasReloadAction=*/true,
                        /*hasViewAllAction=*/false, /*showIfEmpty=*/true),
                mManager, mBridge);
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
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);
        assertNull(snippets.get(0).getOfflinePageDownloadGuid());
        assertNull(snippets.get(1).getOfflinePageDownloadGuid());
        assertNull(snippets.get(2).getOfflinePageDownloadGuid());

        OfflinePageDownloadItem item0 = createOfflineItem(snippets.get(0).mUrl, "guid0");
        OfflinePageDownloadItem item1 = createOfflineItem(snippets.get(1).mAmpUrl, "guid1");

        when(mBridge.getAllItems()).thenReturn(Arrays.asList(item0, item1));

        SuggestionsSection section = createSection(true, mParent, mManager, mBridge);
        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);

        // Check that we pick up the correct information.
        assertEquals(snippets.get(0).getOfflinePageDownloadGuid(), "guid0");
        assertEquals(snippets.get(1).getOfflinePageDownloadGuid(), "guid1");
        assertNull(snippets.get(2).getOfflinePageDownloadGuid());

        OfflinePageDownloadItem item2 = createOfflineItem(snippets.get(2).mUrl, "guid2");
        when(mBridge.getAllItems()).thenReturn(Arrays.asList(item1, item2));

        ArgumentCaptor<OfflinePageDownloadBridge.Observer> observer =
                ArgumentCaptor.forClass(OfflinePageDownloadBridge.Observer.class);
        verify(mBridge).addObserver(observer.capture());

        // Check that a change in OfflinePageDownloadBridge state forces an update.
        observer.getValue().onItemsLoaded();
        assertNull(snippets.get(0).getOfflinePageDownloadGuid());
        assertEquals(snippets.get(1).getOfflinePageDownloadGuid(), "guid1");
        assertEquals(snippets.get(2).getOfflinePageDownloadGuid(), "guid2");
    }

    @Test
    @Feature({"Ntp"})
    public void testViewAllActionPriority() {
        // When all the actions are enabled, ViewAll always has the priority and is shown.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(createInfo(42, /*hasMoreAction=*/true, /*hasReloadAction=*/true,
                        /*hasViewAllAction=*/true, /*showIfEmpty=*/true));
        SuggestionsSection section = new SuggestionsSection(mParent, info, mManager, mBridge);

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
        SuggestionsCategoryInfo info = spy(createInfo(42, /*hasMoreAction=*/true,
                /*hasReloadAction=*/true, /*hasViewAllAction=*/false, /*showIfEmpty=*/true));
        SuggestionsSection section = new SuggestionsSection(mParent, info, mManager, mBridge);

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
        SuggestionsCategoryInfo info = spy(createInfo(42, /*hasMoreAction=*/false,
                /*hasReloadAction=*/true, /*hasViewAllAction=*/false, /*showIfEmpty=*/true));
        SuggestionsSection section = new SuggestionsSection(mParent, info, mManager, mBridge);

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
        SuggestionsCategoryInfo info = spy(createInfo(42, /*hasMoreAction=*/true,
                /*hasReloadAction=*/false, /*hasViewAllAction=*/false, /*showIfEmpty=*/true));
        SuggestionsSection section = new SuggestionsSection(mParent, info, mManager, mBridge);

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
        SuggestionsCategoryInfo info = spy(createInfo(42, /*hasMoreAction=*/false,
                /*hasReloadAction=*/false, /*hasViewAllAction=*/false, /*showIfEmpty=*/true));
        SuggestionsSection section = new SuggestionsSection(mParent, info, mManager, mBridge);

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
        SuggestionsSection section = new SuggestionsSection(mParent,
                spy(createInfo(42, /*hasMoreAction=*/true, /*hasReloadAction=*/false,
                        /*hasViewAllAction=*/false, /*showIfEmpty=*/true)),
                mManager, mBridge);
        section.addSuggestions(createDummySuggestions(suggestionCount), CategoryStatus.AVAILABLE);
        assertFalse(section.getProgressItemForTesting().isVisible());

        // Tap the button
        verifyAction(section, ActionItem.ACTION_FETCH_MORE);
        assertTrue(section.getProgressItemForTesting().isVisible());

        // Simulate receiving suggestions.
        section.addSuggestions(createDummySuggestions(suggestionCount), CategoryStatus.AVAILABLE);
        assertFalse(section.getProgressItemForTesting().isVisible());
    }

    private OfflinePageDownloadItem createOfflineItem(String url, String guid) {
        return new OfflinePageDownloadItem(guid, url, "", "", 0, 0);
    }

    private static void verifyAction(SuggestionsSection section, @ActionItem.Action int action) {
        NewTabPageAdapter adapter = mock(NewTabPageAdapter.class);
        SuggestionsSource suggestionsSource = mock(SuggestionsSource.class);
        NewTabPageManager manager = mock(NewTabPageManager.class);
        when(manager.getSuggestionsSource()).thenReturn(suggestionsSource);

        try {
            section.getActionItem().performAction(manager, adapter);
        } catch (AssertionError e) {
            if (action != ActionItem.ACTION_NONE) throw e;
        }

        verify(section.getCategoryInfo(),
                (action == ActionItem.ACTION_VIEW_ALL ? times(1) : never()))
                .performViewAllAction(manager);
        verify(suggestionsSource, action == ActionItem.ACTION_FETCH_MORE ? times(1) : never())
                .fetchSuggestions(anyInt(), any(String[].class));
        verify(adapter, action == ActionItem.ACTION_RELOAD ? times(1) : never()).reloadSnippets();
    }
}
