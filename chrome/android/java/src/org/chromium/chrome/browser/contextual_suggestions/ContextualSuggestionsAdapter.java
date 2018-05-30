// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.ntp.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.ItemViewType;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderViewHolder;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;

import java.util.List;

/**
 * An adapter that contains the view binder for the content component.
 */
class ContextualSuggestionsAdapter extends RecyclerViewAdapter<ClusterList, NewTabPageViewHolder> {
    private class ContextualSuggestionsViewBinder
            implements ViewBinder<ClusterList, NewTabPageViewHolder> {
        @Override
        public NewTabPageViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            switch (viewType) {
                case ItemViewType.HEADER:
                    return new SectionHeaderViewHolder(mRecyclerView, mUiConfig);

                case ItemViewType.SNIPPET:
                    return new ContextualSuggestionCardViewHolder(mRecyclerView,
                            mContextMenuManager, mUiDelegate, mUiConfig,
                            OfflinePageBridge.getForProfile(mProfile));

                default:
                    assert false;
                    return null;
            }
        }

        @Override
        public void onBindViewHolder(ClusterList model, NewTabPageViewHolder holder, int position) {
            model.onBindViewHolder(holder, position);
        }
    }

    private final Profile mProfile;
    private final UiConfig mUiConfig;
    private final SuggestionsUiDelegate mUiDelegate;
    private final ClusterList mClusterList;
    private final ContextMenuManager mContextMenuManager;

    private SuggestionsRecyclerView mRecyclerView;

    /**
     * Construct a new {@link ContextualSuggestionsAdapter}.
     * @param profile The regular {@link Profile}.
     * @param uiConfig The {@link UiConfig} used to adjust view display.
     * @param uiDelegate The {@link SuggestionsUiDelegate} used to help construct items in the
     *                   content view.
     * @param clusterList The {@link ClusterList} for the component.
     * @param contextMenuManager The {@link ContextMenuManager} used to display a context menu.
     */
    ContextualSuggestionsAdapter(Profile profile, UiConfig uiConfig,
            SuggestionsUiDelegate uiDelegate, ClusterList clusterList,
            ContextMenuManager contextMenuManager) {
        super(clusterList, null);

        setViewBinder(new ContextualSuggestionsViewBinder());

        mProfile = profile;
        mUiConfig = uiConfig;
        mUiDelegate = uiDelegate;
        mClusterList = clusterList;
        mContextMenuManager = contextMenuManager;
    }

    @Override
    @ItemViewType
    public int getItemViewType(int position) {
        return mClusterList.getItemViewType(position);
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = (SuggestionsRecyclerView) recyclerView;
    }

    @Override
    public void onDetachedFromRecyclerView(RecyclerView recyclerView) {
        mRecyclerView = null;
    }

    @Override
    public void onViewRecycled(NewTabPageViewHolder holder) {
        holder.recycle();
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, int position, List<Object> payloads) {
        if (payloads.isEmpty()) {
            onBindViewHolder(holder, position);
            return;
        }

        for (Object payload : payloads) {
            ((NewTabPageViewHolder.PartialBindCallback) payload).onResult(holder);
        }
    }
}
