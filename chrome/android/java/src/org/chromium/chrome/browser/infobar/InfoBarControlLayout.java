// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Switch;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * Lays out controls for InfoBars that require more than the standard pair of OK/CANCEL buttons.
 *
 * This class works with the {@link InfoBarLayout} to define a standard set of controls with
 * standardized spacings and text styling that gets laid out in grid form: https://crbug.com/543205
 *
 * TODO(dfalcantara): Implement the measurement and layout algorithms.
 */
public class InfoBarControlLayout extends FrameLayout {

    public InfoBarControlLayout(Context context) {
        super(context);
    }

    /**
     * Adds a toggle switch to the layout.
     *
     * -------------------------------------------------
     * | ICON | MESSAGE                       | TOGGLE |
     * -------------------------------------------------
     * If an icon is not provided, the ImageView that would normally show it is hidden.
     *
     * @param iconResourceId ID of the drawable to use for the icon, or 0 to hide the ImageView.
     * @param toggleMessage  Message to display for the toggle.
     * @param toggleId       ID to use for the toggle.
     * @param isChecked      Whether the toggle should start off checked.
     */
    public LinearLayout addSwitch(
            int iconResourceId, CharSequence toggleMessage, int toggleId, boolean isChecked) {
        LinearLayout switchLayout = (LinearLayout) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_toggle, this, false);
        addView(switchLayout);

        ImageView iconView = (ImageView) switchLayout.findViewById(R.id.control_toggle_icon);
        if (iconResourceId == 0) {
            removeView(iconView);
        } else {
            iconView.setImageResource(iconResourceId);
        }

        TextView messageView = (TextView) switchLayout.findViewById(R.id.control_toggle_message);
        messageView.setText(toggleMessage);

        Switch switchView = (Switch) switchLayout.findViewById(R.id.control_toggle_switch);
        switchView.setId(toggleId);
        switchView.setChecked(isChecked);

        return switchLayout;
    }
}
