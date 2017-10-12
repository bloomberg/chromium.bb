// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.VrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DON_ENABLED;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.app.Activity;
import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.vr_shell.mock.MockVrIntentHandler;
import org.chromium.chrome.browser.vr_shell.rules.VrActivityRestriction;
import org.chromium.chrome.browser.vr_shell.util.NfcSimUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTestRuleUtils;
import org.chromium.chrome.browser.vr_shell.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;

/**
 * End-to-end tests for transitioning between WebVR's magic window and
 * presentation modes.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG, "enable-webvr"})
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT) // WebVR is only supported on K+
public class WebVrTransitionTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultVrTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mVrTestRule;
    private VrTestFramework mVrTestFramework;

    public WebVrTransitionTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mVrTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInVrActivityRestrictionRule(mVrTestRule);
    }

    @Before
    public void setUp() throws Exception {
        mVrTestFramework = new VrTestFramework(mVrTestRule);
    }

    /**
     * Tests that a successful requestPresent call actually enters VR
     */
    @Test
    @MediumTest
    @VrActivityRestriction({VrActivityRestriction.SupportedActivity.ALL})
    public void testRequestPresentEntersVr() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_requestPresent_enters_vr"),
                PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestFramework.getFirstTabCvc(), mVrTestFramework.getFirstTabWebContents());
        Assert.assertTrue("VrShellDelegate is in VR", VrShellDelegate.isInVr());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that WebVR is not exposed if the flag is not on and the page does
     * not have an origin trial token.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Remove({"enable-webvr"})
    public void testWebVrDisabledWithoutFlagSet() throws InterruptedException {
        // TODO(bsheedy): Remove this test once WebVR is on by default without
        // requiring an origin trial.
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_webvr_disabled_without_flag_set"),
                PAGE_LOAD_TIMEOUT_S);
        VrTestFramework.waitOnJavaScriptStep(mVrTestFramework.getFirstTabWebContents());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that scanning the Daydream View NFC tag on supported devices fires the
     * vrdisplayactivate event.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testNfcFiresVrdisplayactivate() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_nfc_fires_vrdisplayactivate"),
                PAGE_LOAD_TIMEOUT_S);
        VrTestFramework.runJavaScriptOrFail(
                "addListener()", POLL_TIMEOUT_LONG_MS, mVrTestFramework.getFirstTabWebContents());
        NfcSimUtils.simNfcScan(mVrTestRule.getActivity());
        VrTestFramework.waitOnJavaScriptStep(mVrTestFramework.getFirstTabWebContents());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that the requestPresent promise doesn't resolve if the DON flow is
     * not completed.
     */
    @Test
    @MediumTest
    @Restriction({RESTRICTION_TYPE_VIEWER_DAYDREAM, RESTRICTION_TYPE_DON_ENABLED})
    public void testPresentationPromiseUnresolvedDuringDon() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("test_presentation_promise_unresolved_during_don"),
                PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestFramework.getFirstTabCvc(), mVrTestFramework.getFirstTabWebContents());
        VrTestFramework.endTest(mVrTestFramework.getFirstTabWebContents());
    }

    /**
     * Tests that an intent from a trusted app such as Daydream Home allows WebVR content
     * to auto present without the need for a user gesture.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("enable-features=WebVrAutopresent")
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testTrustedIntentAllowsAutoPresent() throws InterruptedException {
        VrIntentUtils.setHandlerInstanceForTesting(new MockVrIntentHandler(
                true /* useMockImplementation */, true /* treatIntentsAsTrusted */));

        // Send an autopresent intent, which will open the link in a CCT
        VrTransitionUtils.sendDaydreamAutopresentIntent(
                VrTestFramework.getHtmlTestFile("test_webvr_autopresent"),
                mVrTestRule.getActivity());

        // Wait until a CCT is opened due to the intent
        final AtomicReference<CustomTabActivity> cct = new AtomicReference<CustomTabActivity>();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<WeakReference<Activity>> list = ApplicationStatus.getRunningActivities();
                for (WeakReference<Activity> ref : list) {
                    Activity activity = ref.get();
                    if (activity == null) continue;
                    if (activity instanceof CustomTabActivity) {
                        cct.set((CustomTabActivity) activity);
                        return true;
                    }
                }
                return false;
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);

        // Wait until the tab is ready
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (cct.get().getActivityTab() == null) return false;
                return !cct.get().getActivityTab().isLoading();
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);

        WebContents wc = cct.get().getActivityTab().getWebContents();
        VrTestFramework.waitOnJavaScriptStep(wc);
        VrTestFramework.endTest(wc);
    }

    /**
     * Tests that the omnibox reappears after exiting VR.
     */
    @Test
    @MediumTest
    public void testControlsVisibleAfterExitingVr() throws InterruptedException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile("generic_webvr_page"), PAGE_LOAD_TIMEOUT_S);
        VrTransitionUtils.enterPresentationAndWait(
                mVrTestFramework.getFirstTabCvc(), mVrTestFramework.getFirstTabWebContents());
        VrTransitionUtils.forceExitVr();
        // The hiding of the controls may only propagate after VR has exited, so give it a chance
        // to propagate. In the worst case this test will erroneously pass, but should never
        // erroneously fail, and should only be flaky if omnibox showing is broken.
        Thread.sleep(100);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ChromeActivity activity = mVrTestFramework.getRule().getActivity();
                return activity.getFullscreenManager().getBrowserControlHiddenRatio() == 0.0;
            }
        }, POLL_TIMEOUT_SHORT_MS, POLL_CHECK_INTERVAL_SHORT_MS);
    }
}
