// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.DisabledReason;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge.SnippetsObserver;
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
        NewTabPageAdapter ntpa = new NewTabPageAdapter(mNewTabPageManager, null, mSnippetsBridge);

        assertEquals(4, ntpa.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, ntpa.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, ntpa.getItemViewType(3));

        List<SnippetArticle> snippets = createDummySnippets();
        mSnippetsObserver.onSnippetsReceived(snippets);

        List<NewTabPageListItem> loadedItems = new ArrayList<>(ntpa.getItemsForTesting());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(
                NewTabPageListItem.VIEW_TYPE_SPACING, ntpa.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsObserver.onSnippetsReceived(Arrays.asList(new SnippetArticle[] {
                new SnippetArticle("foo", "title1", "pub1", "txt1", "foo", "bar", null, 0, 0, 0)}));
        assertEquals(loadedItems, ntpa.getItemsForTesting());
    }

    /**
     * Tests that the adapter keeps listening for snippet updates if it didn't get anything from
     * a previous fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoadingInitiallyEmpty() {
        NewTabPageAdapter ntpa = new NewTabPageAdapter(mNewTabPageManager, null, mSnippetsBridge);

        // If we don't get anything, we should be in the same situation as the initial one.
        mSnippetsObserver.onSnippetsReceived(new ArrayList<SnippetArticle>());
        assertEquals(4, ntpa.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(NewTabPageListItem.VIEW_TYPE_STATUS, ntpa.getItemViewType(2));
        assertEquals(NewTabPageListItem.VIEW_TYPE_SPACING, ntpa.getItemViewType(3));

        // We should load new snippets when we get notified about them.
        List<SnippetArticle> snippets = createDummySnippets();
        mSnippetsObserver.onSnippetsReceived(snippets);
        List<NewTabPageListItem> loadedItems = new ArrayList<>(ntpa.getItemsForTesting());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size() - 1));
        assertEquals(
                NewTabPageListItem.VIEW_TYPE_SPACING, ntpa.getItemViewType(loadedItems.size() - 1));

        // The adapter should ignore any new incoming data.
        mSnippetsObserver.onSnippetsReceived(Arrays.asList(new SnippetArticle[] {
                new SnippetArticle("foo", "title1", "pub1", "txt1", "foo", "bar", null, 0, 0, 0)}));
        assertEquals(loadedItems, ntpa.getItemsForTesting());
    }

    /**
     * Tests that the adapter clears the snippets when asked to.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetClearing() {
        NewTabPageAdapter ntpa = new NewTabPageAdapter(mNewTabPageManager, null, mSnippetsBridge);

        List<SnippetArticle> snippets = createDummySnippets();
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(3 + snippets.size(), ntpa.getItemCount());

        // When we clear the snippets, we should go back to the situation with the status card.
        mSnippetsObserver.onDisabledReasonChanged(DisabledReason.NONE);
        assertEquals(4, ntpa.getItemCount());

        // The adapter should now be waiting for new snippets.
        mSnippetsObserver.onSnippetsReceived(snippets);
        assertEquals(3 + snippets.size(), ntpa.getItemCount());
    }

    private List<SnippetArticle> createDummySnippets() {
        return Arrays.asList(new SnippetArticle[] {
                new SnippetArticle("https://site.com/url1", "title1", "pub1", "txt1",
                        "https://site.com/url1", "https://amp.site.com/url1", null, 0, 0, 0),
                new SnippetArticle("https://site.com/url2", "title2", "pub2", "txt2",
                        "https://site.com/url2", "https://amp.site.com/url1", null, 0, 0, 0),
                new SnippetArticle("https://site.com/url3", "title3", "pub3", "txt3",
                        "https://site.com/url3", "https://amp.site.com/url1", null, 0, 0, 0)});
    }
}
