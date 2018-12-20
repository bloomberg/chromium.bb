// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.view.View;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Model for the Status view.
 */
class StatusProperties {
    /** Enables / disables animations. */
    public static final WritableBooleanPropertyKey ANIMATIONS_ENABLED =
            new WritableBooleanPropertyKey();

    /** Specifies status icon tint color. */
    public static final WritableIntPropertyKey ICON_TINT_COLOR_RES = new WritableIntPropertyKey();

    /** Specifies navigation button type (eg: PAGE, MAGNIFIER) */
    public static final WritableIntPropertyKey NAVIGATION_BUTTON_TYPE =
            new WritableIntPropertyKey();

    /** Specifies string resource holding content description for security icon. */
    public static final WritableIntPropertyKey SECURITY_ICON_DESCRIPTION_RES =
            new WritableIntPropertyKey();

    /** Specifies resource displayed by security chip. */
    public static final WritableIntPropertyKey SECURITY_ICON_RES = new WritableIntPropertyKey();

    /** Specifies color tint list for icon displayed by security chip. */
    public static final WritableIntPropertyKey SECURITY_ICON_TINT_RES =
            new WritableIntPropertyKey();

    /** Specifies status separator color. */
    public static final WritableIntPropertyKey SEPARATOR_COLOR_RES = new WritableIntPropertyKey();

    /** Specifies current selection of the location bar button type. */
    public static final WritableIntPropertyKey STATUS_BUTTON_TYPE = new WritableIntPropertyKey();

    /** Specifies object to receive status click events. */
    public static final WritableObjectPropertyKey<View.OnClickListener> STATUS_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Specifies verbose status text color. */
    public static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_COLOR_RES =
            new WritableIntPropertyKey();

    /** Specifies content of the verbose status text field. */
    public static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_STRING_RES =
            new WritableIntPropertyKey();

    /** Specifies whether verbose status text view is visible. */
    public static final WritableBooleanPropertyKey VERBOSE_STATUS_TEXT_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Specifies width of the verbose status text view. */
    public static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_WIDTH =
            new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ANIMATIONS_ENABLED,
            ICON_TINT_COLOR_RES, NAVIGATION_BUTTON_TYPE, SECURITY_ICON_RES,
            SECURITY_ICON_DESCRIPTION_RES, SECURITY_ICON_TINT_RES, SEPARATOR_COLOR_RES,
            STATUS_BUTTON_TYPE, STATUS_CLICK_LISTENER, VERBOSE_STATUS_TEXT_COLOR_RES,
            VERBOSE_STATUS_TEXT_STRING_RES, VERBOSE_STATUS_TEXT_VISIBLE, VERBOSE_STATUS_TEXT_WIDTH};

    private StatusProperties() {}
}
