// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.annotation.TargetApi;
import android.graphics.Rect;
import android.os.Build;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;

/**
 * A decorator that places separators between RecyclerView items of type
 * {@value NewTabPageListItem#VIEW_TYPE_SNIPPET} and elevation.
 */
public class SnippetItemDecoration extends RecyclerView.ItemDecoration {
    public SnippetItemDecoration() {}

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        Adapter<?> adapter = parent.getAdapter();
        int childPos = parent.getChildAdapterPosition(view);

        // childPos can be NO_POSITION if the method is called with a view being removed from the
        // RecyclerView, for example when dismissing an item.
        if (childPos == RecyclerView.NO_POSITION) {
            return;
        }

        // Only applies to view type snippet.
        if (adapter.getItemViewType(childPos) != NewTabPageListItem.VIEW_TYPE_SNIPPET) return;

        // Add elevation to each snippet if supported.
        ApiCompatibilityUtils.setElevation(
                view, parent.getContext().getResources().getDimensionPixelSize(
                              R.dimen.snippets_card_elevation));

        // Add space below the item. Checks if the current and next view are snippets.
        if (adapter.getItemViewType(childPos + 1) == NewTabPageListItem.VIEW_TYPE_SNIPPET) {
            outRect.set(0, 0, 0, parent.getContext().getResources().getDimensionPixelSize(
                                         R.dimen.snippets_vertical_spacing));
        }
    }
}