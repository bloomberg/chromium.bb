// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.modelutil.ListModel;

/**
 * This stateless class provides methods to bind a {@link ListModel<AccessorySheetDataPiece>}
 * to the {@link RecyclerView} used as view of a tab for the accessory sheet component.
 */
class AccessorySheetTabViewBinder {
    /**
     * Holds any View that represents a list entry.
     */
    static class ElementViewHolder<T, V extends View> extends RecyclerView.ViewHolder {
        ElementViewHolder(ViewGroup parent, int layout) {
            super(LayoutInflater.from(parent.getContext()).inflate(layout, parent, false));
        }

        @SuppressWarnings("unchecked")
        void bind(AccessorySheetDataPiece accessorySheetDataWrapper) {
            bind((T) accessorySheetDataWrapper.getDataPiece(), (V) itemView);
        }

        void bind(T t, V view) {}
    }

    static void initializeView(
            RecyclerView view, @Nullable RecyclerView.OnScrollListener scrollListener) {
        view.setLayoutManager(
                new LinearLayoutManager(view.getContext(), LinearLayoutManager.VERTICAL, false));
        view.setItemAnimator(null);
        if (scrollListener != null) view.addOnScrollListener(scrollListener);
    }
}