// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.Context;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.ActionBarDrawerToggle;
import android.support.v7.widget.Toolbar;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi.DownloadUiObserver;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Handles toolbar functionality for the {@link DownloadManagerUi}.
 */
public class DownloadManagerToolbar extends Toolbar implements DownloadUiObserver {

    private ActionBarDrawerToggle mActionBarDrawerToggle;

    public DownloadManagerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateMenu(R.menu.download_manager_menu);
    }

    @Override
    public void initialize(DownloadManagerUi manager) {
        if (manager.getView() instanceof DrawerLayout) {
            DrawerLayout drawerLayout = (DrawerLayout) manager.getView();
            mActionBarDrawerToggle = new ActionBarDrawerToggle(manager.getActivity(),
                    drawerLayout, (Toolbar) manager.getView().findViewById(R.id.action_bar),
                    R.string.accessibility_bookmark_drawer_toggle_btn_open,
                    R.string.accessibility_bookmark_drawer_toggle_btn_close);
            drawerLayout.addDrawerListener(mActionBarDrawerToggle);
            mActionBarDrawerToggle.syncState();
        }
        if (DeviceFormFactor.isTablet(getContext())) {
            getMenu().findItem(R.id.close_menu_id).setVisible(false);
        }
    }

    @Override
    public void onFilterChanged(int filter) {
        if (filter == DownloadFilter.FILTER_ALL) {
            setTitle(R.string.menu_downloads);
        } else {
            setTitle(DownloadFilter.getStringIdForFilter(filter));
        }
    }

    @Override
    public void onDestroy(DownloadManagerUi manager) {
        manager.removeObserver(this);
    }
}
