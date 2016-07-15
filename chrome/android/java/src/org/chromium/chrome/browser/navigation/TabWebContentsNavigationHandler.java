// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.navigation;

import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides navigation functionalities for Chrome. It wraps the content layer NavigationController
 * and delegates all calls to it.
 */
public class TabWebContentsNavigationHandler implements NavigationHandler {
    private NavigationController mNavigationController;

    public TabWebContentsNavigationHandler(WebContents webContents) {
        mNavigationController = webContents.getNavigationController();
    }

    @Override
    public boolean canGoBack() {
        return mNavigationController.canGoBack();
    }

    @Override
    public boolean canGoForward() {
        return mNavigationController.canGoForward();
    }

    @Override
    public boolean canGoToOffset(int offset) {
        return mNavigationController.canGoToOffset(offset);
    }

    @Override
    public void goToOffset(int offset) {
        mNavigationController.goToOffset(offset);
    }

    @Override
    public void goToNavigationIndex(int index) {
        mNavigationController.goToNavigationIndex(index);
    }

    @Override
    public void goBack() {
        mNavigationController.goBack();
    }

    @Override
    public void goForward() {
        mNavigationController.goForward();
    }

    @Override
    public boolean isInitialNavigation() {
        return mNavigationController.isInitialNavigation();
    }

    @Override
    public void loadIfNecessary() {
        mNavigationController.loadIfNecessary();
    }

    @Override
    public void requestRestoreLoad() {
        mNavigationController.requestRestoreLoad();
    }

    @Override
    public void reload(boolean checkForRepost) {
        mNavigationController.reload(checkForRepost);
    }

    @Override
    public void reloadToRefreshContent(boolean checkForRepost) {
        mNavigationController.reloadToRefreshContent(checkForRepost);
    }

    @Override
    public void reloadBypassingCache(boolean checkForRepost) {
        mNavigationController.reloadBypassingCache(checkForRepost);
    }

    @Override
    public void reloadDisableLoFi(boolean checkForRepost) {
        mNavigationController.reloadDisableLoFi(checkForRepost);
    }

    @Override
    public void cancelPendingReload() {
        mNavigationController.cancelPendingReload();
    }

    @Override
    public void continuePendingReload() {
        mNavigationController.continuePendingReload();
    }

    @Override
    public void loadUrl(LoadUrlParams params) {
        mNavigationController.loadUrl(params);
    }

    @Override
    public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
        return mNavigationController.getDirectedNavigationHistory(isForward, itemLimit);
    }

    @Override
    public String getOriginalUrlForVisibleNavigationEntry() {
        return mNavigationController.getOriginalUrlForVisibleNavigationEntry();
    }

    @Override
    public void clearSslPreferences() {
        mNavigationController.clearSslPreferences();
    }

    @Override
    public boolean getUseDesktopUserAgent() {
        return mNavigationController.getUseDesktopUserAgent();
    }

    @Override
    public void setUseDesktopUserAgent(boolean override, boolean reloadOnChange) {
        mNavigationController.setUseDesktopUserAgent(override, reloadOnChange);
    }

    @Override
    public NavigationEntry getEntryAtIndex(int index) {
        return mNavigationController.getEntryAtIndex(index);
    }

    @Override
    public int getLastCommittedEntryIndex() {
        return mNavigationController.getLastCommittedEntryIndex();
    }

    @Override
    public boolean removeEntryAtIndex(int index) {
        return mNavigationController.removeEntryAtIndex(index);
    }
}
