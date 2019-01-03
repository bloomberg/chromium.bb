// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.browser;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * This class builds paths for the Chrome package.
 */
public abstract class PackagePaths {
    private static final String TAG = "PackagePaths";

    // Prevent instantiation.
    private PackagePaths() {}

    /**
     * @ Build paths for the chrome/webview package for the purpose of loading CrashpadMain via
     * /system/bin/app_process.
     */
    @CalledByNative
    public static String[] makePackagePaths(String arch) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return new String[] {"", ""};
        }
        try {
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            PackageInfo pi = pm.getPackageInfo(
                    BuildInfo.getInstance().packageName, PackageManager.GET_SHARED_LIBRARY_FILES);

            List<String> zipPaths = new ArrayList<>(10);
            zipPaths.add(pi.applicationInfo.sourceDir);
            if (pi.applicationInfo.splitSourceDirs != null) {
                Collections.addAll(zipPaths, pi.applicationInfo.splitSourceDirs);
            }

            if (pi.applicationInfo.sharedLibraryFiles != null) {
                Collections.addAll(zipPaths, pi.applicationInfo.sharedLibraryFiles);
            }

            List<String> libPaths = new ArrayList<>(10);
            libPaths.add(pi.applicationInfo.nativeLibraryDir);
            for (String zip : zipPaths) {
                if (zip.endsWith(".apk")) {
                    libPaths.add(zip + "!/lib/" + arch);
                }
            }
            libPaths.add(System.getProperty("java.library.path"));

            return new String[] {TextUtils.join(File.pathSeparator, zipPaths),
                    TextUtils.join(File.pathSeparator, libPaths)};

        } catch (NameNotFoundException e) {
            throw new RuntimeException(e);
        }
    }
}
