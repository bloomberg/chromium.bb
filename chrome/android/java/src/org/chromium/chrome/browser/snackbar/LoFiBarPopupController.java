// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;

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
    private final boolean mDisabled;
    private Tab mTab;
    private boolean mLoFiPopupShownForPageLoad = false;

    /**
     * Creates an instance of a {@link LoFiBarPopupController}.
     * @param context The {@link Context} in which snackbar is shown.
     * @param snackbarManager The manager that helps to show up snackbar.
     */
    public LoFiBarPopupController(Context context, SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
        mContext = context;
        mDisabled = CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_LOFI_SNACKBAR);
    }

    /**
     * Called on new page loads to indicate that a Lo-Fi snackbar has not been shown yet.
     */
    public void resetLoFiPopupShownForPageLoad() {
        mLoFiPopupShownForPageLoad = false;
    }

    /**
     * If a Lo-Fi snackbar has not been shown for the current page load, creates a Lo-Fi snackbar
     * for the given tab and shows it. If the tab is hidden, waits until it is visible to show the
     * snackbar.
     *
     * @param tab The tab to show the snackbar on.
     * @param isPreview Whether the snackbar should have the Lo-Fi preview message.
     */
    public void maybeCreateLoFiBar(Tab tab, final boolean isPreview) {
        if (mLoFiPopupShownForPageLoad) return;
        mLoFiPopupShownForPageLoad = true;
        if (tab.isHidden()) {
            TabObserver tabObserver = new EmptyTabObserver() {
                @Override
                public void onShown(Tab tab) {
                    showLoFiBar(tab, isPreview);
                    tab.removeObserver(this);
                }

                @Override
                public void onHidden(Tab tab) {
                    dismissLoFiBar();
                }

                @Override
                public void onDestroyed(Tab tab) {
                    dismissLoFiBar();
                }
            };
            tab.addObserver(tabObserver);
            return;
        }
        showLoFiBar(tab, isPreview);
    }

    /**
     * @param tab The tab. Saved to reload the page.
     */
    private void showLoFiBar(Tab tab, boolean isPreview) {
        if (mDisabled) return;
        mTab = tab;
        String message = mContext
                .getString(isPreview ? R.string.data_reduction_lo_fi_preview_snackbar_message
                        : R.string.data_reduction_lo_fi_snackbar_message);
        String buttonText = mContext
                .getString(isPreview ? R.string.data_reduction_lo_fi_preview_snackbar_action
                        : R.string.data_reduction_lo_fi_snackbar_action);
        mSnackbarManager.showSnackbar(Snackbar.make(message, this, Snackbar.TYPE_NOTIFICATION)
                .setAction(buttonText, tab.getId())
                .setDuration(DEFAULT_LO_FI_SNACKBAR_SHOW_DURATION_MS));
        DataReductionProxySettings.getInstance().incrementLoFiSnackbarShown();
        DataReductionProxyUma.dataReductionProxyLoFiUIAction(
                DataReductionProxyUma.ACTION_LOAD_IMAGES_SNACKBAR_SHOWN);
    }

    /**
     * Dismisses the snackbar.
     */
    private void dismissLoFiBar() {
        if (mSnackbarManager.isShowing()) mSnackbarManager.dismissSnackbars(this);
    }

    /**
     * Reloads the page showing all images.
     */
    @Override
    public void onAction(Object actionData) {
        mSnackbarManager.dismissSnackbars(this);
        mTab.reloadDisableLoFi();
        DataReductionProxySettings.getInstance().incrementLoFiUserRequestsForImages();
        DataReductionProxyUma.dataReductionProxyLoFiUIAction(
                DataReductionProxyUma.ACTION_LOAD_IMAGES_SNACKBAR_CLICKED);
    }

    @Override
    public void onDismissNoAction(Object actionData) {}
}