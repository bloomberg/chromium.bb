// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.toolbar;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ListPopupWindow;

import org.chromium.base.Log;
import org.chromium.blimp.R;
import org.chromium.blimp.session.BlimpClientSession;
import org.chromium.blimp.session.EngineInfo;
import org.chromium.blimp.settings.AboutBlimpPreferences;
import org.chromium.blimp.settings.Preferences;

import java.util.ArrayList;
import java.util.List;

/**
 * A PopupMenu attached to Blimp toolbar that presents various menu options to the user.
 */
public class ToolbarMenu implements BlimpClientSession.ConnectionObserver {
    /**
     * An interface to be notified of user actions on ToolbarMenu.
     */
    public interface ToolbarMenuDelegate {
        /**
         * Called to show the debug view.
         * @param show Show debug view if true, hide otherwise.
         */
        public void showDebugView(boolean show);
    }

    private static final String TAG = "ToolbarMenu";

    private Context mContext;
    private ListPopupWindow mPopupMenu;
    private Toolbar mToolbar;

    private static final int ID_OPEN_IN_CHROME = 0;
    private static final int ID_VERSION_INFO = 1;
    private static final int ID_TOGGLE_DEBUG_INFO = 2;

    private List<String> mMenuTitles;
    private ArrayAdapter<String> mPopupMenuAdapter;
    private EngineInfo mEngineInfo;

    // Flag to set the visibility of debug view.
    private boolean mDebugInfoEnabled = false;

    /**
     * Returns if the user has enabled debug view from the menu.
     * @return true if debug view is showing, false otherwise
     */
    public boolean isDebugInfoEnabled() {
        return mDebugInfoEnabled;
    }

    private ToolbarMenuDelegate mDelegate;

    public ToolbarMenu(Context context, Toolbar toolbar) {
        mContext = context;
        mDelegate = (ToolbarMenuDelegate) mContext;
        mToolbar = toolbar;
    }

    /**
     * Opens up a lazily created menu on the toolbar.
     * @param anchorView The view at which menu is to be anchored.
     */
    public void showMenu(View anchorView) {
        if (mPopupMenu == null) {
            initializeMenu(anchorView);
        }
        mPopupMenu.show();
        mPopupMenu.getListView().setDivider(null);
    }

    /**
     * Creates and initializes the app menu anchored to the specified view.
     * @param anchorView The anchor of the {@link ListPopupWindow}
     */
    private void initializeMenu(View anchorView) {
        mPopupMenu = new ListPopupWindow(mContext);
        intializeMenuAdapter();
        mPopupMenu.setAnchorView(anchorView);
        mPopupMenu.setWidth(
                mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_popup_item_width));
        mPopupMenu.setVerticalOffset(-anchorView.getHeight());
        mPopupMenu.setModal(true);
        mPopupMenu.setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                Log.d(TAG, "clicked " + position);
                switch (position) {
                    case ID_OPEN_IN_CHROME:
                        openInChrome();
                        break;
                    case ID_VERSION_INFO:
                        showVersionInfo();
                        break;
                    case ID_TOGGLE_DEBUG_INFO:
                        toggleDebugInfo();
                        break;
                    default:
                        assert false;
                        break;
                }
                mPopupMenu.dismiss();
            }
        });
    }

    /**
     * Creates an adapter for the toolbar menu.
     */
    private void intializeMenuAdapter() {
        mMenuTitles = new ArrayList<>();
        mMenuTitles.add(mContext.getString(R.string.open_in_chrome));
        mMenuTitles.add(mContext.getString(R.string.version_info));
        mMenuTitles.add(mContext.getString(
                mDebugInfoEnabled ? R.string.hide_debug_info : R.string.show_debug_info));

        mPopupMenuAdapter =
                new ArrayAdapter<String>(mContext, R.layout.toolbar_popup_item, mMenuTitles);
        mPopupMenu.setAdapter(mPopupMenuAdapter);
    }

    /**
     * Opens the current URL in chrome.
     */
    private void openInChrome() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(mToolbar.getUrl()));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage("com.android.chrome");
        try {
            mContext.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            // Chrome is not installed, so try with the default browser
            intent.setPackage(null);
            mContext.startActivity(intent);
        }
    }

    private void showVersionInfo() {
        Intent intent = new Intent();
        intent.setClass(mContext, Preferences.class);
        if (mEngineInfo != null) {
            intent.putExtra(AboutBlimpPreferences.EXTRA_ENGINE_IP, mEngineInfo.ipAddress);
            intent.putExtra(AboutBlimpPreferences.EXTRA_ENGINE_VERSION, mEngineInfo.engineVersion);
        }
        mContext.startActivity(intent);
    }

    private void toggleDebugInfo() {
        mDebugInfoEnabled = !mDebugInfoEnabled;
        mMenuTitles.set(ID_TOGGLE_DEBUG_INFO,
                mContext.getString(
                        mDebugInfoEnabled ? R.string.hide_debug_info : R.string.show_debug_info));
        mPopupMenuAdapter.notifyDataSetChanged();
        mDelegate.showDebugView(mDebugInfoEnabled);
    }

    // BlimpClientSession.ConnectionObserver interface.
    @Override
    public void onAssignmentReceived(
            int result, int suggestedMessageResourceId, EngineInfo engineInfo) {
        mEngineInfo = engineInfo;
    }

    @Override
    public void onConnected() {
        if (mEngineInfo == null) return;

        mEngineInfo.setConnected(true);
    }

    @Override
    public void onDisconnected(String reason) {
        if (mEngineInfo == null) return;

        mEngineInfo.setConnected(false);
    }

    @Override
    public void updateDebugStatsUI(int received, int sent, int commits) {}
}
