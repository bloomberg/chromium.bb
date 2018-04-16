// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.StrictMode;
import android.preference.PreferenceFragment;
import android.support.annotation.XmlRes;
import android.support.v7.widget.ActionMenuView;
import android.view.View;
import android.view.ViewGroup;

import javax.annotation.Nullable;

/**
 * A helper class for Preferences.
 */
public class PreferenceUtils {
    /**
     * A helper that is used to load preferences from XML resources without causing a
     * StrictModeViolation. See http://crbug.com/692125.
     *
     * @param preferenceFragment A PreferenceFragment.
     * @param preferencesResId   The id of the XML resource to add to the PreferenceFragment.
     */
    public static void addPreferencesFromResource(
            PreferenceFragment preferenceFragment, @XmlRes int preferencesResId) {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            preferenceFragment.addPreferencesFromResource(preferencesResId);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * A helper that is used to set the visibility of the overflow menu view in a given activity.
     *
     * @param activity The Activity containing the action bar with the menu.
     * @param visibility The new visibility of the overflow menu view.
     * @return True if the visibility could be set, false otherwise (e.g. because no menu exists).
     */
    public static boolean setOverflowMenuVisibility(@Nullable Activity activity, int visibility) {
        if (activity == null) return false;
        ViewGroup actionBar = activity.findViewById(org.chromium.chrome.R.id.action_bar);
        int i = actionBar.getChildCount();
        ActionMenuView menuView = null;
        while (i-- > 0) {
            if (actionBar.getChildAt(i) instanceof ActionMenuView) {
                menuView = (ActionMenuView) actionBar.getChildAt(i);
                break;
            }
        }
        if (menuView == null) return false;
        View overflowButton = menuView.getChildAt(menuView.getChildCount() - 1);
        if (overflowButton == null) return false;
        overflowButton.setVisibility(visibility);
        return true;
    }

    /**
     * Convert a given icon to a plain white version by applying the MATRIX_TRANSFORM_TO_WHITE color
     * filter. The resulting drawable will be brighter than a usual grayscale conversion.
     *
     * For grayscale conversion, use the function ColorMatrix#setSaturation(0) instead.
     * @param icon The drawable to be converted.
     * @return Returns the bright white version of the passed drawable.
     */
    public static Drawable convertToPlainWhite(Drawable icon) {
        icon.mutate().setColorFilter(Color.WHITE, PorterDuff.Mode.SRC_ATOP);
        return icon;
    }
}
