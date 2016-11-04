// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createDummySuggestions;
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
    /**
     * Number of items in a section when there are no suggestions: header, status, action, progress.
     */
    private static final int EMPTY_SECTION_COUNT = 4;

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

        section = ContentSuggestionsTestUtils.createSection(true, true, mParent, mManager, mBridge);
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

        SuggestionsSection section = createSection(false, true, mParent, mManager, mBridge);
        // Note: when status is not initialised, we insert an item for the status card, but it's
        // null!
        assertEquals(EMPTY_SECTION_COUNT, section.getItemCount());

        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);
        verify(mParent).onItemRangeChanged(section, 1, EMPTY_SECTION_COUNT - 1);
        verify(mParent).onItemRangeInserted(
                section, EMPTY_SECTION_COUNT, suggestionCount - EMPTY_SECTION_COUNT + 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testSetStatusNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, mParent, mManager, mBridge);

        section.setStatus(CategoryStatus.AVAILABLE);
        verify(mParent).onItemRangeChanged(section, 1, EMPTY_SECTION_COUNT - 1);

        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);

        // We don't clear suggestions when the status is AVAILABLE.
        section.setStatus(CategoryStatus.AVAILABLE);
        verify(mParent, times(2)).onItemRangeChanged(section, 1, EMPTY_SECTION_COUNT - 1);
        verify(mParent).onItemRangeInserted(
                section, EMPTY_SECTION_COUNT, suggestionCount - EMPTY_SECTION_COUNT + 1);

        // We clear existing suggestions when the status is not AVAILABLE.
        section.setStatus(CategoryStatus.SIGNED_OUT);
        verify(mParent, times(3)).onItemRangeChanged(section, 1, EMPTY_SECTION_COUNT - 1);
        verify(mParent).onItemRangeRemoved(
                section, EMPTY_SECTION_COUNT, suggestionCount - EMPTY_SECTION_COUNT + 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotification() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, mParent, mManager, mBridge);

        section.removeSuggestion(snippets.get(0));
        verify(mParent, never())
                .onItemRangeChanged(any(SuggestionsSection.class), anyInt(), anyInt());
        verify(mParent, never())
                .onItemRangeInserted(any(SuggestionsSection.class), anyInt(), anyInt());
        verify(mParent, never())
                .onItemRangeRemoved(any(SuggestionsSection.class), anyInt(), anyInt());

        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(1));
        verify(mParent).onItemRangeRemoved(section, 2, 1);

        section.removeSuggestion(snippets.get(0));
        verify(mParent).onItemRangeRemoved(section, 1, 1);
        verify(mParent).onItemRangeInserted(section, 1, 3);
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotificationWithButton() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(true, true, mParent, mManager, mBridge);

        section.removeSuggestion(snippets.get(0));
        verify(mParent, never())
                .onItemRangeChanged(any(SuggestionsSection.class), anyInt(), anyInt());
        verify(mParent, never())
                .onItemRangeInserted(any(SuggestionsSection.class), anyInt(), anyInt());
        verify(mParent, never())
                .onItemRangeRemoved(any(SuggestionsSection.class), anyInt(), anyInt());

        section.addSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(0));
        verify(mParent).onItemRangeRemoved(section, 1, 1);

        section.removeSuggestion(snippets.get(1));
        verify(mParent, times(2)).onItemRangeRemoved(section, 1, 1);
        verify(mParent).onItemRangeInserted(section, 1, 3);
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

        SuggestionsSection section = createSection(true, true, mParent, mManager, mBridge);
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

    private OfflinePageDownloadItem createOfflineItem(String url, String guid) {
        return new OfflinePageDownloadItem(guid, url, "", "", 0, 0);
    }
}
