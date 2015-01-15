// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

/**
 * Push Notification information for a given origin.
 */
public class PushNotificationInfo extends PermissionInfo {
    public PushNotificationInfo(String origin, String embedder) {
        super(origin, embedder);
    }

    @Override
    protected int getNativePreferenceValue(String origin, String embedder) {
        return WebsitePreferenceBridge.nativeGetPushNotificationSettingForOrigin(
                origin, embedder);
    }

    @Override
    protected void setNativePreferenceValue(
            String origin, String embedder, int value) {
        WebsitePreferenceBridge.nativeSetPushNotificationSettingForOrigin(
                origin, embedder, value);
    }
}
