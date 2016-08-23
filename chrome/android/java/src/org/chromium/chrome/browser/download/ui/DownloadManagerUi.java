// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.StrictMode;
import android.support.v4.widget.DrawerLayout;
import android.support.v4.widget.DrawerLayout.DrawerListener;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BasicNativePage;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper.OfflinePageItemWrapper;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Displays and manages the UI for the download manager.
 */

public class DownloadManagerUi implements OnMenuItemClickListener, BackendProvider {

    /**
     * Interface to observe the changes in the download manager ui. This should be implemented by
     * the ui components that is shown, in order to let them get proper notifications.
     */
    public interface DownloadUiObserver {
        /**
         * Called when the filter has been changed by the user.
         */
        public void onFilterChanged(int filter);

        /**
         * Called when the download manager is not shown anymore.
         */
        public void onManagerDestroyed();
    }

    private static final String TAG = "download_ui";
    private static final String DEFAULT_MIME_TYPE = "*/*";
    private static final String MIME_TYPE_DELIMITER = "/";

    private final DownloadHistoryAdapter mHistoryAdapter;
    private final FilterAdapter mFilterAdapter;
    private final ObserverList<DownloadUiObserver> mObservers = new ObserverList<>();

    private final Activity mActivity;
    private final boolean mIsOffTheRecord;
    private final ViewGroup mMainView;
    private final DownloadManagerToolbar mToolbar;
    private final SpaceDisplay mSpaceDisplay;
    private final ListView mFilterView;
    private final RecyclerView mRecyclerView;

    private BasicNativePage mNativePage;
    private OfflinePageDownloadBridge mOfflinePageBridge;
    private SelectionDelegate<DownloadHistoryItemWrapper> mSelectionDelegate;
    private final AtomicInteger mNumberOfFilesBeingDeleted = new AtomicInteger();

    public DownloadManagerUi(
            Activity activity, boolean isOffTheRecord, ComponentName parentComponent) {
        mActivity = activity;
        mIsOffTheRecord = isOffTheRecord;
        mOfflinePageBridge = new OfflinePageDownloadBridge(
                Profile.getLastUsedProfile().getOriginalProfile());

        mMainView = (ViewGroup) LayoutInflater.from(activity).inflate(R.layout.download_main, null);

        mSelectionDelegate = new SelectionDelegate<DownloadHistoryItemWrapper>();

        mHistoryAdapter = new DownloadHistoryAdapter(isOffTheRecord, parentComponent);
        mHistoryAdapter.initialize(this);
        addObserver(mHistoryAdapter);

        mSpaceDisplay = new SpaceDisplay(mMainView, mHistoryAdapter);
        mHistoryAdapter.registerAdapterDataObserver(mSpaceDisplay);
        mSpaceDisplay.onChanged();

        mFilterAdapter = new FilterAdapter();
        mFilterAdapter.initialize(this);
        addObserver(mFilterAdapter);

        mToolbar = (DownloadManagerToolbar) mMainView.findViewById(R.id.action_bar);
        mToolbar.setOnMenuItemClickListener(this);
        DrawerLayout drawerLayout = null;
        if (!DeviceFormFactor.isLargeTablet(activity)) {
            drawerLayout = (DrawerLayout) mMainView;
            addDrawerListener(drawerLayout);
        }
        mToolbar.initialize(mSelectionDelegate, 0, drawerLayout, R.id.normal_menu_group,
                R.id.selection_mode_menu_group);
        addObserver(mToolbar);

        mFilterView = (ListView) mMainView.findViewById(R.id.section_list);
        mFilterView.setAdapter(mFilterAdapter);
        mFilterView.setOnItemClickListener(mFilterAdapter);

        mRecyclerView = (RecyclerView) mMainView.findViewById(R.id.recycler_view);
        mRecyclerView.setAdapter(mHistoryAdapter);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(activity));

        FadingShadowView shadow = (FadingShadowView) mMainView.findViewById(R.id.shadow);
        if (DeviceFormFactor.isLargeTablet(mActivity)) {
            shadow.setVisibility(View.GONE);
        } else {
            shadow.init(ApiCompatibilityUtils.getColor(mMainView.getResources(),
                    R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);
        }

        mToolbar.setTitle(R.string.menu_downloads);

        // TODO(ianwen): add support for loading state.
    }

