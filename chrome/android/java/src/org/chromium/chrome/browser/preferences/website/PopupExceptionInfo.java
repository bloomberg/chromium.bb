// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.chrome.browser.preferences.PrefServiceBridge;

import java.io.Serializable;

/**
 * Popup exception information for a given URL pattern.
 */
public class PopupExceptionInfo implements Serializable {
    private final String mPattern;
    private final String mSetting;

    public PopupExceptionInfo(String pattern, String setting) {
        mPattern = pattern;
        mSetting = setting;
    }

    public String getPattern() {
        return mPattern;
    }

    /**
     * @return The ContentSetting specifying whether popups are allowed for this pattern.
     */
    public ContentSetting getContentSetting() {
        if (mSetting.equals(PrefServiceBridge.EXCEPTION_SETTING_ALLOW)) {
            return ContentSetting.ALLOW;
        } else if (mSetting.equals(PrefServiceBridge.EXCEPTION_SETTING_BLOCK)) {
            return ContentSetting.BLOCK;
        } else {
            return null;
        }
    }

    /**
     * Sets whether popups are allowed for this pattern.
     */
    public void setContentSetting(ContentSetting value) {
        if (value != null) {
            PrefServiceBridge.getInstance().setPopupException(
                    mPattern, value == ContentSetting.ALLOW ? true : false);
        } else {
            PrefServiceBridge.getInstance().removePopupException(mPattern);
        }
    }
}