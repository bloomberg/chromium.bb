// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;

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
        mHistoryAdapter = new HistoryAdapter(mSelectionDelegate, this);

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
        } else if (item.getItemId() == R.id.selection_mode_open_in_new_tab) {
            openItemsInNewTabs(mSelectionDelegate.getSelectedItems(), false);
            mSelectionDelegate.clearSelection();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_open_in_incognito) {
            openItemsInNewTabs(mSelectionDelegate.getSelectedItems(), true);
            mSelectionDelegate.clearSelection();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_delete_menu_id) {
            for (HistoryItem historyItem : mSelectionDelegate.getSelectedItems()) {
                mHistoryAdapter.markItemForRemoval(historyItem);
            }
            mHistoryAdapter.removeItems();
            mSelectionDelegate.clearSelection();
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

    /**
     * Removes the HistoryItem from the history backend and the HistoryAdapter.
     * @param item The HistoryItem to remove.
     */
    public void removeItem(HistoryItem item) {
        if (mSelectionDelegate.isItemSelected(item)) {
            mSelectionDelegate.toggleSelectionForItem(item);
        }
        mHistoryAdapter.markItemForRemoval(item);
        mHistoryAdapter.removeItems();
    }

    /**
     * Open the history item.
     * @param url The URL of the history item.
     * @param isIncognito Whether to open the history item in an incognito tab. If null, the tab
     *                    will open in the current tab model.
     * @param createNewTab Whether a new tab should be created. If false, the item will clobber the
     *                     the current tab.
     */
    public void openItem(String url, Boolean isIncognito, boolean createNewTab) {
        // Construct basic intent.
        Intent viewIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        viewIntent.putExtra(Browser.EXTRA_APPLICATION_ID,
                mActivity.getApplicationContext().getPackageName());

        // Determine component or class name.
        ComponentName component;
        if (DeviceFormFactor.isTablet(mActivity)) {
            component = mActivity.getComponentName();
        } else {
            component = IntentUtils.safeGetParcelableExtra(
                    mActivity.getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        }
        if (component != null) {
            viewIntent.setComponent(component);
        } else {
            viewIntent.setClass(mActivity, ChromeLauncherActivity.class);
        }

        // Set other intent extras.
        if (isIncognito != null) {
            viewIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, isIncognito);
        }
        if (createNewTab) viewIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);

        // Send intent.
        IntentHandler.startActivityForTrustedIntent(viewIntent, mActivity);
    }

    private void openItemsInNewTabs(List<HistoryItem> items, boolean isIncognito) {
        for (HistoryItem item : items) {
            openItem(item.getUrl(), isIncognito, true);
        }
    }
}