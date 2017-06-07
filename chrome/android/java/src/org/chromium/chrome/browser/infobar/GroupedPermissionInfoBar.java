// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.tab.Tab;

/**
 * An infobar for showing several permission requests which can be allowed or blocked.
 */
public class GroupedPermissionInfoBar extends PermissionInfoBar {
    private final String[] mPermissionText;
    private final int[] mPermissionIcons;

    GroupedPermissionInfoBar(Tab tab, int[] contentSettingsTypes, String message, String linkText,
            String buttonOk, String buttonCancel, boolean showPersistenceToggle,
            String[] permissionText, int[] permissionIcons) {
        super(tab, contentSettingsTypes, 0, null, message, linkText, buttonOk, buttonCancel,
                showPersistenceToggle);
        mPermissionText = permissionText;
        mPermissionIcons = permissionIcons;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        InfoBarControlLayout control = layout.addControlLayout();

        for (int i = 0; i < mPermissionIcons.length; i++) {
            control.addIcon(ResourceId.mapToDrawableId(mPermissionIcons[0]),
                    R.color.light_normal_color, mPermissionText[i], null);
        }

        // TODO(raymes): Ensure the ALLOW/BLOCK buttons are shown.

        // Call this last to ensure that if a persistence toggle is added, it's added last.
        super.createContent(layout);
    }

    @Override
    @CalledByNative
    protected boolean isPersistSwitchOn() {
        return super.isPersistSwitchOn();
    }

    /**
     * Create an infobar for a given set of permission requests.
     *
     * @param tab                   The tab which owns the infobar.
     * @param message               Message to display at the top of the infobar.
     * @param buttonOk              String to display on the 'Allow' button.
     * @param buttonCancel          String to display on the 'Block' button.
     * @param permissionIcons       Enumerated ID (from ResourceMapper) of an icon to display next
     *                              to each permission.
     * @param permissionText        String to display for each permission request.
     * @param contentSettingsTypes  The list of ContentSettingsTypes requested by the infobar.
     * @param showPersistenceToggle Whether or not a toggle to opt-out of persisting a decision
     *                              should be displayed.
     */
    @CalledByNative
    private static InfoBar create(Tab tab, int[] contentSettingsTypes, String message,
            String linkText, String buttonOk, String buttonCancel, boolean showPersistenceToggle,
            String[] permissionText, int[] permissionIcons) {
        GroupedPermissionInfoBar infobar =
                new GroupedPermissionInfoBar(tab, contentSettingsTypes, message, linkText, buttonOk,
                        buttonCancel, showPersistenceToggle, permissionText, permissionIcons);
        return infobar;
    }
}
