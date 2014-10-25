// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.ViewParent;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelDelegate;
import org.chromium.chrome.browser.tabmodel.TabModelOrderController;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.widget.accessibility.AccessibilityTabModelWrapper;
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
    private final ViewGroup mParent;
    private final TabModelOrderController mOrderController;

    private AccessibilityTabModelWrapper mTabModelWrapper;

    public ChromeShellTabModelSelector(
            WindowAndroid window, ContentVideoViewClient videoViewClient, ViewGroup parent) {
        mWindow = window;
        mContentVideoViewClient = videoViewClient;
        mParent = parent;
        mOrderController = new TabModelOrderController(this);

        TabModelDelegate tabModelDelegate = new TabModelDelegate() {
            @Override
            public void didCreateNewTab(Tab tab) {}

            @Override
            public void didChange() {}

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
                return isTabSwitcherVisible();
            }

            @Override
            public TabModel getModel(boolean incognito) {
                return ChromeShellTabModelSelector.this.getModel(incognito);
            }

            @Override
            public TabModel getCurrentModel() {
                return ChromeShellTabModelSelector.this.getCurrentModel();
            }
        };
        TabModel tabModel = new ChromeShellTabModel(mOrderController, tabModelDelegate);
        initialize(false, tabModel, EmptyTabModel.getInstance());
    }

    @Override
    public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
            boolean incognito) {
        assert !incognito;
        ContentViewClient client = new ContentViewClient() {
            @Override
            public ContentVideoViewClient getContentVideoViewClient() {
                return mContentVideoViewClient;
            }
        };
        ChromeShellTab tab = new ChromeShellTab(
                mParent.getContext(), loadUrlParams.getUrl(), mWindow, client);
        int index = mOrderController.determineInsertionIndex(type, tab);
        TabModel tabModel = getCurrentModel();
        tabModel.addTab(tab, index, type);
        tabModel.setIndex(index, TabSelectionType.FROM_NEW);
        return tab;
    }

    /**
     * Toggles the tab switcher visibility.
     */
    public void toggleTabSwitcher() {
        if (!isTabSwitcherVisible()) {
            showTabSwitcher();
        } else {
            hideTabSwitcher();
        }
    }

    /*
     * Hide the tab switcher.
     */
    public void hideTabSwitcher() {
        if (mTabModelWrapper == null) return;
        ViewParent parent = mTabModelWrapper.getParent();
        if (parent != null) {
            assert parent == mParent;
            mParent.removeView(mTabModelWrapper);
        }
    }

    private void showTabSwitcher() {
        if (mTabModelWrapper == null) {
            mTabModelWrapper = (AccessibilityTabModelWrapper) LayoutInflater.from(
                    mParent.getContext()).inflate(R.layout.accessibility_tab_switcher, null);
            mTabModelWrapper.setup(null);
            mTabModelWrapper.setTabModelSelector(this);
        }

        if (mTabModelWrapper.getParent() == null) {
            mParent.addView(mTabModelWrapper);
        }
    }

    private boolean isTabSwitcherVisible() {
        return mTabModelWrapper != null && mTabModelWrapper.getParent() == mParent;
    }
}
