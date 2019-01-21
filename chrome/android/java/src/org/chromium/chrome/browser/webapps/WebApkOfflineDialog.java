// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.net.NetError;

/**
 * A dialog to notify user of network errors while loading WebAPK's start URL.
 */
public class WebApkOfflineDialog {
    private Dialog mDialog;

    /** Returns whether the dialog is showing. */
    public boolean isShowing() {
        return mDialog != null && mDialog.isShowing();
    }

    /**
     * Shows dialog to notify user of network error.
     * @param activity Activity that will be used for {@link Dialog#show()}.
     * @param appName The name of the Android native client for which the dialog is shown.
     * @param errorCode The network error code.
     */
    public void show(final Activity activity, String appName, int errorCode) {
        AlertDialog.Builder builder = new AlertDialog.Builder(activity, R.style.AlertDialogTheme);
        builder.setMessage(getErrorDescription(activity, appName, errorCode))
                .setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        ApiCompatibilityUtils.finishAndRemoveTask(activity);
                    }
                });

        mDialog = builder.create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.show();
    }

    /** Closes the dialog. */
    public void cancel() {
        mDialog.cancel();
    }

    private String getErrorDescription(Activity activity, String appName, int errorCode) {
        switch (errorCode) {
            case NetError.ERR_INTERNET_DISCONNECTED:
                return activity.getString(R.string.webapk_offline_dialog, appName);
            case NetError.ERR_TUNNEL_CONNECTION_FAILED:
                return activity.getString(
                        R.string.webapk_network_error_message_tunnel_connection_failed);
            default:
                return activity.getString(R.string.webapk_cannot_connect_to_site);
        }
    }
}
