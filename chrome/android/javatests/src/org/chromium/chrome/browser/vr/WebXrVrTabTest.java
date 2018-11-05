// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for WebVR's behavior when multiple tabs are involved.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
public class WebXrVrTabTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;
    private WebVrTestFramework mWebVrTestFramework;

    public WebXrVrTabTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() throws Exception {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        mWebVrTestFramework = new WebVrTestFramework(mTestRule);
    }

    /**
     * Tests that non-focused tabs cannot get pose information.
     */
    @Test
    @MediumTest
    public void testPoseDataUnfocusedTab() throws InterruptedException {
        testPoseDataUnfocusedTabImpl(
                WebVrTestFramework.getFileUrlForHtmlTestFile("test_pose_data_unfocused_tab"),
                mWebVrTestFramework);
    }

    /**
     * Tests that non-focused tabs don't get WebXR rAFs called.
     */
    @Test
    @MediumTest
    @CommandLineFlags
            .Remove({"enable-webvr"})
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testPoseDataUnfocusedTab_WebXr() throws InterruptedException {
        testPoseDataUnfocusedTabImpl(WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                                             "webxr_test_pose_data_unfocused_tab"),
                mWebXrVrTestFramework);
    }

    private void testPoseDataUnfocusedTabImpl(String url, WebXrVrTestFramework framework)
            throws InterruptedException {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.executeStepAndWait("stepCheckFrameDataWhileFocusedTab()");

        mTestRule.loadUrlInNewTab("about:blank");

        framework.executeStepAndWait("stepCheckFrameDataWhileNonFocusedTab()");
        framework.endTest();
    }

    /**
     * Tests that permissions in use by other tabs are shown while in a WebXR for VR immersive
     * session.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
    @CommandLineFlags
            .Remove({"enable-webvr"})
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testPermissionsInOtherTab() throws InterruptedException {
        testPermissionsInOtherTabImpl(false /* incognito */);
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
    @CommandLineFlags
            .Remove({"enable-webvr"})
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testPermissionsInOtherTabIncognito() throws InterruptedException {
        testPermissionsInOtherTabImpl(true /* incognito */);
    }

    private void testPermissionsInOtherTabImpl(boolean incognito) throws InterruptedException {
        EmbeddedTestServer server = mTestRule.getTestServer();
        boolean teardownServer = false;
        if (server == null) {
            server = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
            teardownServer = true;
        }

        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                server.getURL(WebXrVrTestFramework.getEmbeddedServerPathForHtmlTestFile(
                        "generic_webxr_permission_page")),
                PAGE_LOAD_TIMEOUT_S);
        // Be sure to store the stream we're given so that the permission is actually in use, as
        // otherwise the toast doesn't show up since another tab isn't actually using the
        // permission.
        WebXrVrTestFramework.runJavaScriptOrFail(
                "requestPermission({audio:true}, true /* storeValue */)", POLL_TIMEOUT_SHORT_MS,
                mTestRule.getWebContents());
        PermissionUtils.waitForPermissionPrompt();
        PermissionUtils.acceptPermissionPrompt();
        WebXrVrTestFramework.waitOnJavaScriptStep(mTestRule.getWebContents());

        if (incognito) {
            ThreadUtils.runOnUiThreadBlocking(() -> {
                mTestRule.getActivity()
                        .getTabCreator(true /* incognito */)
                        .launchUrl("about:blank", TabLaunchType.FROM_LINK);
            });
        } else {
            mTestRule.loadUrlInNewTab("about:blank");
        }

        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail(mTestRule.getWebContents());
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.WEB_XR_AUDIO_INDICATOR, true /* visible */, () -> {});

        if (teardownServer) {
            server.stopAndDestroyServer();
        }
    }
}
