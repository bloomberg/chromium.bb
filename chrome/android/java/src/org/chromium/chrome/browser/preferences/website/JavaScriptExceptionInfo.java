// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.chrome.browser.preferences.PrefServiceBridge;

import java.io.Serializable;

/**
 * JavaScript exception information for a given URL pattern.
 */
public class JavaScriptExceptionInfo implements Serializable {
    private final String mPattern;
    private final String mSetting;
    private final String mSource;

    public JavaScriptExceptionInfo(String pattern, String setting, String source) {
        mPattern = pattern;
        mSetting = setting;
        mSource = source;
    }

    public String getPattern() {
        return mPattern;
    }

    public String getSetting() {
        return mPattern;
    }

    public String getSource() {
        return mSource;
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
            PrefServiceBridge.getInstance().setJavaScriptAllowed(
                    mPattern, value == ContentSetting.ALLOW ? true : false);
        } else {
            PrefServiceBridge.getInstance().removeJavaScriptException(mPattern);
        }
    }
}