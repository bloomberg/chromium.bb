// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.Manifest.permission;
import android.app.Activity;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.support.v7.app.AlertDialog;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.PermissionCallback;

/**
 * Java counterpart of android DownloadController.
 *
 * Its a singleton class instantiated by the C++ DownloadController.
 */
public class DownloadController {
    private static final String LOGTAG = "DownloadController";
    private static final DownloadController sInstance = new DownloadController();

    /**
     * Class for notifying the application that download has completed.
     */
    public interface DownloadNotificationService {
        /**
         * Notify the host application that a download is finished.
         * @param downloadInfo Information about the completed download.
         */
        void onDownloadCompleted(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is in progress.
         * @param downloadInfo Information about the in-progress download.
         */
        void onDownloadUpdated(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is cancelled.
         * @param downloadInfo Information about the cancelled download.
         */
        void onDownloadCancelled(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is interrupted.
         * @param downloadInfo Information about the completed download.
         * @param isAutoResumable Download can be auto resumed when network becomes available.
         */
        void onDownloadInterrupted(final DownloadInfo downloadInfo, boolean isAutoResumable);
    }

    private static DownloadNotificationService sDownloadNotificationService;

    @CalledByNative
    public static DownloadController getInstance() {
        return sInstance;
    }

    private DownloadController() {
        nativeInit();
    }

    public static void setDownloadNotificationService(DownloadNotificationService service) {
        sDownloadNotificationService = service;
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private void onDownloadCompleted(DownloadInfo downloadInfo) {
        if (sDownloadNotificationService == null) return;
        sDownloadNotificationService.onDownloadCompleted(downloadInfo);
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private void onDownloadInterrupted(DownloadInfo downloadInfo, boolean isAutoResumable) {
        if (sDownloadNotificationService == null) return;
        sDownloadNotificationService.onDownloadInterrupted(downloadInfo, isAutoResumable);
    }

    /**
     * Called when a download was cancelled.
     */
    @CalledByNative
    private void onDownloadCancelled(DownloadInfo downloadInfo) {
        if (sDownloadNotificationService == null) return;
        sDownloadNotificationService.onDownloadCancelled(downloadInfo);
    }

    /**
     * Notifies the download delegate about progress of a download. Downloads that use Chrome
     * network stack use custom notification to display the progress of downloads.
     */
    @CalledByNative
    private void onDownloadUpdated(DownloadInfo downloadInfo) {
        if (sDownloadNotificationService == null) return;
        sDownloadNotificationService.onDownloadUpdated(downloadInfo);
    }


    /**
     * Returns whether file access is allowed.
     *
     * @return true if allowed, or false otherwise.
     */
    @CalledByNative
    private boolean hasFileAccess() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity instanceof ChromeActivity) {
            return ((ChromeActivity) activity)
                    .getWindowAndroid()
                    .hasPermission(permission.WRITE_EXTERNAL_STORAGE);
        }
        return false;
    }

    @CalledByNative
    private void requestFileAccess(final long callbackId) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeActivity)) {
            nativeOnAcquirePermissionResult(callbackId, false, null);
            return;
        }

        final WindowAndroid windowAndroid = ((ChromeActivity) activity).getWindowAndroid();
        if (windowAndroid == null) {
            nativeOnAcquirePermissionResult(callbackId, false, null);
            return;
        }

        if (!windowAndroid.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            nativeOnAcquirePermissionResult(callbackId, false,
                    windowAndroid.isPermissionRevokedByPolicy(permission.WRITE_EXTERNAL_STORAGE)
                            ? null
                            : permission.WRITE_EXTERNAL_STORAGE);
            return;
        }

        View view = activity.getLayoutInflater().inflate(R.layout.update_permissions_dialog, null);
        TextView dialogText = (TextView) view.findViewById(R.id.text);
        dialogText.setText(R.string.missing_storage_permission_download_education_text);

        final PermissionCallback permissionCallback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                nativeOnAcquirePermissionResult(callbackId,
                        grantResults.length > 0
                                && grantResults[0] == PackageManager.PERMISSION_GRANTED,
                        null);
            }
        };

        AlertDialog.Builder builder =
                new AlertDialog.Builder(activity, R.style.AlertDialogTheme)
                        .setView(view)
                        .setPositiveButton(R.string.infobar_update_permissions_button_text,
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int id) {
                                        windowAndroid.requestPermissions(
                                                new String[] {permission.WRITE_EXTERNAL_STORAGE},
                                                permissionCallback);
                                    }
                                })
                        .setOnCancelListener(new DialogInterface.OnCancelListener() {
                            @Override
                            public void onCancel(DialogInterface dialog) {
                                nativeOnAcquirePermissionResult(callbackId, false, null);
                            }
                        });
        builder.create().show();
    }

    /**
     * Called when a download is started.
     */
    @CalledByNative
    private void onDownloadStarted() {
        DownloadUtils.showDownloadStartToast(ContextUtils.getApplicationContext());
    }

    /**
     * Close a tab if it is blank. Returns true if it is or already closed.
     * @param Tab Tab to close.
     * @return true iff the tab was (already) closed.
     */
    @CalledByNative
    static boolean closeTabIfBlank(Tab tab) {
        if (tab == null) return true;
        WebContents contents = tab.getWebContents();
        boolean isInitialNavigation = contents == null
                || contents.getNavigationController().isInitialNavigation();
        if (isInitialNavigation) {
            // Tab is created just for download, close it.
            TabModelSelector selector = tab.getTabModelSelector();
            if (selector == null) return true;
            if (selector.getModel(tab.isIncognito()).getCount() == 1) return false;
            boolean closed = selector.closeTab(tab);
            assert closed;
            return true;
        }
        return false;
    }

    // native methods
    private native void nativeInit();
    private native void nativeOnAcquirePermissionResult(
            long callbackId, boolean granted, String permissionToUpdate);
}
