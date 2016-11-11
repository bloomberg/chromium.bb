// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Bitmap;
import android.support.v7.widget.SwitchCompat;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.permissions.AndroidPermissionRequester;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

/**
 * An infobar used for prompting the user to grant a web API permission.
 *
 */
public class PermissionInfoBar
        extends ConfirmInfoBar implements AndroidPermissionRequester.RequestDelegate {

    /** Whether or not to show a toggle for opting out of persisting the decision. */
    private boolean mShowPersistenceToggle;

    private AndroidPermissionRequester mRequester;

    protected PermissionInfoBar(int iconDrawableId, Bitmap iconBitmap, String message,
            String linkText, String primaryButtonText, String secondaryButtonText,
            boolean showPersistenceToggle) {
        super(iconDrawableId, iconBitmap, message, linkText, primaryButtonText,
                secondaryButtonText);
        mShowPersistenceToggle = showPersistenceToggle;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);

        if (mShowPersistenceToggle) {
            InfoBarControlLayout controlLayout = layout.addControlLayout();
            String description =
                    layout.getContext().getString(R.string.permission_prompt_persist_text);
            controlLayout.addSwitch(
                    0, 0, description, R.id.permission_infobar_persist_toggle, true);
        }
    }

    @Override
    public void onTabReparented(Tab tab) {
        if (mRequester != null) {
            mRequester.setWindowAndroid(tab.getWindowAndroid());
        }
    }

    @Override
    public void onAndroidPermissionAccepted() {
        onButtonClickedInternal(true);
    }

    @Override
    public void onAndroidPermissionCanceled() {
        onCloseButtonClicked();
    }

    /**
     * Specifies the {@link ContentSettingsType}s that are controlled by this InfoBar.
     *
     * @param windowAndroid The WindowAndroid that will be used to check for the necessary
     *                      permissions.
     * @param contentSettings The list of {@link ContentSettingsType}s whose access is guarded
     *                        by this InfoBar.
     */
    protected void setContentSettings(WindowAndroid windowAndroid, int[] contentSettings) {
        assert windowAndroid != null
                : "A WindowAndroid must be specified to request access to content settings";

        mRequester = new AndroidPermissionRequester(windowAndroid, this, contentSettings);
    }

    @Override
    public void onButtonClicked(final boolean isPrimaryButton) {
        if (!isPrimaryButton || getContext() == null || mRequester.shouldSkipPermissionRequest()) {
            onButtonClickedInternal(isPrimaryButton);
            return;
        }

        mRequester.requestAndroidPermissions();
    }

    private void onButtonClickedInternal(boolean isPrimaryButton) {
        super.onButtonClicked(isPrimaryButton);
    }

    /**
     * Returns true if the persist switch exists and is toggled on.
     */
    @CalledByNative
    protected boolean isPersistSwitchOn() {
        SwitchCompat persistSwitch = (SwitchCompat) getView().findViewById(
                R.id.permission_infobar_persist_toggle);
        if (mShowPersistenceToggle && persistSwitch != null) {
            return persistSwitch.isChecked();
        }
        return false;
    }

    /**
     * Creates and begins the process for showing a PermissionInfoBar.
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
     * @param showPersistenceToggle Whether or not a toggle to opt-out of persisting a decision
     *                              should be displayed.
     */
    @CalledByNative
    private static PermissionInfoBar create(WindowAndroid windowAndroid, int enumeratedIconId,
            Bitmap iconBitmap, String message, String linkText, String buttonOk,
            String buttonCancel, int[] contentSettings, boolean showPersistenceToggle) {
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);

        PermissionInfoBar infoBar = new PermissionInfoBar(drawableId, iconBitmap, message, linkText,
                buttonOk, buttonCancel, showPersistenceToggle);
        infoBar.setContentSettings(windowAndroid, contentSettings);

        return infoBar;
    }
}
