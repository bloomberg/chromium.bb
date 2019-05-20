// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.view.View;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.explore_sites.CategoryCardViewHolderFactory;
import org.chromium.chrome.browser.explore_sites.ExploreSitesPage;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * A variant of {@see ExploreSitesPage} that handles touchless context menus.
 */
public class TouchlessExploreSitesPage extends ExploreSitesPage {
    private final ModalDialogManager mModalDialogManager;
    private ChromeActivity mActivity;
    private TouchlessContextMenuManager mTouchlessContextMenuManager;

    /**
     * Create a new instance of the explore sites page.
     */
    public TouchlessExploreSitesPage(ChromeActivity activity, NativePageHost host) {
        super(activity, host);
        mModalDialogManager = activity.getModalDialogManager();
    }

    @Override
    protected void initialize(ChromeActivity activity, final NativePageHost host) {
        mActivity = activity;
        super.initialize(activity, host);
    }

    @Override
    protected ContextMenuManager createContextMenuManager(NativePageNavigationDelegate navDelegate,
            Runnable closeContextMenuCallback, String contextMenuUserActionPrefix) {
        mTouchlessContextMenuManager = new TouchlessContextMenuManager(mActivity,
                mActivity.getModalDialogManager(), navDelegate,
                (enabled) -> {}, closeContextMenuCallback, contextMenuUserActionPrefix);
        return mTouchlessContextMenuManager;
    }

    /** Manually finds the focused view and shows the context menu dialog. */
    public void showContextMenu() {
        View focusedView = getView().findFocus();
        if (focusedView == null) return;
        ContextMenuManager.Delegate delegate =
                ContextMenuManager.getDelegateFromFocusedView(focusedView);
        if (delegate == null) return;
        mTouchlessContextMenuManager.showTouchlessContextMenu(mModalDialogManager, delegate);
    }

    @Override
    protected CategoryCardViewHolderFactory createCategoryCardViewHolderFactory() {
        return new TouchlessCategoryCardViewHolderFactory();
    }
}
