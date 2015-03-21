// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

/**
 * Fullscreen information for a given origin.
 */
public class FullscreenInfo extends PermissionInfo {
    public FullscreenInfo(String origin, String embedder) {
        super(origin, embedder);
    }

    @Override
    protected int getNativePreferenceValue(String origin, String embedder) {
        return WebsitePreferenceBridge.nativeGetFullscreenSettingForOrigin(
                origin, embedder);
    }

    @Override
    protected void setNativePreferenceValue(
            String origin, String embedder, int value) {
        WebsitePreferenceBridge.nativeSetFullscreenSettingForOrigin(
                origin, embedder, value);
    }
}
