// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.chrome.R;

/**
 * A class that decorates the RecyclerView elements.
 */
public class SnippetItemDecoration extends RecyclerView.ItemDecoration {
    private final int mVerticalSpace;

    public SnippetItemDecoration(Context context) {
        this.mVerticalSpace =
                context.getResources().getDimensionPixelSize(R.dimen.snippets_vertical_space);
    }

    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent,
            RecyclerView.State state) {
        outRect.setEmpty();
        if (parent.getChildAdapterPosition(view) != parent.getAdapter().getItemCount() - 1) {
            outRect.bottom = mVerticalSpace;
        }
    }
}