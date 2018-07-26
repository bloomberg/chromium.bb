// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.widget.TintedDrawable;
import org.chromium.chrome.browser.widget.TintedImageButton;

/**
 * This class can hold two buttons, one to be shown in browsing mode and one for tab switching mode.
 */
class ToolbarButtonSlotData {
    /** The button to be shown when in browsing mode. */
    public final ToolbarButtonData browsingModeButtonData;

    /** The button to be shown when in tab switcher mode. */
    public final ToolbarButtonData tabSwitcherModeButtonData;

    /**
     * @param browsingModeButton The button to be shown when in browsing mode.
     * @param tabSwitcherModeButton The button to be shown when in tab switcher mode.
     */
    ToolbarButtonSlotData(
            ToolbarButtonData browsingModeButton, ToolbarButtonData tabSwitcherModeButton) {
        browsingModeButtonData = browsingModeButton;
        tabSwitcherModeButtonData = tabSwitcherModeButton;
    }

    /**
     * A class that holds all the state for a bottom toolbar button. It is used to swap between
     * buttons when entering or leaving tab switching mode.
     */
    static class ToolbarButtonData {
        private final TintedDrawable mTintedDrawable;
        private final CharSequence mAccessibilityStringResId;
        private final OnClickListener mOnClickListener;

        /**
         * @param tintedDrawableResId The drawable's resource id.
         * @param accessibilityStringResId The accessibility's resource id.
         * @param onClickListener An {@link OnClickListener} that is triggered when this button is
         *                        clicked.
         * @param context The {@link Context} used to get the drawable and accessibility string
         *                resources.
         */
        ToolbarButtonData(int tintedDrawableResId, int accessibilityStringResId,
                OnClickListener onClickListener, Context context) {
            mTintedDrawable = TintedDrawable.constructTintedDrawable(context, tintedDrawableResId);
            mAccessibilityStringResId = context.getString(accessibilityStringResId);
            mOnClickListener = onClickListener;
        }

        /**
         * @param imageButton The {@link TintedImageButton} this button data will fill.
         */
        void updateButton(TintedImageButton imageButton) {
            imageButton.setOnClickListener(mOnClickListener);
            imageButton.setImageDrawable(mTintedDrawable);
            imageButton.setContentDescription(mAccessibilityStringResId);
        }
    }
}
