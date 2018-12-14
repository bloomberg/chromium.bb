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
    /** Specifies navigation button type (eg: PAGE, MAGNIFIER) */
    public static final WritableIntPropertyKey NAVIGATION_BUTTON_TYPE =
            new WritableIntPropertyKey();

    /** Specifies whether presented page is an offline page. */
    public static final WritableBooleanPropertyKey PAGE_IS_OFFLINE =
            new WritableBooleanPropertyKey();

    /** Specifies whether page is shown as a preview. */
    public static final WritableBooleanPropertyKey PAGE_IS_PREVIEW =
            new WritableBooleanPropertyKey();

    /** Specifies whether dark colors should be used in the view. */
    public static final WritableBooleanPropertyKey USE_DARK_COLORS =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            NAVIGATION_BUTTON_TYPE, PAGE_IS_OFFLINE, PAGE_IS_PREVIEW, USE_DARK_COLORS};

    private StatusProperties() {}
}
