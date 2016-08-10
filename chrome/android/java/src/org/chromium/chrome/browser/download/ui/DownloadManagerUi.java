// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.support.v4.widget.DrawerLayout;
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
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BasicNativePage;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Displays and manages the UI for the download manager.
 */

public class DownloadManagerUi implements OnMenuItemClickListener {

    /**
     * Interface to observe the changes in the download manager ui. This should be implemented by
     * the ui components that is shown, in order to let them get proper notifications.
     */
    public interface DownloadUiObserver {
        /**
         * Initializes the {@link DownloadUiObserver}.
         */
        public void initialize(DownloadManagerUi manager);

        /**
         * Called when the filter has been changed by the user.
         */
        public void onFilterChanged(int filter);

        /**
         * Called when the download manager is not shown anymore.
         */
        public void onManagerDestroyed(DownloadManagerUi manager);
    }

    private static final String TAG = "download_ui";
    private static final String DEFAULT_MIME_TYPE = "*/*";
    private static final String MIME_TYPE_DELIMITER = "/";

    private final DownloadHistoryAdapter mHistoryAdapter;
    private final FilterAdapter mFilterAdapter;
    private final ObserverList<DownloadUiObserver> mObservers = new ObserverList<>();

    private final Activity mActivity;
    private final ViewGroup mMainView;
    private final DownloadManagerToolbar mToolbar;
    private final SpaceDisplay mSpaceDisplay;
    private final ListView mFilterView;
    private final RecyclerView mRecyclerView;

    private BasicNativePage mNativePage;

    private SelectionDelegate<DownloadHistoryItemWrapper> mSelectionDelegate;

    public DownloadManagerUi(Activity activity) {
        mActivity = activity;
        mMainView = (ViewGroup) LayoutInflater.from(activity).inflate(R.layout.download_main, null);

        mSelectionDelegate = new SelectionDelegate<DownloadHistoryItemWrapper>();

        mHistoryAdapter = new DownloadHistoryAdapter();
        mHistoryAdapter.initialize(this);

        mSpaceDisplay = new SpaceDisplay(mMainView, mHistoryAdapter);
        mHistoryAdapter.registerAdapterDataObserver(mSpaceDisplay);
        mSpaceDisplay.onChanged();

        mFilterAdapter = new FilterAdapter();
        mFilterAdapter.initialize(this);

        mToolbar = (DownloadManagerToolbar) mMainView.findViewById(R.id.action_bar);
        mToolbar.initialize(this);
        mToolbar.setOnMenuItemClickListener(this);
        DrawerLayout drawerLayout = null;
        if (!DeviceFormFactor.isLargeTablet(activity)) {
            drawerLayout = (DrawerLayout) mMainView;
        }
        mToolbar.initialize(mSelectionDelegate, R.string.menu_downloads, drawerLayout,
                R.id.normal_menu_group, R.id.selection_mode_menu_group);

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
            observer.onManagerDestroyed(this);
            removeObserver(observer);
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

    /**
     * @return The SelectionDelegate responsible for tracking selected download items.
     */
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
        }
        if (item.getItemId() == R.id.selection_mode_share_menu_id) {
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

    /** Called when a particular download item has been clicked. */
    void onDownloadItemClicked(DownloadItem item) {
        boolean success = false;

        String mimeType = item.getDownloadInfo().getMimeType();
        String filePath = item.getDownloadInfo().getFilePath();
        Uri fileUri = Uri.fromFile(new File(filePath));

        // Check if any apps can open the file.
        Intent fileIntent = new Intent();
        fileIntent.setAction(Intent.ACTION_VIEW);
        fileIntent.setDataAndType(fileUri, mimeType);
        fileIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        List<ResolveInfo> handlers = ContextUtils.getApplicationContext().getPackageManager()
                .queryIntentActivities(fileIntent, PackageManager.MATCH_DEFAULT_ONLY);
        if (handlers.size() > 0) {
            Intent chooserIntent = Intent.createChooser(fileIntent, null);
            chooserIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            try {
                mActivity.startActivity(chooserIntent);
                success = true;
            } catch (ActivityNotFoundException e) {
                // Can't launch the Intent.
            }
        }

        if (!success) {
            Toast.makeText(mActivity, mActivity.getString(R.string.download_cant_open_file),
                    Toast.LENGTH_SHORT).show();
        }
    }

    /** Called when the filter has been changed by the user. */
    void onFilterChanged(int filter) {
        for (DownloadUiObserver observer : mObservers) {
            observer.onFilterChanged(filter);
        }
        if (mNativePage != null) {
            mNativePage.onStateChange(DownloadFilter.getUrlForFilter(filter));
        }
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
        // TODO(twellington): disable the share button when offline pages are selected.

        List<DownloadHistoryItemWrapper> selectedItems = mSelectionDelegate.getSelectedItems();
        assert selectedItems.size() > 0;

        Intent shareIntent = new Intent();
        String intentMimeType = Intent.normalizeMimeType(selectedItems.get(0).getMimeType());
        String intentAction;

        if (selectedItems.size() == 1) {
            // Set up intent for 1 item.
            intentAction = Intent.ACTION_SEND;
            shareIntent.putExtra(Intent.EXTRA_STREAM, selectedItems.get(0).getUri());
        } else {
            // Set up intent for multiple items.
            intentAction = Intent.ACTION_SEND_MULTIPLE;
            ArrayList<Uri> itemUris = new ArrayList<Uri>();

            String[] intentMimeParts = {"", ""};
            if (intentMimeType != null) intentMimeParts = intentMimeType.split(MIME_TYPE_DELIMITER);

            for (DownloadHistoryItemWrapper itemWrapper : mSelectionDelegate.getSelectedItems()) {
                itemUris.add(itemWrapper.getUri());

                String mimeType = Intent.normalizeMimeType(itemWrapper.getMimeType());

                // If a mime type was not retrieved from the backend or could not be normalized,
                // set the mime type to the default.
                if (mimeType == null) {
                    intentMimeType = DEFAULT_MIME_TYPE;
                    continue;
                }

                // Either the mime type is already the default or it matches the current intent
                // mime type. In either case, intentMimeType is already the correct value.
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

            shareIntent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, itemUris);
        }

        shareIntent.setAction(intentAction);
        shareIntent.setType(intentMimeType);
        shareIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mActivity.startActivity(Intent.createChooser(shareIntent,
                mActivity.getString(R.string.share_link_chooser_title)));
    }
}
