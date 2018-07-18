// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.toolbar;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.ui.StorageSummary;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;

import java.util.List;

/**
 * Handles toolbar functionality for the download home.
 */
public class DownloadHomeToolbar extends SelectableListToolbar<ListItem> {
    private StorageSummary mStorageSummary;
    private View mTitleBar;

    public DownloadHomeToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateMenu(R.menu.download_manager_menu);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleBar = findViewById(R.id.title_bar);

        TextView storageSummaryView = (TextView) findViewById(R.id.download_storage_summary);
        mStorageSummary = new StorageSummary(storageSummaryView);
    }

    /**
     * Removes the close button from the toolbar.
     */
    public void removeCloseButton() {
        getMenu().removeItem(R.id.close_menu_id);
    }

    @Override
    public void onSelectionStateChange(List<ListItem> selectedItems) {
        boolean wasSelectionEnabled = mIsSelectionEnabled;
        super.onSelectionStateChange(selectedItems);

        mTitleBar.setVisibility((mIsSelectionEnabled || mIsSearching) ? GONE : VISIBLE);
        if (mIsSelectionEnabled) {
            int numSelected = mSelectionDelegate.getSelectedItems().size();

            // If the share or delete menu items are shown in the overflow menu instead of as an
            // action, there may not be views associated with them.
            View shareButton = findViewById(R.id.selection_mode_share_menu_id);
            if (shareButton != null) {
                shareButton.setContentDescription(getResources().getQuantityString(
                        R.plurals.accessibility_share_selected_items, numSelected, numSelected));
            }

            View deleteButton = findViewById(R.id.selection_mode_delete_menu_id);
            if (deleteButton != null) {
                deleteButton.setContentDescription(getResources().getQuantityString(
                        R.plurals.accessibility_remove_selected_items, numSelected, numSelected));
            }

            if (!wasSelectionEnabled) {
                RecordUserAction.record("Android.DownloadManager.SelectionEstablished");
            }
        }
    }

    @Override
    public void showSearchView() {
        super.showSearchView();
        mTitleBar.setVisibility(GONE);
    }

    @Override
    public void hideSearchView() {
        super.hideSearchView();
        mTitleBar.setVisibility(VISIBLE);
    }
}