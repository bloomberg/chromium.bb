// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.StrictMode;
import android.view.Menu;
import android.view.MenuItem;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.tab.Tab;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * App menu properties delegate for {@link CustomTabActivity}.
 */
public class CustomTabAppMenuPropertiesDelegate extends AppMenuPropertiesDelegate {
    private static final String SAMPLE_URL = "https://www.google.com";
    private boolean mIsCustomEntryAdded;
    private boolean mShowShare;
    private List<String> mMenuEntries;
    private Map<MenuItem, Integer> mItemToIndexMap = new HashMap<MenuItem, Integer>();
    /**
     * Creates an {@link CustomTabAppMenuPropertiesDelegate} instance.
     */
    public CustomTabAppMenuPropertiesDelegate(ChromeActivity activity, List<String> menuEntries,
            boolean showShare) {
        super(activity);
        mMenuEntries = menuEntries;
        mShowShare = showShare;
    }

    @Override
    public void prepareMenu(Menu menu) {
        Tab currentTab = mActivity.getActivityTab();
        if (currentTab != null) {
            MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
            forwardMenuItem.setEnabled(currentTab.canGoForward());

            mReloadMenuItem = menu.findItem(R.id.reload_menu_id);
            mReloadMenuItem.setIcon(R.drawable.btn_reload_stop);
            loadingStateChanged(currentTab.isLoading());

            MenuItem shareItem = menu.findItem(R.id.share_menu_id);
            shareItem.setVisible(mShowShare);
            shareItem.setEnabled(mShowShare);

            MenuItem openInChromeItem = menu.findItem(R.id.open_in_browser_id);
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(SAMPLE_URL));
            PackageManager pm = mActivity.getPackageManager();
            ResolveInfo info = null;

            // Temporarily allowing disk access while fixing. TODO: http://crbug.com/581856
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            StrictMode.allowThreadDiskWrites();
            try {
                info = pm.resolveActivity(intent, 0);
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }

            String menuItemTitle = info != null && info.match != 0
                    ? mActivity.getString(
                            R.string.menu_open_in_product, info.loadLabel(pm).toString())
                    : mActivity.getString(R.string.menu_open_in_product_default);
            openInChromeItem.setTitle(menuItemTitle);

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
     *         {@link CustomTabIntentDataProvider#getMenuTitles()}. Returns -1 if item not found.
     */
    public int getIndexOfMenuItem(MenuItem menuItem) {
        if (!mItemToIndexMap.containsKey(menuItem)) {
            return -1;
        }
        return mItemToIndexMap.get(menuItem).intValue();
    }

    @Override
    public int getFooterResourceId() {
        return R.layout.powered_by_chrome_footer;
    }

    /**
     * Get the {@link MenuItem} object associated with the given title. If multiple menu items have
     * the same title, a random one will be returned. This method is for testing purpose _only_.
     */
    @VisibleForTesting
    MenuItem getMenuItemForTitle(String title) {
        for (MenuItem item : mItemToIndexMap.keySet()) {
            if (item.getTitle().equals(title)) return item;
        }
        return null;
    }
}