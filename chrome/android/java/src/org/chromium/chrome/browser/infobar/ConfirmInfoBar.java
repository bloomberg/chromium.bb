// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Process;
import android.support.v7.app.AlertDialog;
import android.util.SparseArray;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.PermissionCallback;

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

    /** Text shown on the link, e.g. "Learn more". */
    private final String mLinkText;

    private WindowAndroid mWindowAndroid;

    /**
     * Mapping between the required {@link ContentSettingsType}s and their associated Android
     * runtime permissions.  Only {@link ContentSettingsType}s that are associated with runtime
     * permissions will be included in this list while all others will be excluded.
     */
    private SparseArray<String> mContentSettingsToPermissionsMap;

    protected ConfirmInfoBar(int iconDrawableId, Bitmap iconBitmap, String message,
            String linkText, String primaryButtonText, String secondaryButtonText) {
        super(iconDrawableId, iconBitmap, message);
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
        mLinkText = linkText;
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
        assert windowAndroid != null
                : "A WindowAndroid must be specified to request access to content settings";

        mContentSettingsToPermissionsMap = generatePermissionsMapping(contentSettings);
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        setButtons(layout, mPrimaryButtonText, mSecondaryButtonText);
        if (mLinkText != null) layout.setMessageLinkText(mLinkText);
    }

    /**
     * If your custom infobar overrides this function, YOU'RE PROBABLY DOING SOMETHING WRONG.
     *
     * Adds buttons to the infobar.  This should only be overridden in cases where an infobar
     * requires adding something other than a button for its secondary View on the bottom row
     * (almost never).
     *
     * @param primaryText Text to display on the primary button.
     * @param secondaryText Text to display on the secondary button.  May be null.
     */
    protected void setButtons(InfoBarLayout layout, String primaryText, String secondaryText) {
        layout.setButtons(primaryText, secondaryText);
    }

    private static boolean hasPermission(Context context, String permission) {
        return context.checkPermission(permission, Process.myPid(), Process.myUid())
                != PackageManager.PERMISSION_DENIED;
    }

    private SparseArray<String> generatePermissionsMapping(int[] contentSettings) {
        Context context = mWindowAndroid.getApplicationContext();
        SparseArray<String> permissionsToRequest = new SparseArray<String>();
        for (int i = 0; i < contentSettings.length; i++) {
            String permission = PrefServiceBridge.getAndroidPermissionForContentSetting(
                    contentSettings[i]);
            if (permission != null) {
                if (!hasPermission(context, permission)) {
                    permissionsToRequest.append(contentSettings[i], permission);
                }
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

    @Override
    public void onButtonClicked(final boolean isPrimaryButton) {
        if (mWindowAndroid == null || !isPrimaryButton || getContext() == null
                || mContentSettingsToPermissionsMap == null
                || mContentSettingsToPermissionsMap.size() == 0) {
            onButtonClickedInternal(isPrimaryButton);
            return;
        }

        requestAndroidPermissions();
    }

    private void requestAndroidPermissions() {
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
                                         onCloseButtonClicked();
                                     }
                             });
                    builder.create().show();
                } else if (deniedCount > 0) {
                    onCloseButtonClicked();
                } else {
                    onButtonClickedInternal(true);
                }
            }
        };

        String[] permissionsToRequest = new String[mContentSettingsToPermissionsMap.size()];
        for (int i = 0; i < mContentSettingsToPermissionsMap.size(); i++) {
            permissionsToRequest[i] = mContentSettingsToPermissionsMap.valueAt(i);
        }
        mWindowAndroid.requestPermissions(permissionsToRequest, callback);
    }

    private void onButtonClickedInternal(boolean isPrimaryButton) {
        int action = isPrimaryButton ? ActionType.OK : ActionType.CANCEL;
        onButtonClicked(action);
    }

    /**
     * Creates and begins the process for showing a ConfirmInfoBar.
     * @param windowAndroid The owning window for the infobar.
     * @param enumeratedIconId ID corresponding to the icon that will be shown for the infobar.
     *                         The ID must have been mapped using the ResourceMapper class before
     *                         passing it to this function.
     * @param iconBitmap Bitmap to use if there is no equivalent Java resource for
     *                   enumeratedIconId.
     * @param message Message to display to the user indicating what the infobar is for.
     * @param linkText Link text to display in addition to the message.
     * @param buttonOk String to display on the OK button.
     * @param buttonCancel String to display on the Cancel button.
     * @param contentSettings The list of ContentSettingTypes being requested by this infobar.
     */
    @CalledByNative
    private static ConfirmInfoBar create(WindowAndroid windowAndroid, int enumeratedIconId,
            Bitmap iconBitmap, String message, String linkText, String buttonOk,
            String buttonCancel, int[] contentSettings) {
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);

        ConfirmInfoBar infoBar = new ConfirmInfoBar(
                drawableId, iconBitmap, message, linkText, buttonOk, buttonCancel);
        infoBar.setContentSettings(windowAndroid, contentSettings);

        return infoBar;
    }
}
