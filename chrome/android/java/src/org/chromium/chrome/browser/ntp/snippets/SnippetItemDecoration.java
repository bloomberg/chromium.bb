// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.v4.content.res.ResourcesCompat;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;

/**
 * A decorator that places separators between RecyclerView items of type
 * {@value NewTabPageListItem#VIEW_TYPE_SNIPPET}.
 */
public class SnippetItemDecoration extends RecyclerView.ItemDecoration {
    private final Drawable mSeparator;

    public SnippetItemDecoration(Context context) {
        mSeparator = ResourcesCompat
                .getDrawable(context.getResources(), R.drawable.snippet_separator, null);
    }

    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent,
            RecyclerView.State state) {
        if (!shouldPlaceSeparator(view, parent)) return;

        // Add space below the item to show the separator.
        outRect.set(0, 0, 0, mSeparator.getIntrinsicHeight());
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        final int left = parent.getPaddingLeft();
        final int right = parent.getWidth() - parent.getPaddingRight();

        // Draw the separators underneath the relevant items.
        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            if (!shouldPlaceSeparator(child, parent)) continue;

            RecyclerView.LayoutParams params = (RecyclerView.LayoutParams) child.getLayoutParams();
            final int top = child.getBottom() + params.bottomMargin;
            final int bottom = top + mSeparator.getIntrinsicHeight();

            mSeparator.setBounds(left, top, right, bottom);
            mSeparator.draw(c);
        }
    }

    /**
     * Checks whether there should be a separator below the given view in the RecyclerView.
     * Current logic checks if the current view and next view show snippets.
     */
    private boolean shouldPlaceSeparator(View view, RecyclerView parent) {
        int childPos = parent.getChildAdapterPosition(view);
        Adapter<?> adapter = parent.getAdapter();

        // childPos can be NO_POSITION if the method is called with a view being removed from the
        // RecyclerView, for example when dismissing an item.
        if (childPos == adapter.getItemCount() - 1 || childPos == RecyclerView.NO_POSITION) {
            return false;
        }

        return adapter.getItemViewType(childPos) == NewTabPageListItem.VIEW_TYPE_SNIPPET
                && adapter.getItemViewType(childPos + 1) == NewTabPageListItem.VIEW_TYPE_SNIPPET;
    }
}