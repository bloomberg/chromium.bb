// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.bandwidth.DataReductionProxyUma;

/**
 * Each time a tab loads with Lo-Fi this controller saves that tab id and title to the stack of
 * SnackbarManager. It will then let SnackbarManager show a snackbar representing the top entry
 * of the stack.
 * <p/>
 * When the load images button is clicked, it will reload the page without Lo-Fi.
 */
public class LoFiBarPopupController implements SnackbarManager.SnackbarController {
    private static final int DEFAULT_LO_FI_SNACKBAR_SHOW_DURATION_MS = 6000;
    private final SnackbarManager mSnackbarManager;
    private final Context mContext;
    private Tab mTab;

    /**
     * Creates an instance of a {@link LoFiBarPopupController}.
     * @param context The {@link Context} in which snackbar is shown.
     * @param snackbarManager The manager that helps to show up snackbar.
     */
    public LoFiBarPopupController(Context context, SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
        mContext = context;
    }

    /**
     * @param tab The tab. Saved to reload the page.
     */
    public void showLoFiBar(Tab tab) {
        mTab = tab;
        mSnackbarManager.showSnackbar(
                null, mContext.getString(R.string.data_reduction_lo_fi_snackbar_message),
                mContext.getString(R.string.data_reduction_lo_fi_snackbar_action),
                tab.getId(), this, DEFAULT_LO_FI_SNACKBAR_SHOW_DURATION_MS);
        DataReductionProxyUma.dataReductionProxyLoFiUIAction(
                DataReductionProxyUma.ACTION_LOAD_IMAGES_SNACKBAR_SHOWN);
    }

    /**
     * Dismisses the snackbar.
     */
    public void dismissLoFiBar() {
        if (mSnackbarManager.isShowing()) mSnackbarManager.removeSnackbarEntry(this);
    }

    /**
     * Reloads the page showing all images.
     */
    @Override
    public void onAction(Object actionData) {
        mSnackbarManager.dismissSnackbar(false);
        mTab.stopLoading();
        mTab.reloadIgnoringCache();
        DataReductionProxySettings.getInstance().incrementLoFiUserRequestsForImages();
        DataReductionProxyUma.dataReductionProxyLoFiUIAction(
                DataReductionProxyUma.ACTION_LOAD_IMAGES_SNACKBAR_CLICKED);
    }

    @Override
    public void onDismissNoAction(Object actionData) {}

    @Override
    public void onDismissForEachType(boolean isTimeout) {}
}