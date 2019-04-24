// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.explore_sites.ExploreSitesPage;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * A variant of {@see ExploreSitesPage} that handles touchless context menus.
 */
public class TouchlessExploreSitesPage extends ExploreSitesPage {
    private TouchlessContextMenuManager mTouchlessContextMenuManager;
    private Context mContext;
    private ModalDialogManager mModalDialogManager;

    /**
     * Create a new instance of the explore sites page.
     */
    public TouchlessExploreSitesPage(ChromeActivity activity, NativePageHost host) {
        super(activity, host);
        mContext = activity;
        mModalDialogManager = activity.getModalDialogManager();
    }

    @Override
    protected ContextMenuManager createContextMenuManager(NativePageNavigationDelegate navDelegate,
            Runnable closeContextMenuCallback, String contextMenuUserActionPrefix) {
        mTouchlessContextMenuManager = new TouchlessContextMenuManager(navDelegate,
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
        mTouchlessContextMenuManager.showTouchlessContextMenu(
                mModalDialogManager, mContext, delegate);
    }
}
