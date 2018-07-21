// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;

/**
 * A class that determines whether an empty view should be shown and inserts into the model.
 */
class EmptyViewCoordinator implements OfflineItemFilterObserver {
    private final Context mContext;
    private final DecoratedListItemModel mDecoratedModel;
    private final OfflineItemFilterSource mSource;
    private final Handler mHandler = new Handler();

    private ViewListItem mEmptyViewItem;

    private @Filters.FilterType int mCurrentFilter;

    /** Creates a {@link EmptyViewCoordinator} instance that wraos {@code model}. */
    public EmptyViewCoordinator(Context context, DecoratedListItemModel decoratedModel,
            OfflineItemFilterSource source) {
        mContext = context;
        mDecoratedModel = decoratedModel;
        mSource = source;
        mSource.addObserver(this);
        mHandler.post(() -> onFilterTypeSelected(mCurrentFilter));
    }

    public void onFilterTypeSelected(@Filters.FilterType int filter) {
        boolean hasEmptyView = mEmptyViewItem != null;
        if (filter == mCurrentFilter && hasEmptyView == isEmpty()) return;

        mCurrentFilter = filter;
        updateForEmptyView();
    }

    private boolean isEmpty() {
        boolean showingPrefetch = mCurrentFilter == Filters.FilterType.PREFETCHED;
        for (OfflineItem item : mSource.getItems()) {
            if (showingPrefetch && item.isSuggested) return false;
            if (!showingPrefetch && !item.isSuggested) return false;
        }

        return true;
    }

    private void updateForEmptyView() {
        mEmptyViewItem = isEmpty() ? createEmptyView() : null;
        mDecoratedModel.setEmptyView(mEmptyViewItem);
    }

    private ViewListItem createEmptyView() {
        boolean showingPrefetch = mCurrentFilter == Filters.FilterType.PREFETCHED;
        // TODO(shaktisahu): Supply correct value for prefetch settings.
        boolean prefetchEnabled = true;
        View emptyView = LayoutInflater.from(mContext).inflate(R.layout.downloads_empty_view, null);

        Drawable emptyDrawable = VectorDrawableCompat.create(mContext.getResources(),
                showingPrefetch ? R.drawable.ic_library_news_feed : R.drawable.downloads_big,
                mContext.getTheme());

        TextView emptyTextView = emptyView.findViewById(R.id.empty_view);
        emptyTextView.setText(showingPrefetch
                        ? (prefetchEnabled ? R.string.download_manager_prefetch_tab_empty
                                           : R.string.download_manager_enable_prefetch_message)
                        : R.string.download_manager_ui_empty);
        emptyTextView.setCompoundDrawablesWithIntrinsicBounds(null, emptyDrawable, null, null);

        return new ViewListItem(Long.MAX_VALUE - 1, emptyView);
    }

    // OfflineItemFilterObserver implementation.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        mHandler.post(() -> onFilterTypeSelected(mCurrentFilter));
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        mHandler.post(() -> onFilterTypeSelected(mCurrentFilter));
    }

    @Override
    public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {
        mHandler.post(() -> onFilterTypeSelected(mCurrentFilter));
    }
}
