// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.common;

/** Contains utility methods which are used both by ShellAPK and by Chrome. */
public class WebApkCommonUtils {
    /**
     * Returns name of "Runtime Dex" asset in Chrome APK based on version.
     * @param version
     * @return Dex asset name.
     */
    public static String getRuntimeDexName(int version) {
        return "webapk" + version + ".dex";
    }
}