    /**
     * Sets the {@link BasicNativePage} that holds this manager.
     */
    public void setBasicNativePage(BasicNativePage delegate) {
        mNativePage = delegate;
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void onDestroyed() {
        for (DownloadUiObserver observer : mObservers) {
            observer.onManagerDestroyed();
            removeObserver(observer);
        }

        if (mOfflinePageBridge != null) {
            mOfflinePageBridge.destroy();
            mOfflinePageBridge = null;
        }

        mHistoryAdapter.unregisterAdapterDataObserver(mSpaceDisplay);
    }

    /**
     * Called when the UI needs to react to the back button being pressed.
     *
     * @return Whether the back button was handled.
     */
    public boolean onBackPressed() {
        if (mMainView instanceof DrawerLayout) {
            DrawerLayout drawerLayout = (DrawerLayout) mMainView;
            if (drawerLayout.isDrawerOpen(Gravity.START)) {
                closeDrawer();
                return true;
            }
        }
        if (mSelectionDelegate.isSelectionEnabled()) {
            mSelectionDelegate.clearSelection();
            return true;
        }
        return false;
    }

    /**
     * @return The view that shows the main download UI.
     */
    public ViewGroup getView() {
        return mMainView;
    }

    @Override
    public DownloadDelegate getDownloadDelegate() {
        return DownloadManagerService.getDownloadManagerService(
                ContextUtils.getApplicationContext());
    }

    @Override
    public OfflinePageDownloadBridge getOfflinePageBridge() {
        return mOfflinePageBridge;
    }

    @Override
    public SelectionDelegate<DownloadHistoryItemWrapper> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    /**
     * Sets the download manager to the state that the url represents.
     */
    public void updateForUrl(String url) {
        int filter = DownloadFilter.getFilterFromUrl(url);
        onFilterChanged(filter);
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        if (item.getItemId() == R.id.close_menu_id && !DeviceFormFactor.isTablet(mActivity)) {
            mActivity.finish();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_delete_menu_id) {
            deleteSelectedItems();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_share_menu_id) {
            shareSelectedItems();
            return true;
        }
        return false;
    }

    /**
     * @see DrawerLayout#closeDrawer(int)
     */
    void closeDrawer() {
        if (mMainView instanceof DrawerLayout) {
            ((DrawerLayout) mMainView).closeDrawer(Gravity.START);
        }
    }

    /**
     * @return The activity that holds the download UI.
     */
    Activity getActivity() {
        return mActivity;
    }

    /** Called when the filter has been changed by the user. */
    void onFilterChanged(int filter) {
        mSelectionDelegate.clearSelection();

        for (DownloadUiObserver observer : mObservers) {
            observer.onFilterChanged(filter);
        }

        if (mNativePage != null) {
            mNativePage.onStateChange(DownloadFilter.getUrlForFilter(filter));
        }

        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Filter", filter,
                DownloadFilter.FILTER_BOUNDARY);
    }

