// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableIntPropertyKey;

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

    /** Specifies status separator color. */
    public static final WritableIntPropertyKey SEPARATOR_COLOR_RES = new WritableIntPropertyKey();

    /** Specifies current selection of the location bar button type. */
    public static final WritableIntPropertyKey STATUS_BUTTON_TYPE = new WritableIntPropertyKey();

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
            ICON_TINT_COLOR_RES, NAVIGATION_BUTTON_TYPE, SEPARATOR_COLOR_RES, STATUS_BUTTON_TYPE,
            VERBOSE_STATUS_TEXT_COLOR_RES, VERBOSE_STATUS_TEXT_STRING_RES,
            VERBOSE_STATUS_TEXT_VISIBLE, VERBOSE_STATUS_TEXT_WIDTH};

    private StatusProperties() {}
}
