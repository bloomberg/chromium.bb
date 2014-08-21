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
     */
    public static boolean isOfficialBuild() {
        return nativeIsOfficialBuild();
    }

    /**
     * Only native code can see the OFFICIAL_BUILD flag; check it from there.  This function is
     * not handled by initialize() and is not available early in startup (before the native
     * library has loaded).  Calling it before that point will result in an exception.
     */
    private static native boolean nativeIsOfficialBuild();

    /**
     * Get the HTML for the terms of service to be displayed at first run.
     */
    public static String getTermsOfServiceHtml() {
        return nativeGetTermsOfServiceHtml();
    }

    /**
     * The terms of service are a native resource.
     */
    private static native String nativeGetTermsOfServiceHtml();
}
