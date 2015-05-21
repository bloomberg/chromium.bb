// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hosted;

import android.view.Menu;
import android.view.MenuItem;

import com.google.android.apps.chrome.R;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.appmenu.ChromeAppMenuPropertiesDelegate;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * App menu properties delegate for {@link HostedActivity}.
 */
public class HostedAppMenuPropertiesDelegate extends ChromeAppMenuPropertiesDelegate {
    private boolean mIsCustomEntryAdded;
    private List<String> mMenuEntries;
    private Map<MenuItem, Integer> mItemToIndexMap = new HashMap<MenuItem, Integer>();
    /**
     * Creates an {@link HostedAppMenuPropertiesDelegate} instance.
     */
    public HostedAppMenuPropertiesDelegate(ChromeActivity activity,
            List<String> menuEntries) {
        super(activity);
        mMenuEntries = menuEntries;
    }

    @Override
    public void prepareMenu(Menu menu) {
        Tab currentTab = mActivity.getActivityTab();
        if (currentTab != null) {
            MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
            forwardMenuItem.setEnabled(currentTab.canGoForward());

            mReloadMenuItem = menu.findItem(R.id.reload_menu_id);
            mReloadMenuItem.setIcon(R.drawable.btn_reload_stop);
            // Update the loading state of mReloadMenuItem.
            if (currentTab.isLoading()) loadingStateChanged(true);

            // Add custom menu items. Make sure they are only added once.
            if (!mIsCustomEntryAdded) {
                mIsCustomEntryAdded = true;
                for (int i = 0; i < mMenuEntries.size(); i++) {
                    MenuItem item = menu.add(0, 0, 1, mMenuEntries.get(i));
                    mItemToIndexMap.put(item, i);
                }
            }
        }
    }

    /**
     * @return The index that the given menu item should appear in the result of
     *         {@link HostedIntentDataProvider#getMenuTitles()}. Returns -1 if item not found.
     */
    public int getIndexOfMenuItem(MenuItem menuItem) {
        if (!mItemToIndexMap.containsKey(menuItem)) {
            return -1;
        }
        return mItemToIndexMap.get(menuItem).intValue();
    }
}
