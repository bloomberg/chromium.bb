// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import java.io.Serializable;

/**
 * This class represents voice and video capture permission information for requesting
 * frame origin and embedding frame origin.
 */
public class VoiceAndVideoCaptureInfo implements Serializable {

    private final String mOrigin;
    private final String mEmbedder;

    /**
     * @param origin Requesting frame URL's origin for voice and video capture permission.
     * @param embedder Embedding (top frame) frame URL's origin for voice and video capture
     *                 permission.
     */
    public VoiceAndVideoCaptureInfo(String origin, String embedder) {
        mOrigin = origin;
        mEmbedder = embedder;
    }

    /**
     * @return Requesting frame URL's origin for voice and video capture permission.
     */
    public String getOrigin() {
        return mOrigin;
    }

    /**
     * @return embedding frame URL's origin for voice and video capture permission.
     */
    public String getEmbedder() {
        return mEmbedder;
    }

    /**
     * @return Embedding frame origin for this permission. It it's null, returns origin.
     */
    public String getEmbedderSafe() {
        return mEmbedder != null ? mEmbedder : mOrigin;
    }

    /**
     * @return Whether audio is being captured.
     */
    public ContentSetting getVoiceCapturePermission() {
        return ContentSetting.fromInt(
                WebsitePreferenceBridge.nativeGetVoiceCaptureSettingForOrigin(
                        mOrigin, getEmbedderSafe()));
    }

    /**
     * @return Whether video is being captured.
     */
    public ContentSetting getVideoCapturePermission() {
        return ContentSetting.fromInt(
                WebsitePreferenceBridge.nativeGetVideoCaptureSettingForOrigin(
                        mOrigin, getEmbedderSafe()));
    }

    /**
     * Sets voice capture permission.
     * @param value ALLOW to allow voice capture, BLOCK otherwise.
     */
    public void setVoiceCapturePermission(ContentSetting value) {
        WebsitePreferenceBridge.nativeSetVoiceCaptureSettingForOrigin(
                mOrigin, getEmbedderSafe(), ContentSetting.toInt(value));
    }

    /**
     * Sets video capture permission.
     * @param value ALLOW to allow video capture, BLOCK otherwise.
     */
    public void setVideoCapturePermission(ContentSetting value) {
        WebsitePreferenceBridge.nativeSetVideoCaptureSettingForOrigin(
                mOrigin, getEmbedderSafe(), ContentSetting.toInt(value));
    }
}
