// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.EmbedContentViewActivity;

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
        mSnackbarManager.showSnackbar(Snackbar.make(
                mContext.getString(R.string.data_use_tracking_started_snackbar_message), this)
                .setAction(mContext.getString(R.string.data_use_tracking_snackbar_action),
                        null));
        // TODO(megjablon): Add metrics.
    }

    public void showDataUseTrackingEndedBar() {
        mSnackbarManager.showSnackbar(Snackbar.make(
                mContext.getString(R.string.data_use_tracking_ended_snackbar_message), this)
                .setAction(mContext.getString(R.string.data_use_tracking_snackbar_action),
                        null));
        // TODO(megjablon): Add metrics.
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
        // TODO(megjablon): Add metrics.
        EmbedContentViewActivity.show(mContext, R.string.data_use_learn_more_title,
                R.string.data_use_learn_more_link_url);
    }

    @Override
    public void onDismissNoAction(Object actionData) {}

    @Override
    public void onDismissForEachType(boolean isTimeout) {}
}
