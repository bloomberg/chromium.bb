// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.support.test.filters.LargeTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

/** Integration tests for WebAPK feature. */
public class WebApkIntegrationTest extends ChromeActivityTestCaseBase<WebApkActivity> {
    private static final long STARTUP_TIMEOUT = ScalableTimeout.scaleTimeout(10000);

    private EmbeddedTestServer mTestServer;

    public WebApkIntegrationTest() {
        super(WebApkActivity.class);
    }

    public void startWebApkActivity(String webApkPackageName, final String startUrl)
            throws InterruptedException {
        Intent intent = new Intent(getInstrumentation().getTargetContext(), WebApkActivity.class);
        intent.putExtra(ShortcutHelper.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, startUrl);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        setActivity(getInstrumentation().startActivitySync(intent));
        getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab() != null;
            }
        }, STARTUP_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), startUrl);
    }

    /** Waits for the splash screen to be hidden. */
    public void waitUntilSplashscreenHides() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !getActivity().isSplashScreenVisibleForTests();
            }
        });
    }

    @Override
    public final void startMainActivity() throws InterruptedException {
        // Do nothing
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        WebApkUpdateManager.setUpdatesEnabledForTesting(false);
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Test launching a WebAPK. Test that loading the start page works and that the splashscreen
     * eventually hides.
     */
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunch() throws InterruptedException {
        startWebApkActivity("org.chromium.webapk.test",
                mTestServer.getURL("/chrome/test/data/android/test.html"));
        waitUntilSplashscreenHides();
    }
}
