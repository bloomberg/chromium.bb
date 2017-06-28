// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.res.builder.RobolectricPackageManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for WebApkUtils. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, packageName = WebApkUtilsTest.WEBAPK_PACKAGE_NAME)
public class WebApkUtilsTest {
    protected static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String BROWSER_INSTALLED_SUPPORTING_WEBAPKS = "com.chrome.canary";
    private static final String BROWSER_UNINSTALLED_SUPPORTING_WEBAPKS = "com.chrome.dev";
    private static final String BROWSER_INSTALLED_NOT_SUPPORTING_WEBAPKS =
            "browser.installed.not.supporting.webapks";
    private static final String ANOTHER_BROWSER_INSTALLED_SUPPORTING_WEBAPKS = "com.chrome.beta";

    private static final List<String> sInstalledBrowsers = new ArrayList<String>(Arrays.asList(
            BROWSER_INSTALLED_NOT_SUPPORTING_WEBAPKS, BROWSER_INSTALLED_SUPPORTING_WEBAPKS,
            ANOTHER_BROWSER_INSTALLED_SUPPORTING_WEBAPKS));

    private Context mContext;
    private RobolectricPackageManager mPackageManager;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        WebApkTestHelper.setUpPackageManager();

        mPackageManager = Mockito.spy(RuntimeEnvironment.getRobolectricPackageManager());
        RuntimeEnvironment.setRobolectricPackageManager(mPackageManager);

