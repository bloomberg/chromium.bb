// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createDummySuggestions;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createInfo;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createSection;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.List;

/**
 * Unit tests for {@link SuggestionsSection}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionsSectionTest {
    /** Number of items in a section when there are no suggestions: header, status, progress. */
    private static final int EMPTY_SECTION_COUNT = 3;

    @Test
    @Feature({"Ntp"})
    public void testDismissSibling() {
        ItemGroup.Observer observerMock = mock(ItemGroup.Observer.class);
        List<SnippetArticle> snippets = createDummySuggestions(3);
        SuggestionsSection section;

        // Part 1: ShowMoreButton = true.
        section = new SuggestionsSection(42, createInfo(true, true), observerMock, null);
        section.setStatus(CategoryStatus.AVAILABLE);
        assertNotNull(section.getActionItem());

        // 1.1: Without snippets.
        assertEquals(-1, section.getDismissSiblingPosDelta(section.getActionItem()));
        assertEquals(1, section.getDismissSiblingPosDelta(section.getStatusItem()));

        // 1.2: With snippets.
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);
        assertEquals(0, section.getDismissSiblingPosDelta(section.getActionItem()));
        assertEquals(0, section.getDismissSiblingPosDelta(section.getStatusItem()));
        assertEquals(0, section.getDismissSiblingPosDelta(snippets.get(0)));

        // Part 2: ShowMoreButton = false.
        section = new SuggestionsSection(42, createInfo(false, true), observerMock, null);
        section.setStatus(CategoryStatus.AVAILABLE);
        assertNull(section.getActionItem());

        // 2.1: Without snippets.
        assertEquals(0, section.getDismissSiblingPosDelta(section.getStatusItem()));

        // 2.2: With snippets.
        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);
        assertEquals(0, section.getDismissSiblingPosDelta(section.getStatusItem()));
        assertEquals(0, section.getDismissSiblingPosDelta(snippets.get(0)));
    }

    @Test
    @Feature({"Ntp"})
    public void testSetSuggestionsNotification() {
        ItemGroup.Observer observerMock = mock(ItemGroup.Observer.class);

        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, observerMock);
        // Note: when status is not initialised, we insert an item for the status card, but it's
        // null!
        assertEquals(EMPTY_SECTION_COUNT, section.getItems().size());

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);
        verify(observerMock)
                .notifyGroupChanged(
                        eq(section), eq(EMPTY_SECTION_COUNT), eq(suggestionCount + 1 /* header */));
    }

    @Test
    @Feature({"Ntp"})
    public void testSetStatusNotification() {
        ItemGroup.Observer observerMock = mock(ItemGroup.Observer.class);
        final int emptySectionCount = 3;
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, observerMock);

        section.setStatus(CategoryStatus.AVAILABLE);
        verify(observerMock)
                .notifyGroupChanged(eq(section), eq(EMPTY_SECTION_COUNT), eq(EMPTY_SECTION_COUNT));

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

        // We don't clear suggestions when the status is AVAILABLE.
        section.setStatus(CategoryStatus.AVAILABLE);
        verify(observerMock)
                .notifyGroupChanged(eq(section), eq(suggestionCount + 1), eq(suggestionCount + 1));

        // We clear existing suggestions when the status is not AVAILABLE.
        section.setStatus(CategoryStatus.LOADING_ERROR);
        verify(observerMock)
                .notifyGroupChanged(eq(section), eq(suggestionCount + 1), eq(EMPTY_SECTION_COUNT));
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotification() {
        ItemGroup.Observer observerMock = mock(ItemGroup.Observer.class);

        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(false, true, observerMock);

        section.removeSuggestion(snippets.get(0));
        verify(observerMock, never()).notifyGroupChanged(any(ItemGroup.class), anyInt(), anyInt());
        verify(observerMock, never()).notifyItemRemoved(any(ItemGroup.class), anyInt());

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(1));
        verify(observerMock).notifyItemRemoved(section, 2);

        section.removeSuggestion(snippets.get(0));
        verify(observerMock).notifyItemRemoved(section, 1);
        verify(observerMock).notifyItemInserted(section, 1);
        verify(observerMock).notifyItemInserted(section, 2);
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotificationWithButton() {
        ItemGroup.Observer observerMock = mock(ItemGroup.Observer.class);

        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount);

        SuggestionsSection section = createSection(true, true, observerMock);

        section.removeSuggestion(snippets.get(0));
        verify(observerMock, never()).notifyGroupChanged(any(ItemGroup.class), anyInt(), anyInt());
        verify(observerMock, never()).notifyItemRemoved(any(ItemGroup.class), anyInt());

        section.setSuggestions(snippets, CategoryStatus.AVAILABLE);

        section.removeSuggestion(snippets.get(0));
        verify(observerMock).notifyItemRemoved(section, 1);

        section.removeSuggestion(snippets.get(1));
        verify(observerMock, times(2)).notifyItemRemoved(section, 1);
        verify(observerMock).notifyItemInserted(section, 1);
        verify(observerMock).notifyItemInserted(section, 3);
    }
}
