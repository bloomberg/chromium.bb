// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
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

    @Before
    public void setUp() {
        mNewTabPageManager = mock(NewTabPageManager.class);
        mSnippetsObserver = null;

        // Intercept the observers so that we can mock invocations.
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                mSnippetsObserver = invocation.getArgumentAt(0, SnippetsObserver.class);
                return null;
            }}).when(mNewTabPageManager).setSnippetsObserver(any(SnippetsObserver.class));
    }

    /**
     * Tests the content of the adapter under standard conditions: on start and after a snippet
     * fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoading() {
        NewTabPageAdapter ntpa = new NewTabPageAdapter(mNewTabPageManager, null);
        assertEquals(1, ntpa.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));

        List<SnippetArticle> snippets = Arrays.asList(new SnippetArticle[] {
                new SnippetArticle("title1", "pub1", "txt1", "https://site.com/url1", null, 0, 0),
                new SnippetArticle("title2", "pub2", "txt2", "https://site.com/url2", null, 0, 0),
                new SnippetArticle("title3", "pub3", "txt3", "https://site.com/url3", null, 0, 0)});
        mSnippetsObserver.onSnippetsReceived(snippets);

        List<NewTabPageListItem> loadedItems = ntpa.getItemsForTesting();
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size()));
        assertNull(mSnippetsObserver);
    }

    /**
     * Tests that the adapter keeps listening for snippet updates if it didn't get anything from
     * a previous fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSnippetLoadingInitiallyEmpty() {
        NewTabPageAdapter ntpa = new NewTabPageAdapter(mNewTabPageManager, null);

        // If we don't get anything, we should still have the above-the-fold item present.
        mSnippetsObserver.onSnippetsReceived(new ArrayList<SnippetArticle>());
        assertEquals(1, ntpa.getItemCount());
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertNotNull(mSnippetsObserver);

        // We should load new snippets when we get notified about them.
        List<SnippetArticle> snippets = Arrays.asList(new SnippetArticle[] {
                new SnippetArticle("title1", "pub1", "txt1", "https://site.com/url1", null, 0, 0),
                new SnippetArticle("title2", "pub2", "txt2", "https://site.com/url2", null, 0, 0)});
        mSnippetsObserver.onSnippetsReceived(snippets);
        List<NewTabPageListItem> loadedItems = ntpa.getItemsForTesting();
        assertEquals(NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD, ntpa.getItemViewType(0));
        assertEquals(NewTabPageListItem.VIEW_TYPE_HEADER, ntpa.getItemViewType(1));
        assertEquals(snippets, loadedItems.subList(2, loadedItems.size()));
        assertNull(mSnippetsObserver);
    }
}
