// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.v7.widget.RecyclerView.Adapter;
import android.view.ViewGroup;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SnippetHeaderListItem;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge.SnippetsObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * A class that handles merging above the fold elements and below the fold cards into an adapter
 * that will be used to back the NTP RecyclerView. The first element in the adapter should always be
 * the above-the-fold view (containing the logo, search box, and most visited tiles) and subsequent
 * elements will be the cards shown to the user
 */
public class NewTabPageAdapter extends Adapter<NewTabPageViewHolder> implements SnippetsObserver {
    private static final String TAG = "Ntp";

    private final NewTabPageManager mNewTabPageManager;
    private final NewTabPageLayout mNewTabPageLayout;
    private final AboveTheFoldListItem mAboveTheFoldListItem;
    private final List<NewTabPageListItem> mNewTabPageListItems;

    /**
     * Constructor to create the manager for all the cards to display on the NTP
     *
     * @param manager the NewTabPageManager to use to interact with the snippets service and the
     *                rest of the system.
     * @param newTabPageLayout the layout encapsulating all the above-the-fold elements
     *                         (logo, search box, most visited tiles)
     */
    public NewTabPageAdapter(NewTabPageManager manager, NewTabPageLayout newTabPageLayout) {
        mNewTabPageManager = manager;
        mNewTabPageLayout = newTabPageLayout;
        mAboveTheFoldListItem = new AboveTheFoldListItem();
        mNewTabPageListItems = new ArrayList<NewTabPageListItem>();
        mNewTabPageListItems.add(mAboveTheFoldListItem);

        mNewTabPageManager.setSnippetsObserver(this);
    }

    @Override
    public void onSnippetsReceived(List<SnippetArticle> listSnippets) {
        int newSnippetCount = listSnippets.size();
        Log.d(TAG, "Received %d new snippets.", newSnippetCount);
        mNewTabPageListItems.clear();
        mNewTabPageListItems.add(mAboveTheFoldListItem);

        if (newSnippetCount > 0) {
            mNewTabPageListItems.add(new SnippetHeaderListItem());
            mNewTabPageListItems.addAll(listSnippets);
        }

        notifyDataSetChanged();
    }

    @Override
    @NewTabPageListItem.ViewType
    public int getItemViewType(int position) {
        return mNewTabPageListItems.get(position).getType();
    }

    @Override
    public NewTabPageViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        if (viewType == NewTabPageListItem.VIEW_TYPE_ABOVE_THE_FOLD) {
            return new NewTabPageViewHolder(mNewTabPageLayout);
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_HEADER) {
            return new NewTabPageViewHolder(SnippetHeaderListItem.createView(parent));
        }

        if (viewType == NewTabPageListItem.VIEW_TYPE_SNIPPET) {
            return new SnippetArticleViewHolder(
                    SnippetArticleViewHolder.createView(parent), mNewTabPageManager);
        }

        return null;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, final int position) {
        holder.onBindViewHolder(mNewTabPageListItems.get(position));
    }

    @Override
    public int getItemCount() {
        return mNewTabPageListItems.size();
    }
}
