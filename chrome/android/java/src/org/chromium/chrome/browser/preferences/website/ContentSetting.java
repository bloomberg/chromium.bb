// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

/**
 * Java counterpart to C++ ContentSetting enum.
 *
 * @see ContentSettingValues
 */
public enum ContentSetting {
    DEFAULT(ContentSettingValues.DEFAULT),
    ALLOW(ContentSettingValues.ALLOW),
    BLOCK(ContentSettingValues.BLOCK),
    ASK(ContentSettingValues.ASK),
    SESSION_ONLY(ContentSettingValues.SESSION_ONLY),
    DETECT_IMPORTANT_CONTENT(ContentSettingValues.DETECT_IMPORTANT_CONTENT);

    private int mValue;

    /**
     * Converts the enum value to int.
     */
    public int toInt() {
        return mValue;
    }

    /**
     * Converts an int to its equivalent ContentSetting.
     * @param i The integer to convert.
     * @return What value the enum is representing (or null if failed).
     */
    public static ContentSetting fromInt(int i) {
        for (ContentSetting enumValue : ContentSetting.values()) {
            if (enumValue.toInt() == i) return enumValue;
        }
        return null;
    }

    private ContentSetting(int value) {
        this.mValue = value;
    }
}