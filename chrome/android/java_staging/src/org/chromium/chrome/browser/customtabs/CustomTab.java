// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.Menu;

import com.google.android.apps.chrome.R;

import org.chromium.chrome.browser.CompositorChromeActivity;
import org.chromium.chrome.browser.ContentViewUtil;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuParams;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.tab.ChromeTab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * A chrome tab that is only used as a custom tab.
 */
public class CustomTab extends ChromeTab {

    private TabChromeContextMenuItemDelegate
            mContextMenuDelegate = new TabChromeContextMenuItemDelegate() {
                @Override
                public boolean startDownload(String url, boolean isLink) {
                    // Behave similarly to ChromeTabChromeContextMenuItemDelegate in ChromeTab.
                    return !isLink || !shouldInterceptContextMenuDownload(url);
                }
            };

    /**
     * Construct an CustomTab. It might load a prerendered {@link WebContents} for the URL, if
     * {@link ChromeConnectionService} has successfully warmed up for the url.
     */
    public CustomTab(CompositorChromeActivity activity, WindowAndroid windowAndroid,
            long sessionId, String url, int parentTabId) {
        super(Tab.generateValidId(Tab.INVALID_TAB_ID), activity, false, windowAndroid,
                TabLaunchType.FROM_EXTERNAL_APP, parentTabId, null, null);
        WebContents webContents = ChromeBrowserConnection.getInstance(activity.getApplication())
                                          .takePrerenderedUrl(sessionId, url, null);
        if (webContents == null) {
            webContents = ContentViewUtil.createWebContents(isIncognito(), false);
        }
        initialize(webContents, activity.getTabContentManager(), false);
        getView().requestFocus();
    }

    @Override
    protected ContextMenuPopulator createContextMenuPopulator() {
        return new ChromeContextMenuPopulator(mContextMenuDelegate) {
            @Override
            public void buildContextMenu(ContextMenu menu, Context context,
                    ContextMenuParams params) {
                String linkUrl = params.getLinkUrl();
                if (linkUrl != null) linkUrl = linkUrl.trim();
                if (!TextUtils.isEmpty(linkUrl)) {
                    menu.add(Menu.NONE, org.chromium.chrome.R.id.contextmenu_copy_link_address_text,
                            Menu.NONE, org.chromium.chrome.R.string.contextmenu_copy_link_address);
                }

                String linkText = params.getLinkText();
                if (linkText != null) linkText = linkText.trim();
                if (!TextUtils.isEmpty(linkText)) {
                    menu.add(Menu.NONE, org.chromium.chrome.R.id.contextmenu_copy_link_text,
                            Menu.NONE, org.chromium.chrome.R.string.contextmenu_copy_link_text);
                }
                if (params.isImage()) {
                    menu.add(Menu.NONE, R.id.contextmenu_save_image, Menu.NONE,
                            R.string.contextmenu_save_image);
                    menu.add(Menu.NONE, R.id.contextmenu_open_image, Menu.NONE,
                            R.string.contextmenu_open_image);
                    menu.add(Menu.NONE, R.id.contextmenu_copy_image, Menu.NONE,
                            R.string.contextmenu_copy_image);
                    menu.add(Menu.NONE, R.id.contextmenu_copy_image_url, Menu.NONE,
                            R.string.contextmenu_copy_image_url);
                } else if (UrlUtilities.isDownloadableScheme(params.getLinkUrl())) {
                    // "Save link" is not shown for image.
                    menu.add(Menu.NONE, R.id.contextmenu_save_link_as, Menu.NONE,
                            R.string.contextmenu_save_link);
                }
            }
        };
    }
}
