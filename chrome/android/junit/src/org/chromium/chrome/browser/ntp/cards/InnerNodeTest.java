// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * JUnit tests for {@link InnerNode}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InnerNodeTest {

    private static final int[] ITEM_COUNTS = {1, 2, 3, 0, 3, 2, 1};
    private final List<TreeNode> mChildren = new ArrayList<>();
    @Mock private NodeParent mParent;
    private InnerNode mInnerNode;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        for (int i = 0; i < ITEM_COUNTS.length; i++) {
            TreeNode child = mock(TreeNode.class);
            when(child.getItemCount()).thenReturn(ITEM_COUNTS[i]);
            mChildren.add(child);
        }
        mInnerNode = new InnerNode(mParent) {
            @Override
            protected List<TreeNode> getChildren() {
                return mChildren;
            }
        };
    }

    @Test
    public void testInit() {
        mInnerNode.init();
        for (TreeNode child : mChildren) {
            verify(child).init();
        }
    }

    @Test
    public void testItemCount() {
        assertThat(mInnerNode.getItemCount(), is(12));
    }

    @Test
    public void testGetItemViewType() {
        when(mChildren.get(0).getItemViewType(0)).thenReturn(42);
        when(mChildren.get(2).getItemViewType(2)).thenReturn(23);
        when(mChildren.get(4).getItemViewType(0)).thenReturn(4711);
        when(mChildren.get(6).getItemViewType(0)).thenReturn(1729);

        assertThat(mInnerNode.getItemViewType(0), is(42));
        assertThat(mInnerNode.getItemViewType(5), is(23));
        assertThat(mInnerNode.getItemViewType(6), is(4711));
        assertThat(mInnerNode.getItemViewType(11), is(1729));
    }

    @Test
    public void testBindViewHolder() {
        NewTabPageViewHolder holder = mock(NewTabPageViewHolder.class);
        mInnerNode.onBindViewHolder(holder, 0);
        mInnerNode.onBindViewHolder(holder, 5);
        mInnerNode.onBindViewHolder(holder, 6);
        mInnerNode.onBindViewHolder(holder, 11);

        verify(mChildren.get(0)).onBindViewHolder(holder, 0);
        verify(mChildren.get(2)).onBindViewHolder(holder, 2);
        verify(mChildren.get(4)).onBindViewHolder(holder, 0);
        verify(mChildren.get(6)).onBindViewHolder(holder, 0);
    }

    @Test
    public void testGetSuggestion() {
        SnippetArticle article1 = mock(SnippetArticle.class);
        SnippetArticle article2 = mock(SnippetArticle.class);
        SnippetArticle article3 = mock(SnippetArticle.class);
        SnippetArticle article4 = mock(SnippetArticle.class);

        when(mChildren.get(0).getSuggestionAt(0)).thenReturn(article1);
        when(mChildren.get(2).getSuggestionAt(2)).thenReturn(article2);
        when(mChildren.get(4).getSuggestionAt(0)).thenReturn(article3);
        when(mChildren.get(6).getSuggestionAt(0)).thenReturn(article4);

        assertThat(mInnerNode.getSuggestionAt(0), is(article1));
        assertThat(mInnerNode.getSuggestionAt(5), is(article2));
        assertThat(mInnerNode.getSuggestionAt(6), is(article3));
        assertThat(mInnerNode.getSuggestionAt(11), is(article4));
    }

    @Test
    public void testDidAddChild() {
        TreeNode child = mock(TreeNode.class);
        when(child.getItemCount()).thenReturn(23);
        mChildren.add(3, child);
        mInnerNode.didAddChild(child);

        // The child should have been initialized and the parent notified about the added items.
        verify(child).init();
        verify(mParent).onItemRangeInserted(mInnerNode, 6, 23);

        TreeNode child2 = mock(TreeNode.class);
        when(child2.getItemCount()).thenReturn(0);
        mChildren.add(4, child2);
        mInnerNode.didAddChild(child2);
        verify(child2).init();

        // The empty child should have been initialized, but there should be no change
        // notifications.
        verifyNoMoreInteractions(mParent);
    }

    @Test
    public void testWillRemoveChild() {
        mInnerNode.willRemoveChild(mChildren.get(4));
        mChildren.remove(4);

        // The parent should have been notified about the removed items.
        verify(mParent).onItemRangeRemoved(mInnerNode, 6, 3);

        mInnerNode.willRemoveChild(mChildren.get(3));
        mChildren.remove(3);

        // There should be no change notifications about the empty child.
        verifyNoMoreInteractions(mParent);
    }

    @Test
    public void testNotifications() {
        mInnerNode.onItemRangeInserted(mChildren.get(0), 0, 23);
        mInnerNode.onItemRangeChanged(mChildren.get(2), 2, 9000);
        mInnerNode.onItemRangeChanged(mChildren.get(4), 0, 6502);
        mInnerNode.onItemRangeRemoved(mChildren.get(6), 0, 8086);

        verify(mParent).onItemRangeInserted(mInnerNode, 0, 23);
        verify(mParent).onItemRangeChanged(mInnerNode, 5, 9000);
        verify(mParent).onItemRangeChanged(mInnerNode, 6, 6502);
        verify(mParent).onItemRangeRemoved(mInnerNode, 11, 8086);
    }
}

