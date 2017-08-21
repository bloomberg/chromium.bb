// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;

/**
 * An {@link RecyclerView.OnScrollListener} that is attached to the {@link SuggestionsRecyclerView}
 * to trigger a suggestions load when the SuggestionsRecyclerView is scrolled near the bottom.
 */
public class ScrollToLoadListener extends RecyclerView.OnScrollListener {
    // TODO(peconn): Fix bug where More button is visible on first pull.
    // TODO(peconn): Lock behind a feature.
    /** How many items from the end must be visible for us to trigger a reload. */
    private static final int RELOAD_DISTANCE = 5;

    private final NewTabPageAdapter mAdapter;
    private final LinearLayoutManager mLayoutManager;
    private final SuggestionsUiDelegate mUiDelegate;
    private final SectionList mSections;

    public ScrollToLoadListener(NewTabPageAdapter adapter, LinearLayoutManager layoutManager,
            SuggestionsUiDelegate uiDelegate, SectionList sections) {
        mAdapter = adapter;
        mLayoutManager = layoutManager;
        mUiDelegate = uiDelegate;
        mSections = sections;
    }

    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        if (dy == 0) return;  // We've been triggered by a layout change calculation.
        if (mAdapter.getItemCount() == 0) return;  // Adapter hasn't been populated yet.

        if (mLayoutManager.findLastVisibleItemPosition()
                > mAdapter.getItemCount() - RELOAD_DISTANCE) {
            // We need to post this since onScrolled may run during a measure & layout pass.
            ThreadUtils.postOnUiThread(mSections::fetchMore);
        }
    }
}
