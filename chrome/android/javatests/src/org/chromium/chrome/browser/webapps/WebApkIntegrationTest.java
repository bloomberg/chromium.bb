// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.NativeLibraryTestRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webapk.lib.common.WebApkConstants;

/** Integration tests for WebAPK feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class WebApkIntegrationTest {
    @Rule
    public final ChromeActivityTestRule<WebApkActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(WebApkActivity.class);

    @Rule
    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final long STARTUP_TIMEOUT = ScalableTimeout.scaleTimeout(10000);

    public void startWebApkActivity(String webApkPackageName, final String startUrl)
            throws InterruptedException {
        Intent intent =
                new Intent(InstrumentationRegistry.getTargetContext(), WebApkActivity.class);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, startUrl);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        WebApkActivity webApkActivity =
                (WebApkActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                        intent);
        mActivityTestRule.setActivity(webApkActivity);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getActivityTab() != null;
            }
        }, STARTUP_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), startUrl);
    }

    /** Waits for the splash screen to be hidden. */
    public void waitUntilSplashscreenHides() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getSplashScreenForTests() == null;
            }
        });
    }

    /** Register WebAPK with WebappDataStorage */
    private WebappDataStorage registerWithStorage(final String webappId) throws Exception {
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(webappId, callback);
        callback.waitForCallback(0);
        return WebappRegistry.getInstance().getWebappDataStorage(webappId);
    }

    @Before
    public void setUp() throws Exception {
        WebApkUpdateManager.setUpdatesEnabledForTesting(false);
    }

    /**
     * Test launching a WebAPK. Test that loading the start page works and that the splashscreen
     * eventually hides.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchAndNavigateOffOrigin() throws Exception {
        startWebApkActivity("org.chromium.webapk",
                mTestServerRule.getServer().getURL("/chrome/test/data/android/test.html"));
        waitUntilSplashscreenHides();

        // We navigate outside origin and expect Custom Tab to open on top of WebApkActivity.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "window.top.location = 'https://www.google.com/'");

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!(activity instanceof CustomTabActivity)) {
                    return false;
                }
                CustomTabActivity customTab = (CustomTabActivity) activity;
                return customTab.getActivityTab() != null
                        // Dropping the TLD as Google can redirect to a local site.
                        && customTab.getActivityTab().getUrl().startsWith("https://www.google.");
            }
        });

        CustomTabActivity customTab =
                (CustomTabActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        Assert.assertTrue(
                "Sending to external handlers needs to be enabled for redirect back (e.g. OAuth).",
                IntentUtils.safeGetBooleanExtra(customTab.getIntent(),
                        CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, false));
    }

    /**
     * Test that on first launch:
     * - the "WebApk.LaunchInterval" histogram is not recorded (because there is no prevous launch
     *   to compute the interval from).
     * - the "last used" time is updated (to compute future "launch intervals").
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchIntervalHistogramNotRecordedOnFirstLaunch() throws Exception {
        final String histogramName = "WebApk.LaunchInterval";
        final String packageName = "org.chromium.webapk";
        startWebApkActivity(packageName,
                mTestServerRule.getServer().getURL("/chrome/test/data/android/test.html"));

        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp();
            }
        });
        Assert.assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(
                WebApkConstants.WEBAPK_ID_PREFIX + packageName);
        Assert.assertNotEquals(WebappDataStorage.TIMESTAMP_INVALID, storage.getLastUsedTime());
    }

    /** Test that the "WebApk.LaunchInterval" histogram is recorded on susbequent launches. */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchIntervalHistogramRecordedOnSecondLaunch() throws Exception {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();

        final String histogramName = "WebApk.LaunchInterval";
        final String packageName = "org.chromium.webapk";

        WebappDataStorage storage =
                registerWithStorage(WebApkConstants.WEBAPK_ID_PREFIX + packageName);
        storage.setHasBeenLaunched();
        storage.updateLastUsedTime();
        Assert.assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(histogramName));

        startWebApkActivity(packageName,
                mTestServerRule.getServer().getURL("/chrome/test/data/android/test.html"));

        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp();
            }
        });

        Assert.assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
    }
}
