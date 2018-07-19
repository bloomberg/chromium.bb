// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_NON_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.widget.ImageView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.vr.mock.MockVrDaydreamApi;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.NfcSimUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;

import java.util.concurrent.TimeoutException;

/**
 * End-to-end tests for state transitions in VR, e.g. exiting WebVR presentation
 * into the VR browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class VrBrowserTransitionTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mTestRule = new ChromeTabbedActivityVrTestRule();

    private WebXrVrTestFramework mWebXrVrTestFramework;
    private WebVrTestFramework mWebVrTestFramework;
    private VrBrowserTestFramework mVrBrowserTestFramework;

    @Before
    public void setUp() throws Exception {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        mWebVrTestFramework = new WebVrTestFramework(mTestRule);
        mVrBrowserTestFramework = new VrBrowserTestFramework(mTestRule);
    }

    private void enterVrShellNfc(boolean supported) {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        if (supported) {
            NfcSimUtils.simNfcScanUntilVrEntry(mTestRule.getActivity());
            Assert.assertTrue(VrShellDelegate.isInVr());
        } else {
            NfcSimUtils.simNfcScan(mTestRule.getActivity());
            Assert.assertFalse(VrShellDelegate.isInVr());
        }
        VrBrowserTransitionUtils.forceExitVr();
    }

    private void enterExitVrShell(boolean supported) {
        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        if (!supported) {
            VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApi);
        }
        VrBrowserTransitionUtils.forceEnterVrBrowser();
        if (supported) {
            VrBrowserTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
            Assert.assertTrue(VrShellDelegate.isInVr());
        } else {
            Assert.assertFalse(mockApi.getLaunchInVrCalled());
            Assert.assertFalse(VrShellDelegate.isInVr());
        }
        VrBrowserTransitionUtils.forceExitVr();
        Assert.assertFalse(VrShellDelegate.isInVr());
    }

    /**
     * Verifies that browser successfully transitions from 2D Chrome to the VR
     * browser when the Daydream View NFC tag is scanned on a Daydream-ready device.
     */
    @Test
    @Restriction({RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @RetryOnFailure(message = "crbug.com/736527")
    @LargeTest
    public void test2dtoVrShellNfcSupported() {
        enterVrShellNfc(true /* supported */);
    }

    /**
     * Verifies that the browser does not transition from 2D chrome to the VR
     * browser when the Daydream View NFC tag is scanned on a non-Daydream-ready
     * device.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @MediumTest
    public void test2dtoVrShellNfcUnsupported() {
        enterVrShellNfc(false /* supported */);
    }

    /**
     * Verifies that browser successfully transitions from 2D chrome to the VR
     * browser and back when the VrShellDelegate tells it to on a Daydream-ready
     * device.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void test2dtoVrShellto2dSupported() {
        enterExitVrShell(true /* supported */);
    }

    /**
     * Verifies that browser successfully transitions from 2D Chrome to the VR
     * browser when Chrome gets a VR intent.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testVrIntentStartsVrShell() {
        testVrEntryIntentInternal();
    }

    private String testVrEntryIntentInternal() {
        // Send a VR intent, which will open the link in a CTA.
        final String url =
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page");
        VrBrowserTransitionUtils.sendVrLaunchIntent(
                url, false /* autopresent */, true /* avoidRelaunch */);

        // Wait until we enter VR and have the correct URL.
        VrBrowserTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mTestRule.getWebContents().getVisibleUrl().equals(url);
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);
        return url;
    }

    /**
     * Verifies that browser successfully transitions from 2D Chrome to the VR
     * browser when Chrome gets a VR intent, and returns to 2D when Chrome receives a 2D Intent.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void test2dIntentExitsVrShell() {
        TestVrShellDelegate.getInstance().setAllow2dIntents(true);
        String url = testVrEntryIntentInternal();

        VrBrowserTransitionUtils.send2dMainIntent();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !VrShellDelegate.isInVr();
            }
        }, POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);
        Assert.assertEquals("Url correct", url, mTestRule.getWebContents().getVisibleUrl());
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_DEVICE_NON_DAYDREAM)
    @MediumTest
    public void test2dtoVrShellto2dUnsupported() {
        enterExitVrShell(false /* supported */);
    }

    /**
     * Tests that we exit fullscreen mode after exiting VR from cinema mode.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testExitFullscreenAfterExitingVrFromCinemaMode()
            throws InterruptedException, TimeoutException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"),
                PAGE_LOAD_TIMEOUT_S);
        DOMUtils.clickNode(mVrBrowserTestFramework.getFirstTabWebContents(), "fullscreen",
                false /* goThroughRootAndroidView */);
        mVrBrowserTestFramework.waitOnJavaScriptStep();

        Assert.assertTrue(DOMUtils.isFullscreen(mVrBrowserTestFramework.getFirstTabWebContents()));
        VrBrowserTransitionUtils.forceExitVr();
        // The fullscreen exit from exiting VR isn't necessarily instantaneous, so give it
        // a bit of time.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return !DOMUtils.isFullscreen(mVrBrowserTestFramework.getFirstTabWebContents());
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        }, POLL_TIMEOUT_SHORT_MS, POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Tests that the reported display dimensions are correct when exiting
     * from WebVR presentation to the VR browser.
     */
    @Test
    @CommandLineFlags.Add("enable-webvr")
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testExitPresentationWebVrToVrShell()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        exitPresentationToVrShellImpl(
                WebVrTestFramework.getFileUrlForHtmlTestFile("test_navigation_webvr_page"),
                mWebVrTestFramework, "vrDisplay.exitPresent();");
    }

    /**
     * Tests that the reported display dimensions are correct when exiting
     * from WebVR presentation to the VR browser.
     */
    @Test
    @CommandLineFlags.Add("enable-features=WebXR")
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testExitPresentationWebXrToVrShell()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        exitPresentationToVrShellImpl(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_navigation_webxr_page"),
                mWebXrVrTestFramework, "immersiveSession.end();");
    }

    private void exitPresentationToVrShellImpl(String url, WebXrVrTestFramework framework,
            String exitPresentString) throws InterruptedException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        VrShellImpl vrShellImpl = (VrShellImpl) TestVrShellDelegate.getVrShellForTesting();
        float expectedWidth = vrShellImpl.getContentWidthForTesting();
        float expectedHeight = vrShellImpl.getContentHeightForTesting();
        framework.enterSessionWithUserGestureOrFail();

        // Validate our size is what we expect while in VR.
        // We aren't comparing for equality because there is some rounding that occurs.
        String javascript = "Math.abs(screen.width - " + expectedWidth + ") <= 1 && "
                + "Math.abs(screen.height - " + expectedHeight + ") <= 1";
        Assert.assertTrue(framework.pollJavaScriptBoolean(javascript, POLL_TIMEOUT_LONG_MS));

        // Exit presentation through JavaScript.
        framework.runJavaScriptOrFail(exitPresentString, POLL_TIMEOUT_SHORT_MS);

        Assert.assertTrue(framework.pollJavaScriptBoolean(javascript, POLL_TIMEOUT_LONG_MS));
    }

    /**
     * Tests that entering WebVR presentation from the VR browser, exiting presentation, and
     * re-entering presentation works. This is a regression test for crbug.com/799999.
     */
    @Test
    @CommandLineFlags.Add("enable-webvr")
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testWebVrReEntryFromVrBrowser() throws InterruptedException, TimeoutException {
        reEntryFromVrBrowserImpl(
                WebVrTestFramework.getFileUrlForHtmlTestFile("test_webvr_reentry_from_vr_browser"),
                mWebVrTestFramework);
    }

    /**
     * Tests that entering WebVR presentation from the VR browser, exiting presentation, and
     * re-entering presentation works. This is a regression test for crbug.com/799999.
     */
    @Test
    @CommandLineFlags.Add("enable-features=WebXR")
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testWebXrReEntryFromVrBrowser() throws InterruptedException, TimeoutException {
        reEntryFromVrBrowserImpl(WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                                         "test_webxr_reentry_from_vr_browser"),
                mWebXrVrTestFramework);
    }

    private void reEntryFromVrBrowserImpl(String url, WebXrVrTestFramework framework)
            throws InterruptedException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        EmulatedVrController controller = new EmulatedVrController(mTestRule.getActivity());

        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();

        framework.executeStepAndWait("stepVerifyFirstPresent()");
        // The bug did not reproduce with vrDisplay.exitPresent(), so it might not reproduce with
        // session.end(). Instead, use the controller to exit.
        controller.pressReleaseAppButton();
        framework.executeStepAndWait("stepVerifyMagicWindow()");

        framework.enterSessionWithUserGestureOrFail();
        framework.executeStepAndWait("stepVerifySecondPresent()");

        framework.endTest();
    }

    /**
     * Tests that you can enter VR in Overview mode, and the NTP shows up.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testEnterVrInOverviewMode() throws InterruptedException, TimeoutException {
        final ChromeTabbedActivity activity = mTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ImageView tabSwitcher = (ImageView) activity.findViewById(R.id.tab_switcher_button);
                tabSwitcher.callOnClick();
            }
        });

        Assert.assertTrue(activity.isInOverviewMode());

        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApi);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        Assert.assertFalse(mockApi.getExitFromVrCalled());
        Assert.assertFalse(mockApi.getLaunchVrHomescreenCalled());
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(null);
    }

    /**
     * Tests that attempting to start an Activity through the Activity context in VR triggers DOFF.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testStartActivityTriggersDoffChromeActivity()
            throws InterruptedException, TimeoutException {
        testStartActivityTriggersDoffImpl(mTestRule.getActivity());
    }

    /**
     * Tests that attempting to start an Activity through the Application context in VR triggers
     * DOFF.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testStartActivityTriggersDoffAppContext()
            throws InterruptedException, TimeoutException {
        testStartActivityTriggersDoffImpl(mTestRule.getActivity().getApplicationContext());
    }

    private void testStartActivityTriggersDoffImpl(Context context)
            throws InterruptedException, TimeoutException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        mockApi.setExitFromVrReturnValue(false);
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApi);

        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            ThreadUtils.runOnUiThreadBlocking(() -> {
                Intent preferencesIntent = PreferencesLauncher.createIntentForSettingsPage(
                        context, SingleWebsitePreferences.class.getName());
                context.startActivity(preferencesIntent);
            });
        });
        ThreadUtils.runOnUiThreadBlocking(
                () -> { VrShellDelegateUtils.getDelegateInstance().acceptDoffPromptForTesting(); });

        CriteriaHelper.pollUiThread(() -> { return mockApi.getExitFromVrCalled(); });
        Assert.assertFalse(mockApi.getLaunchVrHomescreenCalled());
        mockApi.close();

        MockVrDaydreamApi mockApiWithDoff = new MockVrDaydreamApi();
        mockApiWithDoff.setExitFromVrReturnValue(true);

        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApiWithDoff);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            PreferencesLauncher.launchSettingsPage(context, null);
            VrShellDelegateUtils.getDelegateInstance().acceptDoffPromptForTesting();
        });
        CriteriaHelper.pollUiThread(
                () -> { return VrShellDelegateUtils.getDelegateInstance().isShowingDoff(); });
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mTestRule.getActivity().onActivityResult(
                    VrShellDelegate.EXIT_VR_RESULT, Activity.RESULT_OK, null);
        });

        ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), Preferences.class);
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(null);
    }

    /**
     * Tests that attempting to start an Activity through the Activity context in VR triggers DOFF.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testStartActivityIfNeeded() throws InterruptedException, TimeoutException {
        Activity context = mTestRule.getActivity();
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Intent preferencesIntent = PreferencesLauncher.createIntentForSettingsPage(
                        context, SingleWebsitePreferences.class.getName());
                Assert.assertFalse(context.startActivityIfNeeded(preferencesIntent, 0));
            }
        });
    }

    /**
     * Tests that exiting VR while a permission prompt or JavaScript dialog is being displayed
     * does not cause a browser crash. Regression test for https://crbug.com/821443.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @LargeTest
    @CommandLineFlags.Add("enable-features=VrBrowsingNativeAndroidUi")
    public void testExitVrWithPromptDisplayed() throws InterruptedException, TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"),
                PAGE_LOAD_TIMEOUT_S);

        // Test JavaScript dialogs.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        // Alerts block JavaScript execution until they're closed, so we can't use the normal
        // runJavaScriptOrFail, as that will time out.
        JavaScriptUtils.executeJavaScript(
                mVrBrowserTestFramework.getFirstTabWebContents(), "alert('Please no crash')");
        VrBrowserTransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);
        VrBrowserTransitionUtils.forceExitVr();

        // Test permission prompts.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mVrBrowserTestFramework.runJavaScriptOrFail(
                "navigator.getUserMedia({video: true}, ()=>{}, ()=>{})", POLL_TIMEOUT_SHORT_MS);
        VrBrowserTransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);
        VrBrowserTransitionUtils.forceExitVr();
    }
}
