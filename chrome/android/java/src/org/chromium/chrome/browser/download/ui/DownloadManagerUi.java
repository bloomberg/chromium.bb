// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.app.Activity;
import android.content.Context;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.ActionBarDrawerToggle;
import android.support.v7.widget.Toolbar;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MenuItem;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Displays and manages the UI for the download manager.
 */
public class DownloadManagerUi extends DrawerLayout implements OnMenuItemClickListener {

    /** Responds to events occurring in the DownloadManagerUi. */
    public interface DownloadManagerUiDelegate {
        /** Called when the close button is clicked. */
        void onCloseButtonClicked(DownloadManagerUi ui);
    }

    private DownloadManagerUiDelegate mDelegate;
    private ActionBarDrawerToggle mActionBarDrawerToggle;
    private DownloadManagerToolbar mToolbar;

    public DownloadManagerUi(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mToolbar = (DownloadManagerToolbar) findViewById(R.id.action_bar);
        mToolbar.setOnMenuItemClickListener(this);
    }

    /**
     * Called when the UI needs to react to the back button being pressed.
     *
     * @return Whether the back button was handled.
     */
    public boolean onBackPressed() {
        if (isDrawerOpen(Gravity.START)) {
            closeDrawer(Gravity.START);
            return true;
        }
        return false;
    }

    /**
     * Initializes the UI for the given Activity.
     *
     * @param activity Activity that is showing the UI.
     */
    public void initialize(DownloadManagerUiDelegate delegate, Activity activity) {
        mDelegate = delegate;

        mActionBarDrawerToggle = new ActionBarDrawerToggle(activity,
                this, (Toolbar) findViewById(R.id.action_bar),
                R.string.accessibility_bookmark_drawer_toggle_btn_open,
                R.string.accessibility_bookmark_drawer_toggle_btn_close);
        addDrawerListener(mActionBarDrawerToggle);
        mActionBarDrawerToggle.syncState();

        FadingShadowView shadow = (FadingShadowView) findViewById(R.id.shadow);
        if (DeviceFormFactor.isLargeTablet(getContext())) {
            shadow.setVisibility(View.GONE);
        } else {
            shadow.init(ApiCompatibilityUtils.getColor(getResources(),
                    R.color.bookmark_app_bar_shadow_color), FadingShadow.POSITION_TOP);
        }

        ((Toolbar) findViewById(R.id.action_bar)).setTitle(R.string.menu_downloads);
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        if (item.getItemId() == R.id.close_menu_id && mDelegate != null) {
            mDelegate.onCloseButtonClicked(DownloadManagerUi.this);
            return true;
        }
        return false;
    }

}
