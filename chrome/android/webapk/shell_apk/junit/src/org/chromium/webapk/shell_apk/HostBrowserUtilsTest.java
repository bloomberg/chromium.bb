// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.support.annotation.IntDef;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for HostBrowserUtils. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(sdk = LocalRobolectricTestRunner.DEFAULT_SDK, manifest = Config.NONE)
public class HostBrowserUtilsTest {
    @IntDef({DefaultBrowserWebApkSupport.YES, DefaultBrowserWebApkSupport.NO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserWebApkSupport {
        int YES = 0;
        int NO = 1;
    }

    // Cannot specify a custom package name with {@link ParameterizedRobolectricTestRunner}.
    private static final String WEBAPK_PACKAGE_NAME = "org.robolectric.default";

    private static final String DEFAULT_BROWSER_SUPPORTING_WEBAPKS =
            "com.google.android.apps.chrome";
    private static final String DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS =
            "com.browser.not.supporting.webapks";
    private static final String[] BROWSERS_SUPPORTING_WEBAPKS =
            new String[] {"com.chrome.canary", "com.chrome.dev", "com.chrome.beta"};
    private static final String[] BROWSERS_NOT_SUPPORTING_WEBAPKS =
            new String[] {"com.random.browser1", "com.random.browser2"};
    private static final String[] ALL_BROWSERS =
            mergeStringArrays(new String[] {DEFAULT_BROWSER_SUPPORTING_WEBAPKS,
                                      DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS},
                    BROWSERS_SUPPORTING_WEBAPKS, BROWSERS_NOT_SUPPORTING_WEBAPKS);

    private Context mContext;
    private ShadowPackageManager mPackageManager;
    private SharedPreferences mSharedPrefs;

    private Set<String> mInstalledBrowsers = new HashSet<String>();

    // Whether we are testing with bound WebAPKs.
    private boolean mIsBoundWebApk;

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    public HostBrowserUtilsTest(boolean isBoundWebApk) {
        mIsBoundWebApk = isBoundWebApk;
    }

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;

        mPackageManager = Shadows.shadowOf(mContext.getPackageManager());
        mSharedPrefs = WebApkSharedPreferences.getPrefs(mContext);

        HostBrowserUtils.resetCachedHostPackageForTesting();
    }

    /**
     * Tests that null will be returned if there isn't any browser installed on the device.
     */
    @Test
    public void testReturnsNullWhenNoBrowserInstalled() {
        if (mIsBoundWebApk) {
            // Bound browser in AndroidManifest.xml is no longer installed.
            setHostBrowserInMetadata(BROWSERS_SUPPORTING_WEBAPKS[0]);
        }
        Assert.assertNull(HostBrowserUtils.getHostBrowserPackageName(mContext));
    }

    /**
     * Tests the order of precedence for bound WebAPKs. The expected order of precedence is:
     * 1) Browser specified in shared preferences if it is still installed.
     * 2) Bound browser specified in AndroidManifest.xml
     * The default browser and the number of installed browsers which support WebAPKs should
     * have no effect.
     */
    @Test
    public void testBoundWebApkPrecedence() {
        if (!mIsBoundWebApk) return;

        final String boundBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
        setHostBrowserInMetadata(boundBrowserSupportingWebApks);

        // Shared pref browser: Still installed
        // Bound browser in AndroidManifest.xml: Still installed
        {
            final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[1];
            setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
            setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);
            Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                    HostBrowserUtils.getHostBrowserPackageName(mContext));
        }

        // Shared pref browser: Null
        // Bound browser in AndroidManifest.xml: Still installed
        HostBrowserUtils.resetCachedHostPackageForTesting();
        setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(boundBrowserSupportingWebApks,
                HostBrowserUtils.getHostBrowserPackageName(mContext));

        // Shared pref browser: No longer installed
        // Bound browser in AndroidManifest.xml: Still installed
        HostBrowserUtils.resetCachedHostPackageForTesting();
        setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(boundBrowserSupportingWebApks,
                HostBrowserUtils.getHostBrowserPackageName(mContext));

