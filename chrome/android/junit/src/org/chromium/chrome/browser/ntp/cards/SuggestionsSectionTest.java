// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anySetOf;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
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
    @Mock private OfflinePageBridge mBridge;
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
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(1));
        assertEquals(0, section.getDismissSiblingPosDelta(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testSetSuggestionsNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, mParent, mManager, mBridge);
        // Note: when status is not initialised, we insert an item for the status card, but it's
        // null!
        assertEquals(EMPTY_SECTION_COUNT, section.getItemCount());

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);
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

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

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

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

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

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(0));
        verify(mParent).onItemRangeRemoved(section, 1, 1);

        section.removeSuggestion(snippets.get(1));
        verify(mParent, times(2)).onItemRangeRemoved(section, 1, 1);
        verify(mParent).onItemRangeInserted(section, 1, 3);
    }

    @Test
    @Feature({"Ntp"})
    public void testOfflineStatus() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        assertFalse(snippets.get(0).isAvailableOffline());
        assertFalse(snippets.get(0).isAmpAvailableOffline());
        assertFalse(snippets.get(1).isAvailableOffline());
        assertFalse(snippets.get(1).isAmpAvailableOffline());

        SuggestionsSection section = createSection(true, true, mParent, mManager, mBridge);
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);
        verify(mBridge).checkPagesExistOffline(anySetOf(String.class), mCallbacks.capture());

        // The callback is asynchronous.
        mCallbacks.getValue().onResult(new HashSet<>(Arrays.asList(
                snippets.get(0).mUrl,
                snippets.get(1).mAmpUrl)));

        assertTrue(snippets.get(0).isAvailableOffline());
        assertFalse(snippets.get(0).isAmpAvailableOffline());
        assertFalse(snippets.get(1).isAvailableOffline());
        assertTrue(snippets.get(1).isAmpAvailableOffline());
    }

    @Test
    @Feature({"Ntp"})
    public void testOfflineStatusUpdate() {
        ArgumentCaptor<OfflinePageModelObserver> observer =
                ArgumentCaptor.forClass(OfflinePageModelObserver.class);

        final int suggestionCount = 1;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        assertFalse(snippets.get(0).isAvailableOffline());
        assertFalse(snippets.get(0).isAmpAvailableOffline());

        SuggestionsSection section = createSection(true, true, mParent, mManager, mBridge);
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

        // The first callback, triggered when we called setSuggestions.
        verify(mBridge).checkPagesExistOffline(anySetOf(String.class), mCallbacks.capture());
        mCallbacks.getAllValues().get(0).onResult(Collections.<String>emptySet());

        verify(mBridge).addObserver(observer.capture());
        observer.getValue().offlinePageModelChanged();

        // The second callback, triggered by the offlinePageModelChanged notification.
        verify(mBridge, times(2))
                .checkPagesExistOffline(anySetOf(String.class), mCallbacks.capture());

        mCallbacks.getAllValues().get(1).onResult(new HashSet<>(Arrays.asList(
                snippets.get(0).mUrl,
                snippets.get(0).mAmpUrl)));

        assertTrue(snippets.get(0).isAvailableOffline());
        assertTrue(snippets.get(0).isAmpAvailableOffline());
    }
}
