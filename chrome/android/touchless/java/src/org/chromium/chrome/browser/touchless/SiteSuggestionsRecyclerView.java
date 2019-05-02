// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;

/**
 * Creates a custom recycler view that would initialize focus search on the middle child.
 */
public class SiteSuggestionsRecyclerView extends RecyclerView {
    private SiteSuggestionsLayoutManager mLayoutManager;

    public SiteSuggestionsRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void setLayoutManager(LayoutManager layout) {
        super.setLayoutManager(layout);

        if (layout instanceof SiteSuggestionsLayoutManager) {
            mLayoutManager = (SiteSuggestionsLayoutManager) layout;
        }
    }

    @Override
    protected boolean onRequestFocusInDescendants(int direction, Rect previouslyFocusedRect) {
        // If we don't have layoutmanager or if we're searching based on a previous focus,
        // default to parent focus search.
        if (mLayoutManager == null || previouslyFocusedRect != null) {
            return super.onRequestFocusInDescendants(direction, previouslyFocusedRect);
        }
        return mLayoutManager.focusCenterItem(direction, null);
    }
}