        // Shared pref browser: Null
        // Bound browser in AndroidManifest.xml: No longer installed
        // Should ignore default browser and number of browsers supporting WebAPKs.
        HostBrowserUtils.resetCachedHostPackageForTesting();
        setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, null);
        setHostBrowserInSharedPreferences(null);
        Assert.assertNull(HostBrowserUtils.getHostBrowserPackageName(mContext));
    }

    /**
     * Tests the order of precedence for unbound WebAPKs. The expected order of precedence is:
     * 1) Browser specified in shared preferences if it is still installed.
     * 2) Default browser if the default browser supports WebAPKs.
     * 3) The browser which supports WebAPKs if there is just one.
     */
    @Test
    public void testUnboundWebApkPrecedence() {
        if (mIsBoundWebApk) return;

        // Shared pref browser: Still installed
        // Default browser: Supports WebAPKs
        {
            final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
            setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
            setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);
            Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                    HostBrowserUtils.getHostBrowserPackageName(mContext));
        }

        // Shared pref browser: Null
        // Default browser: Supports WebAPKs
        HostBrowserUtils.resetCachedHostPackageForTesting();
        setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(DEFAULT_BROWSER_SUPPORTING_WEBAPKS,
                HostBrowserUtils.getHostBrowserPackageName(mContext));

        // Shared pref browser: Null
        // Default browser: Does not support WebAPKs
        // > 1 installed browsers supporting WebAPKs
        HostBrowserUtils.resetCachedHostPackageForTesting();
        setInstalledBrowsers(DefaultBrowserWebApkSupport.NO, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertNull(HostBrowserUtils.getHostBrowserPackageName(mContext));

        // Shared pref browser: Null
        // Default browser: Does not support WebAPKS
        // 1 installed browser supporting WebAPKS
        HostBrowserUtils.resetCachedHostPackageForTesting();
        setInstalledBrowsers(DefaultBrowserWebApkSupport.NO,
                new String[] {BROWSERS_SUPPORTING_WEBAPKS[0], BROWSERS_NOT_SUPPORTING_WEBAPKS[0],
                        BROWSERS_NOT_SUPPORTING_WEBAPKS[1]});
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(BROWSERS_SUPPORTING_WEBAPKS[0],
                HostBrowserUtils.getHostBrowserPackageName(mContext));
    }

    /**
     * Tests that {@link HostBrowserUtils#getHostBrowserPackageName(Context)} doesn't return the
     * current host browser which is cached in the {@link HostBrowserUtils#sHostPackage} and
     * uninstalled.
     */
    @Test
    public void testDoesNotReturnTheCurrentHostBrowserAfterUninstall() {
        if (mIsBoundWebApk) {
            setHostBrowserInMetadata(BROWSERS_SUPPORTING_WEBAPKS[0]);
        }

        final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
        setInstalledBrowsers(DefaultBrowserWebApkSupport.NO, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);
        Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                HostBrowserUtils.getHostBrowserPackageName(mContext));

        uninstallBrowser(sharedPrefBrowserSupportingWebApks);
        Assert.assertNotEquals(sharedPrefBrowserSupportingWebApks,
                HostBrowserUtils.getHostBrowserPackageName(mContext));
    }

    @SafeVarargs
    private static String[] mergeStringArrays(String[]... toMerge) {
        List<String> out = new ArrayList<String>();
        for (String[] toMergeArray : toMerge) {
            out.addAll(Arrays.asList(toMergeArray));
        }
        return out.toArray(new String[0]);
    }

    /** Sets the installed browsers to the passed-in list. */
    private void setInstalledBrowsers(
            @DefaultBrowserWebApkSupport int defaultBrowser, String[] packagesToInstall) {
        HostBrowserUtils.resetCachedHostPackageForTesting();

        while (!mInstalledBrowsers.isEmpty()) {
            uninstallBrowser(mInstalledBrowsers.iterator().next());
        }

        String defaultPackage = (defaultBrowser == DefaultBrowserWebApkSupport.YES)
                ? DEFAULT_BROWSER_SUPPORTING_WEBAPKS
                : DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS;
        installBrowser(defaultPackage);
        if (packagesToInstall != null) {
            for (String packageToInstall : packagesToInstall) {
                installBrowser(packageToInstall);
            }
        }
    }

    private void installBrowser(String packageName) {
        if (mInstalledBrowsers.contains(packageName)) return;

        Intent intent = null;
        try {
            intent = WebApkUtils.getQueryInstalledBrowsersIntent();
        } catch (Exception e) {
            Assert.fail();
            return;
        }
        mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(packageName));
        mPackageManager.addPackage(packageName);
        mInstalledBrowsers.add(packageName);
    }

    /**
     * Uninstall a browser. Note: this function only works for uninstalling the non default browser.
     */
    private void uninstallBrowser(String packageName) {
        Intent intent = null;
        try {
            intent = WebApkUtils.getQueryInstalledBrowsersIntent();
        } catch (Exception e) {
            Assert.fail();
            return;
        }
        mPackageManager.removeResolveInfosForIntent(intent, packageName);
        mPackageManager.removePackage(packageName);
        mInstalledBrowsers.remove(packageName);
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        activityInfo.applicationInfo = new ApplicationInfo();
        activityInfo.applicationInfo.enabled = true;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private void setHostBrowserInSharedPreferences(String hostBrowserPackage) {
        SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(mContext).edit();
        editor.putString(WebApkSharedPreferences.PREF_RUNTIME_HOST, hostBrowserPackage);
        editor.apply();
    }

    private void setHostBrowserInMetadata(String hostBrowserPackage) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, hostBrowserPackage);
        WebApkTestHelper.registerWebApkWithMetaData(WEBAPK_PACKAGE_NAME, bundle);
    }
}