        WebApkUtils.resetCachedHostPackageForTesting();
    }

    /**
     * Tests that null will be returned if there isn't any browser installed on the device.
     */
    @Test
    public void testReturnsNullWhenNoBrowserInstalled() {
        String hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertNull(hostBrowser);
    }

    /**
     * Tests that the package name of the host browser in the SharedPreference will be returned if
     * it is installed, even if a host browser is specified in the AndroidManifest.xml.
     */
    @Test
    public void testReturnsHostBrowserInSharedPreferenceIfInstalled() {
        String expectedHostBrowser = BROWSER_INSTALLED_SUPPORTING_WEBAPKS;
        mockInstallBrowsers(ANOTHER_BROWSER_INSTALLED_SUPPORTING_WEBAPKS);
        setHostBrowserInMetadata(ANOTHER_BROWSER_INSTALLED_SUPPORTING_WEBAPKS);
        setHostBrowserInSharedPreferences(expectedHostBrowser);

        String hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertEquals(expectedHostBrowser, hostBrowser);
    }

    /**
     * This is a test for the WebAPK WITH a runtime host specified in its AndroidManifest.xml.
     * Tests that the package name of the host browser specified in the AndroidManifest.xml will be
     * returned if:
     * 1. there isn't a host browser specified in the SharedPreference or the specified one is
     *    uninstalled.
     * And
     * 2. the host browser stored in the AndroidManifest is still installed.
     */
    @Test
    public void testReturnsHostBrowserInManifestIfInstalled() {
        String expectedHostBrowser = BROWSER_INSTALLED_SUPPORTING_WEBAPKS;
        mockInstallBrowsers(ANOTHER_BROWSER_INSTALLED_SUPPORTING_WEBAPKS);
        setHostBrowserInMetadata(expectedHostBrowser);
        // Simulates there isn't any host browser stored in the SharedPreference.
        setHostBrowserInSharedPreferences(null);

        String hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertEquals(hostBrowser, expectedHostBrowser);

        WebApkUtils.resetCachedHostPackageForTesting();
        // Simulates there is a host browser stored in the SharedPreference but uninstalled.
        setHostBrowserInSharedPreferences(BROWSER_UNINSTALLED_SUPPORTING_WEBAPKS);
        hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertEquals(expectedHostBrowser, hostBrowser);
    }

    /**
     * This is a test for the WebAPK WITH a runtime host specified in its AndroidManifest.xml.
     * Tests that null will be returned if:
     * 1) there isn't a host browser stored in the SharedPreference or it isn't installed.
     * And
     * 2) the host browser specified in the AndroidManifest.xml isn't installed;
     * In this test, we only simulate the the first part of the condition 1.
     */
    @Test
    public void testReturnsNullIfHostBrowserSpecifiedInManifestIsUninstalled() {
        mockInstallBrowsers(BROWSER_INSTALLED_SUPPORTING_WEBAPKS);
        setHostBrowserInMetadata(BROWSER_UNINSTALLED_SUPPORTING_WEBAPKS);
        // Simulates that there isn't any host browser stored in the SharedPreference.
        setHostBrowserInSharedPreferences(null);

        String hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertNull(hostBrowser);
    }

    /**
     * This is a test for the WebAPK WITHOUT any runtime host specified in its AndroidManifest.xml.
     * Tests that the default browser package name will be returned if:
     * 1. there isn't any host browser stored in the SharedPreference, or the specified one has
     *    been uninstalled.
     * And
     * 2. the default browser supports WebAPKs.
     * In this test, we only simulate the the first part of the condition 1.
     */
    @Test
    public void testReturnsDefaultBrowser() {
        String defaultBrowser = BROWSER_INSTALLED_SUPPORTING_WEBAPKS;
        mockInstallBrowsers(defaultBrowser);
        setHostBrowserInMetadata(null);
        // Simulates that there isn't any host browser stored in the SharedPreference.
        setHostBrowserInSharedPreferences(null);

        String hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertEquals(defaultBrowser, hostBrowser);
    }

    /**
     * This is a test for the WebAPK WITHOUT any runtime host specified in its AndroidManifest.xml.
     * Tests that null will be returned if:
     * 1. there isn't any host browser stored in the SharedPreference, or the specified one has
     *    been uninstalled.
     * And
     * 2. the default browser doesn't support WebAPKs.
     * In this test, we only simulate the the first part of the condition 1.
     */
    @Test
    public void testReturnsNullWhenDefaultBrowserDoesNotSupportWebApks() {
        mockInstallBrowsers(BROWSER_INSTALLED_NOT_SUPPORTING_WEBAPKS);
        setHostBrowserInMetadata(null);
        setHostBrowserInSharedPreferences(null);

        String hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertNull(hostBrowser);
    }

    /**
     * Tests that {@link WebApkUtils#getHostBrowserPackageName(Context)} doesn't return the current
     * host browser which is cached in the {@link WebApkUtils#sHostPackage} and uninstalled.
     */
    @Test
    public void testDoesNotReturnTheCurrentHostBrowserAfterUninstall() {
        String currentHostBrowser = BROWSER_INSTALLED_SUPPORTING_WEBAPKS;
        mockInstallBrowsers(ANOTHER_BROWSER_INSTALLED_SUPPORTING_WEBAPKS);
        setHostBrowserInMetadata(null);
        setHostBrowserInSharedPreferences(currentHostBrowser);

        String hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertEquals(currentHostBrowser, hostBrowser);

        uninstallBrowser(currentHostBrowser);
        hostBrowser = WebApkUtils.getHostBrowserPackageName(mContext);
        Assert.assertNotEquals(currentHostBrowser, hostBrowser);
    }

    /**
     * Uninstall a browser. Note: this function only works for uninstalling the non default browser.
     */
    private void uninstallBrowser(String packageName) {
        Intent intent = null;
        try {
            intent = Intent.parseUri("http://", Intent.URI_INTENT_SCHEME);
        } catch (Exception e) {
            Assert.fail();
            return;
        }
        mPackageManager.removeResolveInfosForIntent(intent, packageName);
        mPackageManager.removePackage(packageName);
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private void mockInstallBrowsers(String defaultBrowser) {
        Intent intent = null;
        try {
            intent = Intent.parseUri("http://", Intent.URI_INTENT_SCHEME);
        } catch (Exception e) {
            Assert.fail();
            return;
        }

        for (String name : sInstalledBrowsers) {
            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(name));
        }

        ResolveInfo defaultBrowserInfo = newResolveInfo(defaultBrowser);
        mPackageManager.addResolveInfoForIntent(intent, defaultBrowserInfo);

        Mockito.when(mPackageManager.resolveActivity(any(Intent.class), anyInt()))
                .thenReturn(defaultBrowserInfo);
    }

    private void setHostBrowserInSharedPreferences(String hostBrowserPackage) {
        SharedPreferences sharedPref =
                mContext.getSharedPreferences(WebApkConstants.PREF_PACKAGE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPref.edit();
        editor.putString(WebApkUtils.SHARED_PREF_RUNTIME_HOST, hostBrowserPackage);
        editor.apply();
    }

    private void setHostBrowserInMetadata(String hostBrowserPackage) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, hostBrowserPackage);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);
    }
}
