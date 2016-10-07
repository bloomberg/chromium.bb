// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createDummySuggestions;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createSection;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

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

    @Mock
    private ItemGroup.Observer mObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @Feature({"Ntp"})
    public void testDismissSibling() {
        List<SnippetArticle> snippets = createDummySuggestions(3);
        SuggestionsSection section;

        section = ContentSuggestionsTestUtils.createSection(true, true, mObserver);
        section.setStatus(CategoryStatus.AVAILABLE);
        assertNotNull(section.getActionItem());

        // Without snippets.
        assertEquals(-1, section.getDismissSiblingPosDelta(section.getActionItem()));
        assertEquals(1, section.getDismissSiblingPosDelta(section.getStatusItem()));

        // With snippets.
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);
        assertEquals(0, section.getDismissSiblingPosDelta(section.getActionItem()));
        assertEquals(0, section.getDismissSiblingPosDelta(section.getStatusItem()));
        assertEquals(0, section.getDismissSiblingPosDelta(snippets.get(0)));
    }

    @Test
    @Feature({"Ntp"})
    public void testSetSuggestionsNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, mObserver);
        // Note: when status is not initialised, we insert an item for the status card, but it's
        // null!
        assertEquals(EMPTY_SECTION_COUNT, section.getItems().size());

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);
        verify(mObserver).onItemRangeChanged(section, 1, EMPTY_SECTION_COUNT - 1);
        verify(mObserver).onItemRangeInserted(
                section, EMPTY_SECTION_COUNT, suggestionCount - EMPTY_SECTION_COUNT + 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testSetStatusNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, mObserver);

        section.setStatus(CategoryStatus.AVAILABLE);
        verify(mObserver).onItemRangeChanged(section, 1, EMPTY_SECTION_COUNT - 1);

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

        // We don't clear suggestions when the status is AVAILABLE.
        section.setStatus(CategoryStatus.AVAILABLE);
        verify(mObserver, times(2)).onItemRangeChanged(section, 1, EMPTY_SECTION_COUNT - 1);
        verify(mObserver).onItemRangeInserted(
                section, EMPTY_SECTION_COUNT, suggestionCount - EMPTY_SECTION_COUNT + 1);

        // We clear existing suggestions when the status is not AVAILABLE.
        section.setStatus(CategoryStatus.SIGNED_OUT);
        verify(mObserver, times(3)).onItemRangeChanged(section, 1, EMPTY_SECTION_COUNT - 1);
        verify(mObserver).onItemRangeRemoved(
                section, EMPTY_SECTION_COUNT, suggestionCount - EMPTY_SECTION_COUNT + 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotification() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, mObserver);

        section.removeSuggestion(snippets.get(0));
        verify(mObserver, never())
                .onItemRangeChanged(any(SuggestionsSection.class), anyInt(), anyInt());
        verify(mObserver, never())
                .onItemRangeInserted(any(SuggestionsSection.class), anyInt(), anyInt());
        verify(mObserver, never())
                .onItemRangeRemoved(any(SuggestionsSection.class), anyInt(), anyInt());

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(1));
        verify(mObserver).onItemRangeRemoved(eq(section), eq(2), eq(1));

        section.removeSuggestion(snippets.get(0));
        verify(mObserver).onItemRangeRemoved(eq(section), eq(1), eq(1));
        verify(mObserver).onItemRangeInserted(eq(section), eq(1), eq(1));
        verify(mObserver).onItemRangeInserted(eq(section), eq(2), eq(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotificationWithButton() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(true, true, mObserver);

        section.removeSuggestion(snippets.get(0));
        verify(mObserver, never())
                .onItemRangeChanged(any(SuggestionsSection.class), anyInt(), anyInt());
        verify(mObserver, never())
                .onItemRangeInserted(any(SuggestionsSection.class), anyInt(), anyInt());
        verify(mObserver, never())
                .onItemRangeRemoved(any(SuggestionsSection.class), anyInt(), anyInt());

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(0));
        verify(mObserver).onItemRangeRemoved(eq(section), eq(1), eq(1));

        section.removeSuggestion(snippets.get(1));
        verify(mObserver, times(2)).onItemRangeRemoved(eq(section), eq(1), eq(1));
        verify(mObserver).onItemRangeInserted(eq(section), eq(1), eq(1));
        verify(mObserver).onItemRangeInserted(eq(section), eq(3), eq(1));
    }
}
