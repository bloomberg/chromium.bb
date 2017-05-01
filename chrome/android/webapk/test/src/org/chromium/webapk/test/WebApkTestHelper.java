// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.test;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;

import org.robolectric.RuntimeEnvironment;
import org.robolectric.res.builder.RobolectricPackageManager;

/**
 * Helper class for WebAPK JUnit tests.
 */
public class WebApkTestHelper {
    /**
     * Registers WebAPK.
     * @param packageName The package to register
     * @param metaData Bundle with meta data from WebAPK's Android Manifest.
     */
    public static void registerWebApkWithMetaData(String packageName, Bundle metaData) {
        RobolectricPackageManager packageManager =
                (RobolectricPackageManager) RuntimeEnvironment.application.getPackageManager();
        packageManager.addPackage(newPackageInfo(packageName, metaData));
    }

    private static PackageInfo newPackageInfo(String packageName, Bundle metaData) {
        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.metaData = metaData;
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.applicationInfo = applicationInfo;
        return packageInfo;
    }
}
