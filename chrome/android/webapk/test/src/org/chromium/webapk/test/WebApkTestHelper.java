// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.test;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.res.Resources;
import android.os.Bundle;

import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.res.builder.DefaultPackageManager;
import org.robolectric.res.builder.RobolectricPackageManager;

import java.util.HashMap;

/**
 * Helper class for WebAPK JUnit tests.
 */
public class WebApkTestHelper {
    /** FakePackageManager allows setting up Resources for installed packages. */
    private static class FakePackageManager extends DefaultPackageManager {
        private final HashMap<String, Resources> mResourceMap;

        public FakePackageManager() {
            mResourceMap = new HashMap<>();
        }

        @Override
        public Resources getResourcesForApplication(String appPackageName)
                throws NameNotFoundException {
            Resources result = mResourceMap.get(appPackageName);
            if (result == null) throw new NameNotFoundException(appPackageName);

            return result;
        }

        public void setResourcesForTest(String packageName, Resources resources) {
            mResourceMap.put(packageName, resources);
        }
    }

    /**
     * Setups a new {@FakePackageManager}.
     */
    public static void setUpPackageManager() {
        FakePackageManager packageManager = new FakePackageManager();
        RuntimeEnvironment.setRobolectricPackageManager(packageManager);
    }

    /**
     * Registers WebAPK. This function also creates an empty resource for the WebAPK.
     * @param packageName The package to register
     * @param metaData Bundle with meta data from WebAPK's Android Manifest.
     */
    public static void registerWebApkWithMetaData(String packageName, Bundle metaData) {
        RobolectricPackageManager packageManager =
                RuntimeEnvironment.getRobolectricPackageManager();
        if (!(packageManager instanceof FakePackageManager)) {
            setUpPackageManager();
        }
        packageManager = RuntimeEnvironment.getRobolectricPackageManager();
        Resources res = Mockito.mock(Resources.class);
        ((FakePackageManager) packageManager).setResourcesForTest(packageName, res);
        packageManager.addPackage(newPackageInfo(packageName, metaData));
    }

    /** Sets the resource for the given package name. */
    public static void setResource(String packageName, Resources res) {
        RobolectricPackageManager packageManager =
                RuntimeEnvironment.getRobolectricPackageManager();
        if (packageManager instanceof FakePackageManager) {
            ((FakePackageManager) packageManager).setResourcesForTest(packageName, res);
        }
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
