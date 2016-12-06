// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Displays and manages the UI for browsing history.
 */
public class HistoryManager implements OnMenuItemClickListener {
    private final Activity mActivity;
    private final SelectableListLayout mSelectableListLayout;
    private final HistoryAdapter mHistoryAdapter;
    private final SelectionDelegate<HistoryItem> mSelectionDelegate;

    /**
     * Creates a new HistoryManager.
     * @param activity The Activity associated with the HistoryManager.
     */
    public HistoryManager(Activity activity) {
        mActivity = activity;

        mSelectionDelegate = new SelectionDelegate<>();
        mHistoryAdapter = new HistoryAdapter(mSelectionDelegate);

        mSelectableListLayout =
                (SelectableListLayout) LayoutInflater.from(activity).inflate(
                        R.layout.history_main, null);
        mSelectableListLayout.initializeRecyclerView(mHistoryAdapter);
        mSelectableListLayout.initializeToolbar(R.layout.history_toolbar, mSelectionDelegate,
                R.string.menu_history, null, R.id.normal_menu_group,
                R.id.selection_mode_menu_group, this);
        mSelectableListLayout.initializeEmptyView(
                TintedDrawable.constructTintedDrawable(mActivity.getResources(),
                        R.drawable.history_large),
                R.string.history_manager_empty);

        mHistoryAdapter.initialize();

        // TODO(twellington): add a scroll listener to the RecyclerView that loads more items when
        //                    the scroll position is near the end.
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        if (item.getItemId() == R.id.close_menu_id && !DeviceFormFactor.isTablet(mActivity)) {
            mActivity.finish();
            return true;
        }
        return false;
    }

    /**
     * @return The view that shows the main browsing history UI.
     */
    public ViewGroup getView() {
        return mSelectableListLayout;
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void onDestroyed() {
        mSelectableListLayout.onDestroyed();
        mHistoryAdapter.onDestroyed();
    }
}