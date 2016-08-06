// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeApplication;

/**
 * Java counterpart to webapk_installer.h
 * Contains functionality to install WebAPKs.
 */
public abstract class WebApkInstaller {
    /**
     * Installs a WebAPK.
     * @param filePath File to install.
     * @param packageName Package name to install WebAPK at.
     * @return True if the install was started. A "true" return value does not guarantee that the
     *         install succeeds.
     */
    public abstract boolean installAsync(String filePath, String packageName);

    @CalledByNative
    static boolean installAsyncFromNative(String filePath, String packageName) {
        Context context = ContextUtils.getApplicationContext();
        WebApkInstaller apkInstaller = ((ChromeApplication) context).createWebApkInstaller();
        if (apkInstaller == null) {
            return false;
        }
        apkInstaller.installAsync(filePath, packageName);
        return true;
    }
}
