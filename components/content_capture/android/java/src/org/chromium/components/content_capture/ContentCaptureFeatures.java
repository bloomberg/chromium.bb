// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.content_capture;

/**
 * The class to get if feature is enabled from native.
 */
public class ContentCaptureFeatures {
    public static boolean isEnabled() {
        return nativeIsEnabled();
    }
    private static native boolean nativeIsEnabled();
}
