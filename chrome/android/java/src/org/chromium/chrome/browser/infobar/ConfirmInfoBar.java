// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Process;

import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.PermissionCallback;

import java.util.ArrayList;
import java.util.List;

/**
 * An infobar that presents the user with several buttons.
 *
 * TODO(newt): merge this into InfoBar.java.
 */
public class ConfirmInfoBar extends InfoBar {
    /** Text shown on the primary button, e.g. "OK". */
    private final String mPrimaryButtonText;

    /** Text shown on the secondary button, e.g. "Cancel".*/
    private final String mSecondaryButtonText;

    /** Text shown on the extra button, e.g. "More info". */
    private final String mTertiaryButtonText;

    /** Notified when one of the buttons is clicked. */
    private final InfoBarListeners.Confirm mConfirmListener;

    private WindowAndroid mWindowAndroid;

    /**
     * The list of {@link ContentSettingsType}s being requested by this infobar.  Can be null or
     * empty if none apply.
     */
    private int[] mContentSettings;

    public ConfirmInfoBar(InfoBarListeners.Confirm confirmListener, int iconDrawableId,
            Bitmap iconBitmap, String message, String linkText, String primaryButtonText,
            String secondaryButtonText) {
        super(confirmListener, iconDrawableId, iconBitmap, message);
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
        mTertiaryButtonText = linkText;
        mConfirmListener = confirmListener;
    }

    /**
     * Specifies the {@link ContentSettingsType}s that are controlled by this InfoBar.
     *
     * @param windowAndroid The WindowAndroid that will be used to check for the necessary
     *                      permissions.
     * @param contentSettings The list of {@link ContentSettingsType}s whose access is guarded
     *                        by this InfoBar.
     */
    protected void setContentSettings(
            WindowAndroid windowAndroid, int[] contentSettings) {
        mWindowAndroid = windowAndroid;
        mContentSettings = contentSettings;

        assert windowAndroid != null
                : "A WindowAndroid must be specified to request access to content settings";
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        layout.setButtons(mPrimaryButtonText, mSecondaryButtonText, mTertiaryButtonText);
    }

    private static boolean hasPermission(Context context, String permission) {
        return context.checkPermission(permission, Process.myPid(), Process.myUid())
                != PackageManager.PERMISSION_DENIED;
    }

    private List<String> getPermissionsToRequest() {
        Context context = getContext();
        List<String> permissionsToRequest = new ArrayList<String>();
        for (int i = 0; i < mContentSettings.length; i++) {
            switch (mContentSettings[i]) {
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION:
                    if (!hasPermission(context, Manifest.permission.ACCESS_FINE_LOCATION)) {
                        permissionsToRequest.add(Manifest.permission.ACCESS_FINE_LOCATION);
                    }
                    break;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
                    if (!hasPermission(context, Manifest.permission.CAMERA)) {
                        permissionsToRequest.add(Manifest.permission.CAMERA);
                    }
                    break;
                case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
                    if (!hasPermission(context, Manifest.permission.RECORD_AUDIO)) {
                        permissionsToRequest.add(Manifest.permission.RECORD_AUDIO);
                    }
                    break;
                default:
                    // No associated Android permission, so just skip it.
                    break;
            }
        }
        return permissionsToRequest;
    }

    @Override
    public void onButtonClicked(final boolean isPrimaryButton) {
        if (mWindowAndroid == null || mContentSettings == null
                || !isPrimaryButton || getContext() == null) {
            onButtonClickedInternal(isPrimaryButton);
            return;
        }

        List<String> permissionsToRequest = getPermissionsToRequest();
        if (permissionsToRequest.isEmpty()) {
            onButtonClickedInternal(isPrimaryButton);
            return;
        }

        PermissionCallback callback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(
                    String[] permissions, int[] grantResults) {
                boolean grantedAllPermissions = true;
                for (int i = 0; i < grantResults.length; i++) {
                    if (grantResults[i] == PackageManager.PERMISSION_DENIED) {
                        grantedAllPermissions = false;
                        break;
                    }
                }

                if (!grantedAllPermissions) {
                    onCloseButtonClicked();
                } else {
                    onButtonClickedInternal(true);
                }
            }
        };

        mWindowAndroid.requestPermissions(
                permissionsToRequest.toArray(new String[permissionsToRequest.size()]),
                callback);
    }

    private void onButtonClickedInternal(boolean isPrimaryButton) {
        if (mConfirmListener != null) {
            mConfirmListener.onConfirmInfoBarButtonClicked(this, isPrimaryButton);
        }

        int action = isPrimaryButton ? InfoBar.ACTION_TYPE_OK : InfoBar.ACTION_TYPE_CANCEL;
        onButtonClicked(action, "");
    }
}
