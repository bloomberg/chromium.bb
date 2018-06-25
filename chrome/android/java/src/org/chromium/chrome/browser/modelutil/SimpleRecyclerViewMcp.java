// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.modelutil;

import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;

/**
 * A model change processor (MCP), i.e. an implementation of {@link RecyclerViewAdapter.Delegate},
 * which represents a homogeneous {@link SimpleList} of items that are independent of each other.
 * It is intended primarily (but not exclusively) for use in a {@link RecyclerView}.
 * @param <T> The type of items in the list.
 * @param <VH> The view holder type that shows items.
 */
public class SimpleRecyclerViewMcp<T, VH>
        extends ForwardingListObservable<Void> implements RecyclerViewAdapter.Delegate<VH, Void> {
    /**
     * A view binder used to bind items in the {@link ListObservable} model to view holders.
     *
     * @param <T> The item type in the {@link SimpleList} model.
     * @param <VH> The view holder type that shows items.
     */
    public interface ViewBinder<T, VH> {
        /**
         * Called to display the specified {@code item} in the provided {@code holder}.
         * @param holder The view holder which should be updated to represent the {@code item}.
         * @param item The item in the list.
         */
        void onBindViewHolder(VH holder, T item);
    }

    /**
     * A functional interface to return the view type for an item.
     * @param <T> The item type.
     */
    public interface ItemViewTypeCallback<T> {
        /**
         * @param item The item for which to return the view type.
         * @return The view type for the given {@code item}.
         * @see RecyclerView.Adapter#getItemViewType
         */
        int getItemViewType(T item);
    }

    private final SimpleList<T> mModel;
    private final ItemViewTypeCallback<T> mItemViewTypeCallback;
    private final ViewBinder<T, VH> mViewBinder;

    /**
     * @param model The {@link SimpleList} model used to retrieve items to display.
     * @param itemViewTypeCallback The callback to return the view type for an item, or null to use
     *         the default view type.
     * @param viewBinder The {@link ViewBinder} binding this adapter to the view holder.
     */
    public SimpleRecyclerViewMcp(SimpleListObservable<T> model,
            @Nullable ItemViewTypeCallback<T> itemViewTypeCallback, ViewBinder<T, VH> viewBinder) {
        mModel = model;
        mItemViewTypeCallback = itemViewTypeCallback;
        mViewBinder = viewBinder;
        model.addObserver(this);
    }

    @Override
    public int getItemCount() {
        return mModel.size();
    }

    @Override
    public int getItemViewType(int position) {
        if (mItemViewTypeCallback == null) return 0;

        return mItemViewTypeCallback.getItemViewType(mModel.get(position));
    }

    @Override
    public void onBindViewHolder(VH holder, int position, @Nullable Void payload) {
        assert payload == null;
        mViewBinder.onBindViewHolder(holder, mModel.get(position));
    }
}
