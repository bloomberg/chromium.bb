// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.WebContents;

import java.io.File;

import javax.annotation.Nullable;

/**
 * Helper class to handle communication between download location dialog and native.
 */
public class DownloadLocationDialogBridge
        implements DownloadLocationDialog.DownloadLocationDialogListener {
    // TODO(jming): Remove this when switching to a dropdown instead of going to preferences.
    private static DownloadLocationDialogBridge sInstance;

    private long mNativeDownloadLocationDialogBridge;
    private DownloadLocationDialog mLocationDialog;

    @Nullable
    public static DownloadLocationDialogBridge getInstance() {
        return sInstance;
    }

    private DownloadLocationDialogBridge(long nativeDownloadLocationDialogBridge) {
        mNativeDownloadLocationDialogBridge = nativeDownloadLocationDialogBridge;
    }

    /**
     * Update the file location that is displayed on the alert dialog.
     *
     * @param newLocation Where the user wants to download the file.
     */
    public void updateFileLocation(File newLocation) {
        if (mLocationDialog == null) return;
        mLocationDialog.setFileLocation(newLocation);
    }

    @CalledByNative
    public static DownloadLocationDialogBridge create(long nativeDownloadLocationDialogBridge) {
        sInstance = new DownloadLocationDialogBridge(nativeDownloadLocationDialogBridge);
        return sInstance;
    }

    @CalledByNative
    private void destroy() {
        mNativeDownloadLocationDialogBridge = 0;
        if (mLocationDialog != null) mLocationDialog.dismiss();
        sInstance = null;
    }

    @CalledByNative
    public void showDialog(WebContents webContents, String suggestedPath) {
        Activity windowAndroidActivity = webContents.getTopLevelNativeWindow().getActivity().get();
        if (windowAndroidActivity == null) return;

        if (mLocationDialog != null) return;
        mLocationDialog =
                new DownloadLocationDialog(windowAndroidActivity, this, new File(suggestedPath));
        // TODO(jming): Use ModalDialogManager.
        mLocationDialog.show();
    }

    @Override
    public void onComplete(File returnedPath) {
        if (mNativeDownloadLocationDialogBridge == 0) return;

        nativeOnComplete(mNativeDownloadLocationDialogBridge, returnedPath.getAbsolutePath());
        mLocationDialog = null;
    }

    @Override
    public void onCanceled() {
        if (mNativeDownloadLocationDialogBridge == 0) return;

        nativeOnCanceled(mNativeDownloadLocationDialogBridge);
        mLocationDialog = null;
    }

    public native void nativeOnComplete(
            long nativeDownloadLocationDialogBridge, String returnedPath);
    public native void nativeOnCanceled(long nativeDownloadLocationDialogBridge);
}
