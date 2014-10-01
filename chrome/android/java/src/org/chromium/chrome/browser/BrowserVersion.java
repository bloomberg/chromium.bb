// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

/**
 * TODO(nileshagrawal): Rename this class to something more appropriate.
 * Provides a way for java code to determine whether Chrome was built as an official build.
 */
public class BrowserVersion {
    /**
     * Check if the browser was built as an "official" build.
     *
     * This function depends on the native library being loaded; calling it before then will crash.
     */
    public static boolean isOfficialBuild() {
        return nativeIsOfficialBuild();
    }

    private static native boolean nativeIsOfficialBuild();
}
