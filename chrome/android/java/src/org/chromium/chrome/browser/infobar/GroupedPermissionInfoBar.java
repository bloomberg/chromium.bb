// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.ui.base.WindowAndroid;

/**
 * An infobar for showing several permission requests which can be allowed or blocked.
 */
public class GroupedPermissionInfoBar extends ConfirmInfoBar {
    private final int[] mPermissionIcons;
    private final String[] mPermissionText;

    GroupedPermissionInfoBar(String message, String buttonOk, String buttonCancel,
            int[] permissionIcons, String[] permissionText) {
        super(0, null, message, null, buttonOk, buttonCancel);
        mPermissionIcons = permissionIcons;
        mPermissionText = permissionText;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        InfoBarControlLayout control = layout.addControlLayout();

        for (int i = 0; i < mPermissionIcons.length; i++) {
            control.addIcon(ResourceId.mapToDrawableId(mPermissionIcons[i]), mPermissionText[i],
                    null, R.color.light_normal_color);
        }
    }

    /**
     * Create an infobar for a given set of permission requests.
     *
     * @param message Message to display at the top of the infobar.
     * @param buttonOk String to display on the 'Allow' button.
     * @param buttonCancel String to display on the 'Block' button.
     * @param permissionIcons Enumerated ID (from ResourceMapper) of an icon to display next to each
     *                        permission.
     * @param permissionText String to display for each permission request.
     * @param windowAndroid The window which owns the infobar.
     * @param contentSettings The list of ContentSettingsTypes requested by the infobar.
     */
    @CalledByNative
    private static InfoBar create(String message, String buttonOk, String buttonCancel,
            int[] permissionIcons, String[] permissionText, WindowAndroid windowAndroid,
            int[] contentSettings) {
        GroupedPermissionInfoBar infobar = new GroupedPermissionInfoBar(
                message, buttonOk, buttonCancel, permissionIcons, permissionText);
        infobar.setContentSettings(windowAndroid, contentSettings);
        return infobar;
    }
}
