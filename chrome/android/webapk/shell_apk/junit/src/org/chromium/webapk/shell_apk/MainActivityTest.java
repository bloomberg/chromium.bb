// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.res.builder.RobolectricPackageManager;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

/**
 * Tests MainActivity.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MainActivityTest {

    private static final String HOST_BROWSER_PACKAGE_NAME = "truly.random";

    private ShadowApplication mShadowApplication;
    private RobolectricPackageManager mPackageManager;

    @Before
    public void setUp() {
        mShadowApplication = Shadows.shadowOf(RuntimeEnvironment.application);
        mShadowApplication.setPackageName(WebApkTestHelper.WEBAPK_PACKAGE_NAME);
        mPackageManager =
                (RobolectricPackageManager) RuntimeEnvironment.application.getPackageManager();
    }

    /**
     * Tests that when the user launches the WebAPK and the user does not have any browser installed
     * that the WebAPK launches Google Play to install the host browser.
     */
    @Test
    public void testBrowserNotInstalled() {
        // Throw ActivityNotFoundException if Intent cannot be resolved.
        mShadowApplication.checkActivities(true);

        // Set WebAPK's meta-data.
        Bundle metaData = new Bundle();
        metaData.putString(WebApkMetaDataKeys.RUNTIME_HOST, HOST_BROWSER_PACKAGE_NAME);
        metaData.putString(WebApkMetaDataKeys.START_URL, "http://random.org");
        WebApkTestHelper.registerWebApkWithMetaData(metaData);

        // Make intents to Google Play not throw ActivityNotFoundException.
        mPackageManager.addResolveInfoForIntent(
                MainActivity.createInstallIntent(HOST_BROWSER_PACKAGE_NAME),
                newResolveInfo("google.play"));

        Robolectric.buildActivity(MainActivity.class).create();

        Intent startActivityIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(startActivityIntent);
        Assert.assertTrue(startActivityIntent.getDataString().startsWith("market://"));
    }

    /**
     * Test that when the user launches the WebAPK and the user has neither a browser nor Google
     * Play installed that launching the WebAPK silently fails and does not crash.
     */
    @Test
    public void testBrowserNotInstalledAndGooglePlayNotInstalled() {
        // Throw ActivityNotFoundException if Intent cannot be resolved.
        mShadowApplication.checkActivities(true);

        // Set WebAPK's meta-data.
        Bundle metaData = new Bundle();
        metaData.putString(WebApkMetaDataKeys.RUNTIME_HOST, HOST_BROWSER_PACKAGE_NAME);
        metaData.putString(WebApkMetaDataKeys.START_URL, "http://random.org");
        WebApkTestHelper.registerWebApkWithMetaData(metaData);

        Robolectric.buildActivity(MainActivity.class).create();

        Intent startActivityIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertNull(startActivityIntent);
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }
}
