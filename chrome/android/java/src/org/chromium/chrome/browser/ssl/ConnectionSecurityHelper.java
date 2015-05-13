// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import org.chromium.content_public.browser.WebContents;

/**
 * Provides a way of accessing helpers for connection security levels.
 */
public class ConnectionSecurityHelper {
    /**
     * Fetch the security level for a given web contents.
     *
     * @param webContents The web contents to get the security level for.
     * @return The ConnectionSecurityHelperSecurityLevel for the specified web contents.
     *
     * @see ConnectionSecurityHelperSecurityLevel
     */
    public static int getSecurityLevelForWebContents(WebContents webContents) {
        if (webContents == null) return ConnectionSecurityHelperSecurityLevel.NONE;
        return nativeGetSecurityLevelForWebContents(webContents);
    }

    private ConnectionSecurityHelper() {}

    private static native int nativeGetSecurityLevelForWebContents(WebContents webContents);
}