    /**
     * Adds a {@link DownloadUiObserver} to observe the changes in the download manager.
     */
    void addObserver(DownloadUiObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes a {@link DownloadUiObserver} that were added in
     * {@link #addObserver(DownloadUiObserver)}
     */
    void removeObserver(DownloadUiObserver observer) {
        mObservers.removeObserver(observer);
    }

    private void shareSelectedItems() {
        List<DownloadHistoryItemWrapper> selectedItems = mSelectionDelegate.getSelectedItems();
        assert selectedItems.size() > 0;

        Intent shareIntent = new Intent();
        String intentAction;
        ArrayList<Uri> itemUris = new ArrayList<Uri>();
        StringBuilder offlinePagesString = new StringBuilder();
        int selectedItemsFilterType = selectedItems.get(0).getFilterType();

        String intentMimeType = "";
        String[] intentMimeParts = {"", ""};

        for (int i = 0; i < selectedItems.size(); i++) {
            DownloadHistoryItemWrapper wrappedItem  = selectedItems.get(i);
            if (wrappedItem.hasBeenExternallyRemoved()) continue;

            if (wrappedItem instanceof OfflinePageItemWrapper) {
                if (offlinePagesString.length() != 0) {
                    offlinePagesString.append("\n");
                }
                offlinePagesString.append(wrappedItem.getUrl());
            } else {
                itemUris.add(getUriForItem(wrappedItem));
            }

            if (selectedItemsFilterType != wrappedItem.getFilterType()) {
                selectedItemsFilterType = DownloadFilter.FILTER_ALL;
            }

            String mimeType = Intent.normalizeMimeType(wrappedItem.getMimeType());

            // If a mime type was not retrieved from the backend or could not be normalized,
            // set the mime type to the default.
            if (TextUtils.isEmpty(mimeType)) {
                intentMimeType = DEFAULT_MIME_TYPE;
                continue;
            }

            // If the intent mime type has not been set yet, set it to the mime type for this item.
            if (TextUtils.isEmpty(intentMimeType)) {
                intentMimeType = mimeType;
                if (!TextUtils.isEmpty(intentMimeType)) {
                    intentMimeParts = intentMimeType.split(MIME_TYPE_DELIMITER);
                    // Guard against invalid mime types.
                    if (intentMimeParts.length != 2) intentMimeType = DEFAULT_MIME_TYPE;
                }
                continue;
            }

            // Either the mime type is already the default or it matches the current item's mime
            // type. In either case, intentMimeType is already the correct value.
            if (TextUtils.equals(intentMimeType, DEFAULT_MIME_TYPE)
                    || TextUtils.equals(intentMimeType, mimeType)) {
                continue;
            }

            String[] mimeParts = mimeType.split(MIME_TYPE_DELIMITER);
            if (!TextUtils.equals(intentMimeParts[0], mimeParts[0])) {
                // The top-level types don't match; fallback to the default mime type.
                intentMimeType = DEFAULT_MIME_TYPE;
            } else {
                // The mime type should be {top-level type}/*
                intentMimeType = intentMimeParts[0] + MIME_TYPE_DELIMITER + "*";
            }
        }

        // If there are no non-deleted items to share, return early.
        if (itemUris.size() == 0 && offlinePagesString.length() == 0) {
            Toast.makeText(mActivity, mActivity.getString(R.string.download_cant_share_deleted),
                    Toast.LENGTH_SHORT).show();
            RecordUserAction.record("Android.DownloadManager.Share.Deleted");
            return;
        }

        // Use Action_SEND if there is only one downloaded item or only text to share.
        if (itemUris.size() == 0 || (itemUris.size() == 1 && offlinePagesString.length() == 0)) {
            intentAction = Intent.ACTION_SEND;
        } else {
            intentAction = Intent.ACTION_SEND_MULTIPLE;
        }

        if (itemUris.size() == 1 && offlinePagesString.length() == 0) {
            shareIntent.putExtra(Intent.EXTRA_STREAM, getUriForItem(selectedItems.get(0)));
        } else {
            shareIntent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, itemUris);
        }

        if (offlinePagesString.length() != 0) {
            shareIntent.putExtra(Intent.EXTRA_TEXT, offlinePagesString.toString());
        }

        shareIntent.setAction(intentAction);
        shareIntent.setType(intentMimeType);
        shareIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mActivity.startActivity(Intent.createChooser(shareIntent,
                mActivity.getString(R.string.share_link_chooser_title)));

        // TODO(twellington): ideally the intent chooser would be started with
        //                    startActivityForResult() and the selection would only be cleared after
        //                    receiving an OK response. See crbug.com/638916.
        mSelectionDelegate.clearSelection();

        recordShareHistograms(selectedItems.size(), selectedItemsFilterType);
    }

    private Uri getUriForItem(DownloadHistoryItemWrapper itemWrapper) {
        Uri uri = null;

        // #getContentUriFromFile causes a disk read when it calls into FileProvider#getUriForFile.
        // Obtaining a content URI is on the critical path for creating a share intent after the
        // user taps on the share button, so even if we were to run this method on a background
        // thread we would have to wait. As it depends on user-selected items, we cannot
        // know/preload which URIs we need until the user presses share.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            // Try to obtain a content:// URI, which is preferred to a file:/// URI so that
            // receiving apps don't attempt to determine the file's mime type (which often fails).
            uri = ContentUriUtils.getContentUriFromFile(mActivity.getApplicationContext(),
                    itemWrapper.getFile());
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Could not create content uri: " + e);
        }
        StrictMode.setThreadPolicy(oldPolicy);

        if (uri == null) uri = Uri.fromFile(itemWrapper.getFile());

        return uri;
    }

    private void deleteSelectedItems() {
        List<DownloadHistoryItemWrapper> selectedItems = mSelectionDelegate.getSelectedItems();
        mNumberOfFilesBeingDeleted.addAndGet(selectedItems.size());

        for (int i = 0; i < selectedItems.size(); i++) {
            DownloadHistoryItemWrapper wrappedItem  = selectedItems.get(i);

            // More than one download item may be associated with the same file path. After all
            // files have been deleted, initiate a check for removed download files so that any
            // download items associated with the same path as a deleted item are updated.
            wrappedItem.delete(new Callback<Void>() {
                @Override
                public void onResult(Void unused) {
                    int remaining = mNumberOfFilesBeingDeleted.decrementAndGet();
                    if (remaining != 0) return;

                    DownloadManagerService service =
                            DownloadManagerService.getDownloadManagerService(
                                    mActivity.getApplicationContext());
                    service.checkForExternallyRemovedDownloads(mIsOffTheRecord);
                }
            });
        }
        mSelectionDelegate.clearSelection();

        RecordUserAction.record("Android.DownloadManager.Delete");
    }

    private void addDrawerListener(DrawerLayout drawer) {
        drawer.addDrawerListener(new DrawerListener() {
            @Override
            public void onDrawerSlide(View drawerView, float slideOffset) {
            }

            @Override
            public void onDrawerOpened(View drawerView) {
                RecordUserAction.record("Android.DownloadManager.OpenDrawer");
            }

            @Override
            public void onDrawerClosed(View drawerView) {
            }

            @Override
            public void onDrawerStateChanged(int newState) {
            }
        });
    }

    private void recordShareHistograms(int count, int filterType) {
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Share.FileTypes",
                filterType, DownloadFilter.FILTER_BOUNDARY);

        RecordHistogram.recordLinearCountHistogram("Android.DownloadManager.Share.Count",
                count, 1, 20, 20);
    }
}
