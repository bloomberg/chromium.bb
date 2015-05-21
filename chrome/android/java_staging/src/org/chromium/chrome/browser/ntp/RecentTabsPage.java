// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Color;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListView;

import com.google.android.apps.chrome.R;

import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.util.ViewUtils;

/**
 * The native recent tabs page. Lists recently closed tabs, open windows and tabs from the user's
 * synced devices, and snapshot documents sent from Chrome to Mobile in an expandable list view.
 */
public class RecentTabsPage implements NativePage,
        ExpandableListView.OnChildClickListener, ExpandableListView.OnGroupCollapseListener,
        ExpandableListView.OnGroupExpandListener, RecentTabsManager.UpdatedCallback,
        View.OnCreateContextMenuListener, InvalidationAwareThumbnailProvider {

    private final Activity mActivity;
    private final ExpandableListView mListView;
    private final String mTitle;
    private final ViewGroup mView;

    private RecentTabsManager mRecentTabsManager;
    private RecentTabsRowAdapter mAdapter;

    private boolean mSnapshotContentChanged;
    private int mSnapshotListPosition;
    private int mSnapshotListTop;
    private int mSnapshotWidth;
    private int mSnapshotHeight;

    /**
     * Constructor returns an instance of RecentTabsPage.
     *
     * @param activity The activity this view belongs to.
     * @param recentTabsManager The RecentTabsManager which provides the model data.
     */
    public RecentTabsPage(Activity activity, RecentTabsManager recentTabsManager) {
        mActivity = activity;
        mRecentTabsManager = recentTabsManager;

        mTitle = activity.getResources().getString(R.string.recent_tabs);
        mRecentTabsManager.setUpdatedCallback(this);
        LayoutInflater inflater = LayoutInflater.from(activity);
        mView = (ViewGroup) inflater.inflate(R.layout.recent_tabs_page, null);
        mListView = (ExpandableListView) mView.findViewById(R.id.odp_listview);
        mAdapter = buildAdapter(activity, recentTabsManager);
        mListView.setAdapter(mAdapter);
        mListView.setOnChildClickListener(this);
        mListView.setGroupIndicator(null);
        mListView.setOnGroupCollapseListener(this);
        mListView.setOnGroupExpandListener(this);
        mListView.setOnCreateContextMenuListener(this);

        onUpdated();
    }

    private static RecentTabsRowAdapter buildAdapter(Activity activity,
            RecentTabsManager recentTabsManager) {
        return new RecentTabsRowAdapter(activity, recentTabsManager);
    }

    // NativePage overrides

    @Override
    public String getUrl() {
        return UrlConstants.RECENT_TABS_URL;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return Color.WHITE;
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public String getHost() {
        return UrlConstants.RECENT_TABS_HOST;
    }

    @Override
    public void destroy() {
        assert getView().getParent() == null : "Destroy called before removed from window";
        mRecentTabsManager.destroy();
        mRecentTabsManager = null;
        mAdapter.notifyDataSetInvalidated();
        mAdapter = null;
        mListView.setAdapter((RecentTabsRowAdapter) null);
    }

    @Override
    public void updateForUrl(String url) {
    }

    // ExpandableListView.OnChildClickedListener
    @Override
    public boolean onChildClick(ExpandableListView parent, View v, int groupPosition,
            int childPosition, long id) {
        return mAdapter.getGroup(groupPosition).onChildClick(childPosition);
    }

    // ExpandableListView.OnGroupExpandedListener
    @Override
    public void onGroupExpand(int groupPosition) {
        mAdapter.getGroup(groupPosition).setCollapsed(false);
        mSnapshotContentChanged = true;
    }

    // ExpandableListView.OnGroupCollapsedListener
    @Override
    public void onGroupCollapse(int groupPosition) {
        mAdapter.getGroup(groupPosition).setCollapsed(true);
        mSnapshotContentChanged = true;
    }

    // RecentTabsManager.UpdatedCallback
    @Override
    public void onUpdated() {
        mAdapter.notifyDataSetChanged();
        for (int i = 0; i < mAdapter.getGroupCount(); i++) {
            if (mAdapter.getGroup(i).isCollapsed()) {
                mListView.collapseGroup(i);
            } else {
                mListView.expandGroup(i);
            }
        }
        mSnapshotContentChanged = true;
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        // Would prefer to have this context menu view managed internal to RecentTabsGroupView
        // Unfortunately, setting either onCreateContextMenuListener or onLongClickListener
        // disables the native onClick (expand/collapse) behaviour of the group view.
        ExpandableListView.ExpandableListContextMenuInfo info =
                (ExpandableListView.ExpandableListContextMenuInfo) menuInfo;

        int type = ExpandableListView.getPackedPositionType(info.packedPosition);
        int groupPosition = ExpandableListView.getPackedPositionGroup(info.packedPosition);

        if (type == ExpandableListView.PACKED_POSITION_TYPE_GROUP) {
            mAdapter.getGroup(groupPosition).onCreateContextMenuForGroup(menu, mActivity);
        } else if (type == ExpandableListView.PACKED_POSITION_TYPE_CHILD) {
            int childPosition = ExpandableListView.getPackedPositionChild(info.packedPosition);
            mAdapter.getGroup(groupPosition).onCreateContextMenuForChild(childPosition, menu,
                    mActivity);
        }
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        if (mView.getWidth() == 0 || mView.getHeight() == 0) return false;

        View topItem = mListView.getChildAt(0);
        return mSnapshotContentChanged
                || mSnapshotListPosition != mListView.getFirstVisiblePosition()
                || mSnapshotListTop != (topItem == null ? 0 : topItem.getTop())
                || mView.getWidth() != mSnapshotWidth
                || mView.getHeight() != mSnapshotHeight;
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        ViewUtils.captureBitmap(mView, canvas);
        mSnapshotContentChanged = false;
        mSnapshotListPosition = mListView.getFirstVisiblePosition();
        View topItem = mListView.getChildAt(0);
        mSnapshotListTop = topItem == null ? 0 : topItem.getTop();
        mSnapshotWidth = mView.getWidth();
        mSnapshotHeight = mView.getHeight();
    }
}
