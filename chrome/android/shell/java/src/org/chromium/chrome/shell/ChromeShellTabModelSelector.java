// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.content.Context;

import org.chromium.chrome.browser.ChromeContentViewClient;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelDelegate;
import org.chromium.chrome.browser.tabmodel.TabModelOrderController;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.content.browser.ContentVideoViewClient;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

/**
 * Basic implementation of TabModelSelector for use in ChromeShell. Only has a regular TabModel,
 * no incognito one.
 */
class ChromeShellTabModelSelector extends TabModelSelectorBase {

    private final WindowAndroid mWindow;
    private final ContentVideoViewClient mContentVideoViewClient;
    private final Context mContext;
    private final TabModelOrderController mOrderController;

    private TabManager mTabManager;

    public ChromeShellTabModelSelector(
            WindowAndroid window, ContentVideoViewClient videoViewClient, Context context,
            TabManager tabManager) {
        mWindow = window;
        mContentVideoViewClient = videoViewClient;
        mContext = context;
        mOrderController = new TabModelOrderController(this);
        mTabManager = tabManager;

        TabModelDelegate tabModelDelegate = new TabModelDelegate() {
            @Override
            public void selectModel(boolean incognito) {
                assert !incognito;
            }

            @Override
            public void requestToShowTab(Tab tab, TabSelectionType type) {
            }

            @Override
            public boolean isSessionRestoreInProgress() {
                return false;
            }

            @Override
            public boolean isInOverviewMode() {
                return mTabManager.isTabSwitcherVisible();
            }

            @Override
            public TabModel getModel(boolean incognito) {
                return ChromeShellTabModelSelector.this.getModel(incognito);
            }

            @Override
            public TabModel getCurrentModel() {
                return ChromeShellTabModelSelector.this.getCurrentModel();
            }

            @Override
            public boolean closeAllTabsRequest(boolean incognito) {
                return false;
            }
        };
        TabModel tabModel = new ChromeShellTabModel(mOrderController, tabModelDelegate);
        initialize(false, tabModel, EmptyTabModel.getInstance());
        markTabStateInitialized();
    }

    @Override
    public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
            boolean incognito) {
        assert !incognito;
        ContentViewClient client = new ChromeContentViewClient() {
            @Override
            public ContentVideoViewClient getContentVideoViewClient() {
                return mContentVideoViewClient;
            }
        };
        ChromeShellTab tab = new ChromeShellTab(
                mContext, loadUrlParams, mWindow, client, mTabManager);
        int index = mOrderController.determineInsertionIndex(type, tab);
        TabModel tabModel = getCurrentModel();
        tabModel.addTab(tab, index, type);
        tabModel.setIndex(index, TabSelectionType.FROM_NEW);
        return tab;
    }

}
