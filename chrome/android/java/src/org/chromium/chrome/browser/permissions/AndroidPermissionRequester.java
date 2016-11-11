// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.app.Activity;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.support.v7.app.AlertDialog;
import android.util.SparseArray;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.PermissionCallback;

/**
 * An object to handle requesting native permissions from Android when the user grants a website
 * a permission.
 */
public class AndroidPermissionRequester {

    private WindowAndroid mWindowAndroid;
    private RequestDelegate mDelegate;

    /**
     * Mapping between the required {@link ContentSettingsType}s and their associated Android
     * runtime permissions.  Only {@link ContentSettingsType}s that are associated with runtime
     * permissions will be included in this list while all others will be excluded.
     */
    private SparseArray<String> mContentSettingsToPermissionsMap;

    /**
    * An interface for classes which need to be informed of the outcome of asking a user to grant an
    * Android permission.
    */
    public interface RequestDelegate {
        void onAndroidPermissionAccepted();
        void onAndroidPermissionCanceled();
    }

    public AndroidPermissionRequester(WindowAndroid windowAndroid,
            RequestDelegate delegate, int[] contentSettings) {
        mWindowAndroid = windowAndroid;
        mDelegate = delegate;
        mContentSettingsToPermissionsMap = generatePermissionsMapping(contentSettings);
    }

    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    private SparseArray<String> generatePermissionsMapping(int[] contentSettings) {
        SparseArray<String> permissionsToRequest = new SparseArray<String>();
        for (int i = 0; i < contentSettings.length; i++) {
            String permission = PrefServiceBridge.getAndroidPermissionForContentSetting(
                    contentSettings[i]);
            if (permission != null && !mWindowAndroid.hasPermission(permission)) {
                permissionsToRequest.append(contentSettings[i], permission);
            }
        }
        return permissionsToRequest;
    }

    private int getDeniedPermissionResourceId(String permission) {
        int contentSettingsType = 0;
        // SparseArray#indexOfValue uses == instead of .equals, so we need to manually iterate
        // over the list.
        for (int i = 0; i < mContentSettingsToPermissionsMap.size(); i++) {
            if (permission.equals(mContentSettingsToPermissionsMap.valueAt(i))) {
                contentSettingsType = mContentSettingsToPermissionsMap.keyAt(i);
            }
        }

        if (contentSettingsType == ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION) {
            return R.string.infobar_missing_location_permission_text;
        }
        if (contentSettingsType == ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
            return R.string.infobar_missing_microphone_permission_text;
        }
        if (contentSettingsType == ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
            return R.string.infobar_missing_camera_permission_text;
        }
        assert false : "Unexpected content setting type received: " + contentSettingsType;
        return R.string.infobar_missing_multiple_permissions_text;
    }

    public boolean shouldSkipPermissionRequest() {
        return (mWindowAndroid == null || mContentSettingsToPermissionsMap == null
                || mContentSettingsToPermissionsMap.size() == 0);
    }

    public void requestAndroidPermissions() {
        PermissionCallback callback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(
                    String[] permissions, int[] grantResults) {
                int deniedCount = 0;
                int requestableCount = 0;
                int deniedStringId = R.string.infobar_missing_multiple_permissions_text;
                for (int i = 0; i < grantResults.length; i++) {
                    if (grantResults[i] == PackageManager.PERMISSION_DENIED) {
                        deniedCount++;
                        if (deniedCount > 1) {
                            deniedStringId = R.string.infobar_missing_multiple_permissions_text;
                        } else {
                            deniedStringId = getDeniedPermissionResourceId(permissions[i]);
                        }

                        if (mWindowAndroid.canRequestPermission(permissions[i])) {
                            requestableCount++;
                        }
                    }
                }

                Activity activity = mWindowAndroid.getActivity().get();
                if (deniedCount > 0 && requestableCount > 0 && activity != null) {
                    View view = activity.getLayoutInflater().inflate(
                            R.layout.update_permissions_dialog, null);
                    TextView dialogText = (TextView) view.findViewById(R.id.text);
                    dialogText.setText(deniedStringId);

                    AlertDialog.Builder builder =
                            new AlertDialog.Builder(activity, R.style.AlertDialogTheme)
                            .setView(view)
                            .setPositiveButton(R.string.infobar_update_permissions_button_text,
                                    new DialogInterface.OnClickListener() {
                                        @Override
                                        public void onClick(DialogInterface dialog, int id) {
                                            requestAndroidPermissions();
                                        }
                                    })
                             .setOnCancelListener(new DialogInterface.OnCancelListener() {
                                     @Override
                                     public void onCancel(DialogInterface dialog) {
                                         mDelegate.onAndroidPermissionCanceled();
                                     }
                             });
                    builder.create().show();
                } else if (deniedCount > 0) {
                    mDelegate.onAndroidPermissionCanceled();
                } else {
                    mDelegate.onAndroidPermissionAccepted();
                }
            }
        };

        String[] permissionsToRequest = new String[mContentSettingsToPermissionsMap.size()];
        for (int i = 0; i < mContentSettingsToPermissionsMap.size(); i++) {
            permissionsToRequest[i] = mContentSettingsToPermissionsMap.valueAt(i);
        }
        mWindowAndroid.requestPermissions(permissionsToRequest, callback);
    }

}
