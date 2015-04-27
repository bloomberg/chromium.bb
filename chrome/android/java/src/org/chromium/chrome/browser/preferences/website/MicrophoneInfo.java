// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

/**
 * Microphone information for a given origin.
 */
public class MicrophoneInfo extends PermissionInfo {
    public MicrophoneInfo(String origin, String embedder) {
        super(origin, embedder);
    }

    protected int getNativePreferenceValue(String origin, String embedder) {
        return WebsitePreferenceBridge.nativeGetMicrophoneSettingForOrigin(
                origin, embedder);
    }

    protected void setNativePreferenceValue(
            String origin, String embedder, int value) {
        WebsitePreferenceBridge.nativeSetMicrophoneSettingForOrigin(
                origin, embedder, value);
    }
}
