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
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;

import java.io.File;
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
        public void onDestroy(DownloadManagerUi manager);
    }

    private static final String TAG = "download_ui";

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

    public DownloadManagerUi(Activity activity) {
        mActivity = activity;
        mMainView = (ViewGroup) LayoutInflater.from(activity).inflate(R.layout.download_main, null);

        mHistoryAdapter = new DownloadHistoryAdapter();
        mHistoryAdapter.initialize(this);

        mFilterAdapter = new FilterAdapter();
        mFilterAdapter.initialize(this);

        mToolbar = (DownloadManagerToolbar) mMainView.findViewById(R.id.action_bar);
        mToolbar.initialize(this);
        mToolbar.setOnMenuItemClickListener(this);

        mSpaceDisplay = new SpaceDisplay(mMainView, mHistoryAdapter);
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

        mSpaceDisplay.onChanged();
        mToolbar.setTitle(R.string.menu_downloads);

        getDownloadManagerService().addDownloadHistoryAdapter(mHistoryAdapter);
        mHistoryAdapter.registerAdapterDataObserver(mSpaceDisplay);

        // TODO(ianwen): add support for loading state.
        getDownloadManagerService().getAllDownloads();
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
        getDownloadManagerService().removeDownloadHistoryAdapter(mHistoryAdapter);
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
        return false;
    }

    /**
     * @return The view that shows the main download UI.
     */
    public ViewGroup getView() {
        return mMainView;
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

    private static DownloadManagerService getDownloadManagerService() {
        return DownloadManagerService.getDownloadManagerService(
                ContextUtils.getApplicationContext());
    }

}
