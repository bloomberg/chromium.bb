// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.EmbedContentViewActivity;
import org.chromium.chrome.browser.datausage.DataUseTabUIManager;
import org.chromium.chrome.browser.datausage.DataUseTabUIManager.DataUsageUIAction;

/**
 * The controller for two data use snackbars:
 *
 * 1. When Chrome starts tracking data use in a Tab, it shows a snackbar informing the user that
 * data use tracking has started.
 *
 * 2. When Chrome stops tracking data use in a Tab, it shows a snackbar informing the user that
 * data use tracking has ended.
 */
public class DataUseSnackbarController implements SnackbarManager.SnackbarController {
    /** Snackbar types */
    private static final int STARTED_SNACKBAR = 0;
    private static final int ENDED_SNACKBAR = 1;

    private final SnackbarManager mSnackbarManager;
    private final Context mContext;

    /**
     * Creates an instance of a {@link DataUseSnackbarController}.
     * @param context The {@link Context} in which snackbar is shown.
     * @param snackbarManager The manager that helps to show up snackbar.
     */
    public DataUseSnackbarController(Context context, SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
        mContext = context;
    }

    public void showDataUseTrackingStartedBar() {
        mSnackbarManager.showSnackbar(Snackbar
                .make(mContext.getString(R.string.data_use_tracking_started_snackbar_message), this,
                        Snackbar.TYPE_NOTIFICATION)
                .setAction(mContext.getString(R.string.data_use_tracking_snackbar_action),
                        STARTED_SNACKBAR));
        DataUseTabUIManager.recordDataUseUIAction(DataUsageUIAction.STARTED_SNACKBAR_SHOWN);
    }

    public void showDataUseTrackingEndedBar() {
        mSnackbarManager.showSnackbar(Snackbar
                .make(mContext.getString(R.string.data_use_tracking_ended_snackbar_message), this,
                        Snackbar.TYPE_NOTIFICATION)
                .setAction(mContext.getString(R.string.data_use_tracking_snackbar_action),
                        ENDED_SNACKBAR));
        DataUseTabUIManager.recordDataUseUIAction(DataUsageUIAction.ENDED_SNACKBAR_SHOWN);
    }

    /**
     * Dismisses the snackbar.
     */
    public void dismissDataUseBar() {
        if (mSnackbarManager.isShowing()) mSnackbarManager.dismissSnackbars(this);
    }

    /**
     * Loads the "Learn more" page.
     */
    @Override
    public void onAction(Object actionData) {
        EmbedContentViewActivity.show(mContext, R.string.data_use_learn_more_title,
                R.string.data_use_learn_more_link_url);

        if (actionData == null) return;
        int snackbarType = (int) actionData;
        switch (snackbarType) {
            case STARTED_SNACKBAR:
                DataUseTabUIManager.recordDataUseUIAction(
                        DataUsageUIAction.STARTED_SNACKBAR_MORE_CLICKED);
                break;
            case ENDED_SNACKBAR:
                DataUseTabUIManager.recordDataUseUIAction(
                        DataUsageUIAction.ENDED_SNACKBAR_MORE_CLICKED);
                break;
            default:
                assert false;
                break;
        }
    }

    @Override
    public void onDismissNoAction(Object actionData) {}
}
