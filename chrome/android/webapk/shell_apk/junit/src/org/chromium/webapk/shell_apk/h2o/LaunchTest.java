// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.WebApkUtils;

import java.util.ArrayList;

/** Tests launching WebAPK. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, packageName = LaunchTest.WEBAPK_PACKAGE_NAME)
public final class LaunchTest {
    /** Values based on manifest specified in GN file. */
    public static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.h2o.junit_webapk";
    private static final String BROWSER_PACKAGE_NAME = "com.google.android.apps.chrome";
    private static final String SHARE_ACTIVITY1_CLASS_NAME =
            "org.chromium.webapk.shell_apk.ShareActivity1";
    private static final String DEFAULT_START_URL = "https://pwa.rocks/";

    /** Chromium version which does not support showing the splash screen within WebAPK. */
    private static final int BROWSER_H2O_INCOMPATIBLE_VERSION = 57;

    private ShadowApplication mShadowApplication;
    private PackageManager mPackageManager;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        mShadowApplication = ShadowApplication.getInstance();
        mPackageManager = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = Shadows.shadowOf(mPackageManager);
    }

    /**
     * Test launching via a deep link.
     * Check:
     * 1) That the host browser was launched.
     * 2) Which activities were launnched between the activity which handled
     * the intent and the host browser getting launched.
     */
    @Test
    @Ignore
    public void testDeepLink() {
        final String deepLinkUrl = "https://pwa.rocks/deep.html";

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(deepLinkUrl));
        launchIntent.setPackage(WEBAPK_PACKAGE_NAME);

        ArrayList<Intent> launchedIntents;
        launchedIntents = launchAndCheckBrowserLaunched(false /* splashActivityInitiallyEnabled */,
                false /* browserCompatibleWithSplashActivity */, launchIntent,
                H2OTransparentLauncherActivity.class, deepLinkUrl);
        Assert.assertEquals(1, launchedIntents.size());

        launchedIntents = launchAndCheckBrowserLaunched(false /* splashActivityInitiallyEnabled */,
                true /* browserCompatibleWithSplashActivity */, launchIntent,
                H2OTransparentLauncherActivity.class, deepLinkUrl);
        Assert.assertEquals(2, launchedIntents.size());
        assertIntentComponentClassNameEquals(H2OMainActivity.class, launchedIntents.get(0));

        launchedIntents = launchAndCheckBrowserLaunched(true /* splashActivityInitiallyEnabled */,
                false /* browserCompatibleWithSplashActivity */, launchIntent,
                H2OTransparentLauncherActivity.class, deepLinkUrl);
        Assert.assertEquals(2, launchedIntents.size());
        assertIntentComponentClassNameEquals(SplashActivity.class, launchedIntents.get(0));

        launchedIntents = launchAndCheckBrowserLaunched(true /* splashActivityInitiallyEnabled */,
                true /* browserCompatibleWithSplashActivity */, launchIntent,
                H2OTransparentLauncherActivity.class, deepLinkUrl);
        Assert.assertEquals(2, launchedIntents.size());
        assertIntentComponentClassNameEquals(SplashActivity.class, launchedIntents.get(0));
    }

    /** Test that the host browser is launched as a result of a main launch intent. */
    @Test
    @Ignore
    public void testMainIntent() {
        Intent launchIntent = new Intent(Intent.ACTION_MAIN);
        launchIntent.setPackage(WEBAPK_PACKAGE_NAME);

        ArrayList<Intent> launchedIntents;
        launchedIntents = launchAndCheckBrowserLaunched(false /* splashActivityInitiallyEnabled */,
                false /* browserCompatibleWithSplashActivity */, launchIntent,
                H2OMainActivity.class, DEFAULT_START_URL);
        Assert.assertEquals(1, launchedIntents.size());

        launchedIntents = launchAndCheckBrowserLaunched(false /* splashActivityInitiallyEnabled */,
                true /* browserCompatibleWithSplashActivity */, launchIntent, H2OMainActivity.class,
                DEFAULT_START_URL);
        Assert.assertEquals(1, launchedIntents.size());

        launchedIntents = launchAndCheckBrowserLaunched(true /* splashActivityInitiallyEnabled */,
                false /* browserCompatibleWithSplashActivity */, launchIntent, SplashActivity.class,
                DEFAULT_START_URL);
        Assert.assertEquals(1, launchedIntents.size());

        launchedIntents = launchAndCheckBrowserLaunched(true /* splashActivityInitiallyEnabled */,
                true /* browserCompatibleWithSplashActivity */, launchIntent, SplashActivity.class,
                DEFAULT_START_URL);
        Assert.assertEquals(1, launchedIntents.size());
    }

    /**
     * Tests that the target share activity is propagated to the host browser launch intent in
     * the scenario where there are several hops between the share intent getting handled and the
     * browser getting launched.
     */
    @Test
    @Ignore
    public void testTargetShareActivityPreserved() {
        final String expectedStartUrl = "https://pwa.rocks/share_title.html?text=subject_value";

        Intent launchIntent = new Intent(Intent.ACTION_SEND);
        launchIntent.setComponent(
                new ComponentName(WEBAPK_PACKAGE_NAME, SHARE_ACTIVITY1_CLASS_NAME));
        launchIntent.putExtra(Intent.EXTRA_TEXT, "subject_value");

        ArrayList<Intent> launchedIntents =
                launchAndCheckBrowserLaunched(true /* splashActivityInitiallyEnabled */,
                        false /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class, expectedStartUrl);
        Assert.assertTrue(launchedIntents.size() > 1);

        Intent browserLaunchIntent = launchedIntents.get(launchedIntents.size() - 1);
        Assert.assertEquals(SHARE_ACTIVITY1_CLASS_NAME,
                browserLaunchIntent.getStringExtra(
                        WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME));
    }

    /**
     * Tests that the EXTRA_SOURCE intent extra in the launch intent is propagated to the host
     * browser launch intent in the scenario where there are several activity hops between
     * the deep link getting handled and the host browser getting launched.
     */
    @Test
    @Ignore
    public void testSourcePropagated() {
        final String deepLinkUrl = "https://pwa.rocks/deep_link.html";
        final int source = 2;

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(deepLinkUrl));
        launchIntent.setPackage(WEBAPK_PACKAGE_NAME);
        launchIntent.putExtra(WebApkConstants.EXTRA_SOURCE, source);

        ArrayList<Intent> launchedIntents =
                launchAndCheckBrowserLaunched(true /* splashActivityInitiallyEnabled */,
                        true /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class, deepLinkUrl);
        Assert.assertTrue(launchedIntents.size() > 1);

        Intent browserLaunchIntent = launchedIntents.get(launchedIntents.size() - 1);
        Assert.assertEquals(
                source, browserLaunchIntent.getIntExtra(WebApkConstants.EXTRA_SOURCE, -1));
    }

    /** Checks the name of the intent's component class name. */
    private static void assertIntentComponentClassNameEquals(Class expectedClass, Intent intent) {
        Assert.assertEquals(expectedClass.getName(), intent.getComponent().getClassName());
    }

    /**
     * Launches WebAPK with the given intent and configuration. Tests that the host browser is
     * launched and which activities are enabled after the browser launch.
     * @param splashActivityInitiallyEnabled Whether SplashActivity is enabled at the beginning
     *         of the test case.
     * @param browserCompatibleWithSplashActivity Whether the host browser supports the ShellAPK
     *         showing the splash screen.
     * @param launchIntent Intent to launch.
     * @param launchActivity Activity which should receive the launch intent.
     * @param expectedLaunchUrl The expected launch URL.
     * @return List of launched activity intents (including the host browser launch intent).
     */
    private ArrayList<Intent> launchAndCheckBrowserLaunched(boolean splashActivityInitiallyEnabled,
            boolean browserCompatibleWithSplashActivity, Intent launchIntent,
            Class<? extends Activity> launchActivity, String expectedLaunchUrl) {
        ComponentName h2oMainComponent =
                new ComponentName(WEBAPK_PACKAGE_NAME, H2OMainActivity.class.getName());
        ComponentName splashComponent =
                new ComponentName(WEBAPK_PACKAGE_NAME, SplashActivity.class.getName());

        mPackageManager.setComponentEnabledSetting(
                splashActivityInitiallyEnabled ? splashComponent : h2oMainComponent,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
        mPackageManager.setComponentEnabledSetting(
                splashActivityInitiallyEnabled ? h2oMainComponent : splashComponent,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
        installBrowser(BROWSER_PACKAGE_NAME,
                browserCompatibleWithSplashActivity
                        ? H2OLauncher.MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH
                        : BROWSER_H2O_INCOMPATIBLE_VERSION);

        // Android modifies the intent when the intent is used to launch an activity. Clone the
        // intent so as not to affect test cases which use the same intent.
        Intent launchIntentCopy = (Intent) launchIntent.clone();

        ArrayList<Intent> launchedIntents =
                runActivityChain(launchIntentCopy, launchActivity, BROWSER_PACKAGE_NAME);

        Assert.assertTrue(!launchedIntents.isEmpty());
        Intent browserLaunchIntent = launchedIntents.get(launchedIntents.size() - 1);
        Assert.assertEquals(
                HostBrowserLauncher.ACTION_START_WEBAPK, browserLaunchIntent.getAction());
        Assert.assertEquals(
                expectedLaunchUrl, browserLaunchIntent.getStringExtra(WebApkConstants.EXTRA_URL));

        Assert.assertEquals(browserCompatibleWithSplashActivity,
                isWebApkActivityEnabled(mPackageManager, splashComponent));
        Assert.assertEquals(!browserCompatibleWithSplashActivity,
                isWebApkActivityEnabled(mPackageManager, h2oMainComponent));

        return launchedIntents;
    }

    /** Returns whether the passed in component is enabled. */
    private static boolean isWebApkActivityEnabled(
            PackageManager packageManager, ComponentName component) {
        int enabledSetting = packageManager.getComponentEnabledSetting(component);
        return (enabledSetting == PackageManager.COMPONENT_ENABLED_STATE_ENABLED);
    }

    /**
     * Launches activity with the given intent. Runs till the browser package is launched. Returns
     * the chain of launched activities (including the browser launch).
     */
    @SuppressWarnings("unchecked")
    private ArrayList<Intent> runActivityChain(
            Intent launchIntent, Class<? extends Activity> launchActivity, String browserPackage) {
        ArrayList<Intent> activityIntentChain = new ArrayList<Intent>();

        Robolectric.buildActivity(launchActivity, launchIntent).create();
        for (;;) {
            Intent startedActivityIntent = mShadowApplication.getNextStartedActivity();
            if (startedActivityIntent == null) break;

            activityIntentChain.add(startedActivityIntent);

            if (browserPackage.equals(startedActivityIntent.getPackage())) break;

            Class<? extends Activity> startedActivityClass = null;
            try {
                startedActivityClass = (Class<? extends Activity>) Class.forName(
                        startedActivityIntent.getComponent().getClassName());
            } catch (ClassNotFoundException e) {
                Assert.fail();
            }
            Robolectric.buildActivity(startedActivityClass, startedActivityIntent).create();
        }
        return activityIntentChain;
    }

    /** Installs browser with the given package name and version. */
    private void installBrowser(String browserPackageName, int version) {
        Intent intent = WebApkUtils.getQueryInstalledBrowsersIntent();
        mShadowPackageManager.addResolveInfoForIntent(intent, newResolveInfo(browserPackageName));
        mShadowPackageManager.addPackage(newPackageInfo(browserPackageName, version));
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private static PackageInfo newPackageInfo(String packageName, int version) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.versionName = version + ".";
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.enabled = true;
        return packageInfo;
    }
}
