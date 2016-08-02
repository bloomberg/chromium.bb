// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SnippetsSource.SnippetsObserver;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;


/**
 * Unit tests for {@link NewTabPageAdapter}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageAdapterTest {

    private NewTabPageManager mNewTabPageManager;
    private SnippetsObserver mSnippetsObserver;
    private SnippetsBridge mSnippetsBridge;

    @Before
    public void setUp() {
        RecordHistogram.disableForTests();
        mNewTabPageManager = mock(NewTabPageManager.class);
        mSnippetsObserver = null;
        mSnippetsBridge = mock(SnippetsBridge.class);

        // Intercept the observers so that we can mock invocations.
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                mSnippetsObserver = invocation.getArgumentAt(0, SnippetsObserver.class);
                return null;
            }}).when(mSnippetsBridge).setObserver(any(SnippetsObserver.class));
    }

    /**
     * Tests the content of the adapter under standard conditions: on start and after a snippet
     * fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoading() {
        NewTabPageAdapter ntpa =
                new NewTabPageAdapter(mNewTabPageManager, null, mSnippetsBridge, null);

        assertEquals(5, ntpa.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, ntpa.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_PROGRESS, ntpa.getItemViewType(3));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, ntpa.getItemViewType(4));

        List<SnippetArticleListItem> snippets = createDummySnippets();
        mSnippetsObserver.onSnippetsReceived(snippets);

        List<NewTabPageListItem> loadedItems = new ArrayList<>(ntpa.getItemsForTesting());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(
                NewTabPageListItem.VIEW_TYPE_SPACING, ntpa.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsObserver.onSnippetsReceived(
                Arrays.asList(new SnippetArticleListItem[] {new SnippetArticleListItem(
                        "foo", "title1", "pub1", "txt1", "foo", "bar", 0, 0, 0)}));
        assertEquals(loadedItems, ntpa.getItemsForTesting());
    }

    /**
     * Tests that the adapter keeps listening for snippet updates if it didn't get anything from
     * a previous fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoadingInitiallyEmpty() {
        NewTabPageAdapter ntpa =
                new NewTabPageAdapter(mNewTabPageManager, null, mSnippetsBridge, null);

        // If we don't get anything, we should be in the same situation as the initial one.
        mSnippetsObserver.onSnippetsReceived(new ArrayList<SnippetArticleListItem>());
        assertEquals(5, ntpa.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, ntpa.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_PROGRESS, ntpa.getItemViewType(3));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, ntpa.getItemViewType(4));

        // We should load new snippets when we get notified about them.
        List<SnippetArticleListItem> snippets = createDummySnippets();
        mSnippetsObserver.onSnippetsReceived(snippets);
        List<NewTabPageListItem> loadedItems = new ArrayList<>(ntpa.getItemsForTesting());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(
                NewTabPageListItem.VIEW_TYPE_SPACING, ntpa.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsObserver.onSnippetsReceived(
                Arrays.asList(new SnippetArticleListItem[] {new SnippetArticleListItem(
                        "foo", "title1", "pub1", "txt1", "foo", "bar", 0, 0, 0)}));
        assertEquals(loadedItems, ntpa.getItemsForTesting());
    }

    /**
     * Tests that the adapter clears the snippets when asked to.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetClearing() {
        NewTabPageAdapter ntpa =
                new NewTabPageAdapter(mNewTabPageManager, null, mSnippetsBridge, null);

        List<SnippetArticleListItem> snippets = createDummySnippets();
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(3 + snippets.size(), ntpa.getItemCount());

        // If we get told that snippets are enabled, we just leave the current
        // ones there and not clear.
        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.AVAILABLE);
        assertEquals(3 + snippets.size(), ntpa.getItemCount());

        // When snippets are disabled, we clear them and we should go back to
        // the situation with the status card.
        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.SIGNED_OUT);
        assertEquals(5, ntpa.getItemCount());

        // The adapter should now be waiting for new snippets.
        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.AVAILABLE);
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(3 + snippets.size(), ntpa.getItemCount());
    }

    /**
     * Tests that the adapter loads snippets only when the status is favorable.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoadingBlock() {
        NewTabPageAdapter ntpa =
                new NewTabPageAdapter(mNewTabPageManager, null, mSnippetsBridge, null);

        List<SnippetArticleListItem> snippets = createDummySnippets();

        // By default, status is INITIALIZING, so we can load snippets
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(3 + snippets.size(), ntpa.getItemCount());

        // If we have snippets, we should not load the new list.
        snippets.add(new SnippetArticleListItem("https://site.com/url1", "title1", "pub1", "txt1",
                "https://site.com/url1", "https://amp.site.com/url1", 0, 0, 0));
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(3 + snippets.size() - 1, ntpa.getItemCount());

        // When snippets are disabled, we should not be able to load them
        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.SIGNED_OUT);
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(5, ntpa.getItemCount());

        // INITIALIZING lets us load snippets still.
        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.INITIALIZING);
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(3 + snippets.size(), ntpa.getItemCount());

        // The adapter should now be waiting for new snippets.
        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.AVAILABLE);
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(3 + snippets.size(), ntpa.getItemCount());
    }

    /**
     * Tests how the loading indicator reacts to status changes.
     */
    @Test
    @Feature({"Ntp"})
    public void testProgressIndicatorDisplay() {
        NewTabPageAdapter ntpa =
                new NewTabPageAdapter(mNewTabPageManager, null, mSnippetsBridge, null);
        int progressPos = ntpa.getBottomSpacerPosition() - 1;
        ProgressListItem progress = (ProgressListItem) ntpa.getItemsForTesting().get(progressPos);

        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.INITIALIZING);
        assertTrue(progress.isVisible());

        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.AVAILABLE);
        assertFalse(progress.isVisible());

        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.AVAILABLE_LOADING);
        assertTrue(progress.isVisible());

        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.NOT_PROVIDED);
        assertFalse(progress.isVisible());

        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertFalse(progress.isVisible());

        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.SIGNED_OUT);
        assertFalse(progress.isVisible());

        mSnippetsObserver.onCategoryStatusChanged(CategoryStatus.LOADING_ERROR);
        assertFalse(progress.isVisible());
    }

    private List<SnippetArticleListItem> createDummySnippets() {
        List<SnippetArticleListItem> snippets = new ArrayList<>();
        snippets.add(new SnippetArticleListItem("https://site.com/url1", "title1", "pub1", "txt1",
                "https://site.com/url1", "https://amp.site.com/url1", 0, 0, 0));
        snippets.add(new SnippetArticleListItem("https://site.com/url2", "title2", "pub2", "txt2",
                "https://site.com/url2", "https://amp.site.com/url2", 0, 0, 0));
        snippets.add(new SnippetArticleListItem("https://site.com/url3", "title3", "pub3", "txt3",
                "https://site.com/url3", "https://amp.site.com/url3", 0, 0, 0));
        return snippets;
    }
}
