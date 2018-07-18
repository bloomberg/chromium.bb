// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.app.Activity;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.snackbars.DeleteUndoCoordinator;
import org.chromium.chrome.browser.download.home.toolbar.DownloadHomeToolbar;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;

import java.io.Closeable;

/**
 * The top level coordinator for the download home UI.  This is currently an in progress class and
 * is not fully fleshed out yet.
 */
class DownloadManagerCoordinatorImpl
        implements DownloadManagerCoordinator, Toolbar.OnMenuItemClickListener {
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final DateOrderedListCoordinator mListCoordinator;
    private final DeleteUndoCoordinator mDeleteCoordinator;

    private SelectableListLayout<ListItem> mSelectableListLayout;
    private ViewGroup mMainView;
    private DownloadHomeToolbar mToolbar;
    private Activity mActivity;

    private boolean mMuteFilterChanges;
    private boolean mIsSeparateActivity;
    private int mSearchMenuId;
    private TextView mEmptyView;

    private SelectionDelegate<ListItem> mSelectionDelegate;

    /** Builds a {@link DownloadManagerCoordinatorImpl} instance. */
    @SuppressWarnings({"unchecked"}) // mSelectableListLayout
    public DownloadManagerCoordinatorImpl(Profile profile, Activity activity, boolean offTheRecord,
            boolean isSeparateActivity, SnackbarManager snackbarManager) {
        mActivity = activity;
        mDeleteCoordinator = new DeleteUndoCoordinator(snackbarManager);
        mSelectionDelegate = new SelectionDelegate<ListItem>();
        mListCoordinator = new DateOrderedListCoordinator(mActivity, offTheRecord,
                OfflineContentAggregatorFactory.forProfile(profile),
                mDeleteCoordinator::showSnackbar, mSelectionDelegate, this ::notifyFilterChanged);

        mMainView =
                (ViewGroup) LayoutInflater.from(mActivity).inflate(R.layout.download_main, null);
        mSelectableListLayout =
                (SelectableListLayout<ListItem>) mMainView.findViewById(R.id.selectable_list);
        mEmptyView = mSelectableListLayout.initializeEmptyView(
                VectorDrawableCompat.create(
                        mActivity.getResources(), R.drawable.downloads_big, mActivity.getTheme()),
                R.string.download_manager_ui_empty, R.string.download_manager_no_results);

        RecyclerView recyclerView = (RecyclerView) mListCoordinator.getView();
        mSelectableListLayout.initializeRecyclerView(recyclerView.getAdapter(), recyclerView);

        boolean isLocationEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE);
        int normalGroupId =
                isLocationEnabled ? R.id.with_settings_normal_menu_group : R.id.normal_menu_group;
        mSearchMenuId = isLocationEnabled ? R.id.with_settings_search_menu_id : R.id.search_menu_id;

        mToolbar = (DownloadHomeToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.download_home_toolbar, mSelectionDelegate, 0, null, normalGroupId,
                R.id.selection_mode_menu_group, R.color.modern_primary_color, this, true,
                isSeparateActivity);
        mToolbar.getMenu().setGroupVisible(normalGroupId, true);
        mToolbar.initializeSearchView(
                /* searchDelegate = */ null, R.string.download_manager_search, mSearchMenuId);

        mIsSeparateActivity = isSeparateActivity;
        if (!mIsSeparateActivity) mToolbar.removeCloseButton();

        RecordUserAction.record("Android.DownloadManager.Open");
    }

    // DownloadManagerCoordinator implementation.
    @Override
    public void destroy() {
        mDeleteCoordinator.destroy();
        mListCoordinator.destroy();
    }

    @Override
    public View getView() {
        return mMainView;
    }

    @Override
    public boolean onBackPressed() {
        // TODO(dtrainor): Clear selection if multi-select is supported.
        return false;
    }

    @Override
    public void updateForUrl(String url) {
        try (FilterChangeBlock block = new FilterChangeBlock()) {
            mListCoordinator.setSelectedFilter(Filters.fromUrl(url));
        }
    }

    @Override
    public void showPrefetchSection() {
        updateForUrl(Filters.toUrl(Filters.FilterType.PREFETCHED));
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    private void notifyFilterChanged(@FilterType int filter) {
        mSelectionDelegate.clearSelection();
        if (mMuteFilterChanges) return;

        String url = Filters.toUrl(filter);
        for (Observer observer : mObservers) observer.onUrlChanged(url);
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        // TODO(shaktisahu): Handle menu items.
        return false;
    }

    /**
     * Helper class to mute state changes when processing a state change request from an external
     * source.
     */
    private class FilterChangeBlock implements Closeable {
        private final boolean mOriginalMuteFilterChanges;

        public FilterChangeBlock() {
            mOriginalMuteFilterChanges = mMuteFilterChanges;
            mMuteFilterChanges = true;
        }

        @Override
        public void close() {
            mMuteFilterChanges = mOriginalMuteFilterChanges;
        }
    }
}
