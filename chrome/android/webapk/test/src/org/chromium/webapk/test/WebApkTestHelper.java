// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.test;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.os.Bundle;

import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import java.net.URISyntaxException;

/**
 * Helper class for WebAPK JUnit tests.
 */
public class WebApkTestHelper {
    /**
     * Registers WebAPK. This function also creates an empty resource for the WebAPK.
     * @param packageName The package to register
     * @param metaData Bundle with meta data from WebAPK's Android Manifest.
     */
    public static void registerWebApkWithMetaData(String packageName, Bundle metaData) {
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        Resources res = Mockito.mock(Resources.class);
        packageManager.resources.put(packageName, res);
        packageManager.addPackage(newPackageInfo(packageName, metaData));
    }

    /** Registers intent filter for the passed-in package name and URL. */
    public static void addIntentFilterForUrl(String packageName, String url) {
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        try {
            Intent deepLinkIntent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
            deepLinkIntent.addCategory(Intent.CATEGORY_BROWSABLE);
            deepLinkIntent.setPackage(packageName);
            packageManager.addResolveInfoForIntent(deepLinkIntent, newResolveInfo(packageName));
        } catch (URISyntaxException e) {
        }
    }

    /** Sets the resource for the given package name. */
    public static void setResource(String packageName, Resources res) {
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        packageManager.resources.put(packageName, res);
    }

    private static PackageInfo newPackageInfo(String packageName, Bundle metaData) {
        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.metaData = metaData;
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.applicationInfo = applicationInfo;
        return packageInfo;
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }
}
