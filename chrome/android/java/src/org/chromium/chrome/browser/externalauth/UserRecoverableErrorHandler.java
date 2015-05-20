// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalauth;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;

import com.google.android.gms.common.GooglePlayServicesUtil;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Handles situations where Google Play Services encounters a user-recoverable
 * error. This is typically because Google Play Services requires an upgrade.
 * Three "canned" handlers are provided, with suggested use cases as described
 * below.
 * <br>
 * <strong>Silent</strong>: use this handler if the dependency is purely for
 * convenience and a (potentially suboptimal) fallback is available.
 * <br>
 * <strong>SystemNotification</strong>: use this handler if the dependency is
 * for a feature that the user isn't actively trying to access interactively.
 * To avoid excessively nagging the user, only one notification will ever be
 * shown during the lifetime of the process.
 * <br>
 * <strong>ModalDialog</strong>: use this handler if the dependency is
 * for a feature that the user is actively trying to access interactively where
 * the feature cannot function (or would be severely impaired) unless the
 * dependency is satisfied. The dialog will be presented as many times as the
 * user tries to access the feature.
 */
public abstract class UserRecoverableErrorHandler implements Runnable {
    /**
     * A handler that does nothing.
     */
    public static final class Silent extends UserRecoverableErrorHandler {
        @Override
        public void run() {
            // Do nothing.
        }
    }

    /**
     * A handler that displays a System Notification. To avoid repeatedly nagging the user, this
     * is done at most one time per process.
     * @see GooglePlayServicesUtil#showErrorNotification(int, Context)
     */
    public static final class SystemNotification extends UserRecoverableErrorHandler {
        /**
         * The error code returned from Google Play Services.
         */
        private final int mErrorCode;

        /**
         * The context in which the error was encountered.
         */
        private final Context mContext;

        /**
         * Tracks whether the notification has yet been shown.
         */
        private static final AtomicBoolean sNotificationShown = new AtomicBoolean(false);

        /**
         * Create a new System Notification handler for the specified context and error code.
         * @param context the context in which the error was encountered.
         * @param errorCode the error code from Google Play Services.
         */
        public SystemNotification(Context context, int errorCode) {
            mContext = context;
            mErrorCode = errorCode;
        }

        @Override
        public void run() {
            if (!sNotificationShown.getAndSet(true)) {
                return;
            }
            GooglePlayServicesUtil.showErrorNotification(mErrorCode, mContext);
        }
    }

    /**
     * A handler that displays a modal dialog. Unlike {@link SystemNotification}, this handler
     * will take action every time it is invoked.
     * @see GooglePlayServicesUtil#getErrorDialog(int, Activity, int,
     * android.content.DialogInterface.OnCancelListener)
     */
    public static final class ModalDialog extends UserRecoverableErrorHandler {
        /**
         * The error code returned from Google Play Services.
         */
        private final int mErrorCode;

        /**
         * The activity in which the error was encountered.
         */
        private final Activity mActivity;

        /**
         * The request code given when calling startActivityForResult.
         */
        private final int mRequestCode;

        /**
         * The DialogInterface.OnCancelListener to invoke if the dialog is canceled.
         */
        private final DialogInterface.OnCancelListener mOnCancelListener;

        /**
         * Create a new Modal Dialog handler for the specified activity and error code.
         * @param activity the activity in which the dialog is to be displayed.
         * @param errorCode the error code from Google Play Services.
         * @param requestCode the request code given when calling startActivityForResult.
         * @param onCancelListener the DialogInterface.OnCancelListener to invoke if the dialog
         * is canceled.
         */
        public ModalDialog(Activity activity, int errorCode, int requestCode,
                DialogInterface.OnCancelListener onCancelListener) {
            mActivity = activity;
            mErrorCode = errorCode;
            mRequestCode = requestCode;
            mOnCancelListener = onCancelListener;
        }

        @Override
        public void run() {
            Dialog dialog = GooglePlayServicesUtil.getErrorDialog(
                    mErrorCode, mActivity, mRequestCode, mOnCancelListener);
            dialog.show();
        }
    }
}