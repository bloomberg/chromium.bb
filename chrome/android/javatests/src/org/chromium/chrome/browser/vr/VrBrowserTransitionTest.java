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
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.PointF;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.widget.ImageView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.StrictModeContext;
import org.chromium.base.test.BundleTestRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.vr.mock.MockVrDaydreamApi;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.NfcSimUtils;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.browser.vr.util.VrShellDelegateUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * End-to-end tests for state transitions in VR, e.g. exiting WebXR presentation
 * into the VR browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // VR is only supported on L+.
public class VrBrowserTransitionTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mTestRule = new ChromeTabbedActivityVrTestRule();

    @Rule
    public BundleTestRule mBundleTestRule = new BundleTestRule();

    private WebXrVrTestFramework mWebXrVrTestFramework;
    private VrBrowserTestFramework mVrBrowserTestFramework;

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        mVrBrowserTestFramework = new VrBrowserTestFramework(mTestRule);
    }

    private void enterVrShellNfc(boolean supported) {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        if (supported) {
            NfcSimUtils.simNfcScanUntilVrEntry(mTestRule.getActivity());
            Assert.assertTrue("Browser is not in VR", VrShellDelegate.isInVr());
        } else {
            NfcSimUtils.simNfcScan(mTestRule.getActivity());
            Assert.assertFalse("Browser is in VR", VrShellDelegate.isInVr());
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
            Assert.assertTrue("Browser is not in VR", VrShellDelegate.isInVr());
        } else {
            Assert.assertFalse("launchInVr was called", mockApi.getLaunchInVrCalled());
            Assert.assertFalse("Browser is in VR", VrShellDelegate.isInVr());
        }
        VrBrowserTransitionUtils.forceExitVr();
        Assert.assertFalse("Browser is in VR", VrShellDelegate.isInVr());
    }

    /**
     * Verifies that browser successfully transitions from 2D Chrome to the VR
     * browser when the Daydream View NFC tag is scanned on a Daydream-ready device.
     */
    @Test
    @Restriction({RESTRICTION_TYPE_VIEWER_DAYDREAM})
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
        final String url = mVrBrowserTestFramework.getUrlForFile("test_navigation_2d_page");
        VrBrowserTransitionUtils.sendVrLaunchIntent(url);

        // Wait until we enter VR and have the correct URL.
        VrBrowserTransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
        CriteriaHelper.pollUiThread(
                ()
                        -> { return mTestRule.getWebContents().getVisibleUrlString().equals(url); },
                "Displayed URL does not match URL provided to VR launch intent",
                POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);
        return url;
    }

    /**
     * Verifies that browser successfully transitions from 2D Chrome to the VR
     * browser when Chrome gets a VR intent, and returns to 2D when Chrome receives a 2D Intent.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
    @MediumTest
    public void test2dIntentExitsVrShell() {
        TestVrShellDelegate.getInstance().setAllow2dIntents(true);
        String url = testVrEntryIntentInternal();

        VrBrowserTransitionUtils.send2dMainIntent();
        CriteriaHelper.pollUiThread(() -> {
            return !VrShellDelegate.isInVr();
        }, "2D intent did not exit VR", POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);
        Assert.assertEquals(
                "URL is incorrect", url, mTestRule.getWebContents().getVisibleUrlString());
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
    public void testExitFullscreenAfterExitingVrFromCinemaMode() throws TimeoutException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mVrBrowserTestFramework.loadFileAndAwaitInitialization(
                "test_navigation_2d_page", PAGE_LOAD_TIMEOUT_S);
        DOMUtils.clickNode(mVrBrowserTestFramework.getCurrentWebContents(), "fullscreen",
                false /* goThroughRootAndroidView */);
        mVrBrowserTestFramework.waitOnJavaScriptStep();

        Assert.assertTrue("Page is not in fullscreen",
                DOMUtils.isFullscreen(mVrBrowserTestFramework.getCurrentWebContents()));
        VrBrowserTransitionUtils.forceExitVr();
        // The fullscreen exit from exiting VR isn't necessarily instantaneous, so give it
        // a bit of time.
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    try {
                        return !DOMUtils.isFullscreen(
                                mVrBrowserTestFramework.getCurrentWebContents());
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "Exiting VR did not exit fullscreen", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that the reported display dimensions are correct when exiting
     * from WebXR presentation to the VR browser.
     */
    @Test
    @CommandLineFlags.Add("enable-features=WebXR")
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
    @MediumTest
    public void testExitPresentationWebXrToVrShell() throws IllegalArgumentException {
        exitPresentationToVrShellImpl("test_navigation_webxr_page", mWebXrVrTestFramework);
    }

    private void exitPresentationToVrShellImpl(String url, WebXrVrTestFramework framework) {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        float expectedWidth = vrShell.getContentWidthForTesting();
        float expectedHeight = vrShell.getContentHeightForTesting();
        framework.enterSessionWithUserGestureOrFail();

        // Validate our size is what we expect while in VR.
        // We aren't comparing for equality because there is some rounding that occurs.
        String javascript = "Math.abs(screen.width - " + expectedWidth + ") <= 1 && "
                + "Math.abs(screen.height - " + expectedHeight + ") <= 1";
        framework.pollJavaScriptBooleanOrFail(javascript, POLL_TIMEOUT_LONG_MS);

        // Exit presentation through JavaScript.
        framework.endSession();

        framework.pollJavaScriptBooleanOrFail(javascript, POLL_TIMEOUT_LONG_MS);
        framework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that entering WebXR presentation from the VR browser, exiting presentation, and
     * re-entering presentation works. This is a regression test for crbug.com/799999.
     */
    @Test
    @CommandLineFlags.Add("enable-features=WebXR")
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
    @MediumTest
    public void testWebXrReEntryFromVrBrowser() {
        reEntryFromVrBrowserImpl(
                "test_webxr_reentry_from_vr_browser", mWebXrVrTestFramework, false);
    }

    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
    @MediumTest
    public void testWebXrReEntryFromVrBrowserViaEndSession() {
        reEntryFromVrBrowserImpl("test_webxr_reentry_from_vr_browser", mWebXrVrTestFramework, true);
    }

    private void reEntryFromVrBrowserImpl(
            String url, WebXrVrTestFramework framework, boolean useEndSession) {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();

        framework.executeStepAndWait("stepVerifyFirstPresent()");
        if (useEndSession) {
            // Exit presentation through JavaScript.
            framework.endSession();
        } else {
            // The bug did not reproduce with vrDisplay.exitPresent(), so it might not reproduce
            // with session.end(). Instead, use the controller to exit.
            NativeUiUtils.clickAppButton(UserFriendlyElementName.NONE, new PointF());
        }
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
    public void testEnterVrInOverviewMode() {
        final ChromeTabbedActivity activity = mTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ImageView tabSwitcher = (ImageView) activity.findViewById(R.id.tab_switcher_button);
            tabSwitcher.callOnClick();
        });

        Assert.assertTrue("Browser is not in overview mode", activity.isInOverviewMode());

        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApi);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        Assert.assertFalse("exitFromVr was called", mockApi.getExitFromVrCalled());
        Assert.assertFalse("launchVrHomescreen was called", mockApi.getLaunchVrHomescreenCalled());
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(null);
    }

    /**
     * Tests that attempting to start an Activity through the Activity context in VR triggers DOFF.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testStartActivityTriggersDoffChromeActivity() {
        testStartActivityTriggersDoffImpl(mTestRule.getActivity());
    }

    /**
     * Tests that attempting to start an Activity through the Application context in VR triggers
     * DOFF.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testStartActivityTriggersDoffAppContext() {
        testStartActivityTriggersDoffImpl(mTestRule.getActivity().getApplicationContext());
    }

    private void testStartActivityTriggersDoffImpl(Context context) {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        MockVrDaydreamApi mockApi = new MockVrDaydreamApi();
        mockApi.setExitFromVrReturnValue(false);
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApi);

        // Enable the mock controller even though we don't use it, because the real controller will
        // never allow the scene to reach quiescense.
        NativeUiUtils.enableMockedInput();
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                settingsLauncher.launchSettingsActivity(context, SingleWebsiteSettings.class);
            });
        });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { VrShellDelegateUtils.getDelegateInstance().acceptDoffPromptForTesting(); });

        CriteriaHelper.pollUiThread(
                () -> { return mockApi.getExitFromVrCalled(); }, "exitFromVr was not called");
        Assert.assertFalse("launchVrHomescreen was called", mockApi.getLaunchVrHomescreenCalled());
        mockApi.close();

        MockVrDaydreamApi mockApiWithDoff = new MockVrDaydreamApi();
        mockApiWithDoff.setExitFromVrReturnValue(true);

        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(mockApiWithDoff);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(context);
            VrShellDelegateUtils.getDelegateInstance().acceptDoffPromptForTesting();
        });
        CriteriaHelper.pollUiThread(() -> {
            return VrShellDelegateUtils.getDelegateInstance().isShowingDoff();
        }, "DOFF screen was not shown");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTestRule.getActivity().onActivityResult(
                    VrShellDelegate.EXIT_VR_RESULT, Activity.RESULT_OK, null);
        });

        ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SettingsActivity.class);
        VrShellDelegateUtils.getDelegateInstance().overrideDaydreamApiForTesting(null);
    }

    /**
     * Tests that attempting to start an Activity through the Activity context in VR triggers DOFF.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testStartActivityIfNeeded() {
        Activity context = mTestRule.getActivity();
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            Intent preferencesIntent = settingsLauncher.createSettingsActivityIntent(
                    context, SingleWebsiteSettings.class.getName());
            Assert.assertFalse("Starting an activity did not trigger DOFF",
                    context.startActivityIfNeeded(preferencesIntent, 0));
        });
    }

    /**
     * Tests that exiting VR while a permission prompt or JavaScript dialog is being displayed
     * does not cause a browser crash. Regression test for https://crbug.com/821443.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @LargeTest
    public void testExitVrWithPromptDisplayed() {
        mVrBrowserTestFramework.loadFileAndAwaitInitialization(
                "test_navigation_2d_page", PAGE_LOAD_TIMEOUT_S);

        // Test JavaScript dialogs.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        // Alerts block JavaScript execution until they're closed, so we can't use the normal
        // runJavaScriptOrFail, as that will time out.
        JavaScriptUtils.executeJavaScript(
                mVrBrowserTestFramework.getCurrentWebContents(), "alert('Please no crash')");
        VrBrowserTransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);
        VrBrowserTransitionUtils.forceExitVr();

        // Test permission prompts.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mVrBrowserTestFramework.runJavaScriptOrFail(
                "navigator.getUserMedia({video: true}, ()=>{}, ()=>{})", POLL_TIMEOUT_SHORT_MS);
        VrBrowserTransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);
        VrBrowserTransitionUtils.forceExitVr();
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that clicking on the Incognito mode's "Learn More" link triggers DOFF. Automation of
     * a manual test in https://crbug.com/861925.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    @FlakyTest(message = "crbug.com/1032769")
    public void testIncognitoLearnMoreTriggersDoff() {
        mTestRule.newIncognitoTabFromMenu();
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        final IncognitoNewTabPage ntp =
                (IncognitoNewTabPage) mTestRule.getActivity().getActivityTab().getNativePage();
        // Enable the mock controller even though we don't use it, because the real controller will
        // never allow the scene to reach quiescense.
        NativeUiUtils.enableMockedInput();
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { ntp.getView().findViewById(R.id.learn_more).performClick(); });
        });
        // This is a roundabout way of ensuring that the UI that popped up was actually the DOFF
        // prompt.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { VrShellDelegateUtils.getDelegateInstance().acceptDoffPromptForTesting(); });
        CriteriaHelper.pollUiThread(() -> {
            return VrShellDelegateUtils.getDelegateInstance().isShowingDoff();
        }, "Did not enter DOFF flow after accepting DOFF prompt");
        // Not necessary for the test, but helps avoid having to exit VR during the next test's
        // pre-test setup.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTestRule.getActivity().onActivityResult(
                    VrShellDelegate.EXIT_VR_RESULT, Activity.RESULT_OK, null);
        });
    }

    /**
     * Verifies that we fail to enter VR when Async Reprojection fails to be turned on with a
     * Daydream View headset paired.
     */
    @Test
    @Restriction({RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @MediumTest
    public void testVrUnsupportedWhenReprojectionFails() {
        AtomicBoolean failed = new AtomicBoolean(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                VrShell vrShell = new VrShell(
                        mTestRule.getActivity(), VrShellDelegateUtils.getDelegateInstance(), null) {
                    @Override
                    public boolean setAsyncReprojectionEnabled(boolean enabled) {
                        return false;
                    }
                };
            } catch (VrShellDelegate.VrUnsupportedException e) {
                failed.set(true);
            }
        });
        Assert.assertTrue(
                "Creating VrShell didn't fail when Async Reprojection failed.", failed.get());
    }

    /**
     * Verifies that permissions granted outside of VR persist while in VR, even after the page is
     * refreshed. Automation of a manual test from https://crbug.com/861941.
     */
    @Test
    @Restriction({RESTRICTION_TYPE_VIEWER_DAYDREAM})
    @MediumTest
    public void testPermissionsPersistWhenEnteringVrBrowser() {
        mVrBrowserTestFramework.loadFileAndAwaitInitialization(
                "test_permissions_persist_when_entering_vr_browser", PAGE_LOAD_TIMEOUT_S);
        // Ensure that permission requests initially trigger a prompt.
        Assert.assertTrue("Camera permission would not trigger prompt",
                mVrBrowserTestFramework.permissionRequestWouldTriggerPrompt("camera"));
        Assert.assertTrue("Microphone permission would not trigger prompt",
                mVrBrowserTestFramework.permissionRequestWouldTriggerPrompt("microphone"));
        // Request camera and microphone permissions.
        mVrBrowserTestFramework.runJavaScriptOrFail(
                "stepRequestPermission()", POLL_TIMEOUT_SHORT_MS);
        // Accept the resulting prompt and wait for the permissions to be granted to the site.
        PermissionUtils.waitForPermissionPrompt();
        PermissionUtils.acceptPermissionPrompt();
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        // Reload the page and ensure that the permissions are still granted.
        mVrBrowserTestFramework.loadFileAndAwaitInitialization(
                "test_permissions_persist_when_entering_vr_browser", PAGE_LOAD_TIMEOUT_S);
        Assert.assertFalse("Camera permission would trigger prompt after reload",
                mVrBrowserTestFramework.permissionRequestWouldTriggerPrompt("camera"));
        Assert.assertFalse("Microphone permission would trigger prompt after reload",
                mVrBrowserTestFramework.permissionRequestWouldTriggerPrompt("microphone"));
        // Enter the VR Browser and ensure the permission request auto-succeeds.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mVrBrowserTestFramework.executeStepAndWait("stepRequestPermission()");
        mVrBrowserTestFramework.endTest();
    }

    /**
     * Verifies that an NFC scan in 2D Chrome while viewing a native page still successfully enters
     * the VR browser. Automation of a manual test from https://crbug.com/862155.
     */
    @Test
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @MediumTest
    public void testNfcScanOnNativePage() {
        // We can't loop over all the native URLs since multiple NFC entries in a short timespan
        // isn't possible. So, just pick the native history page as a suitable one.
        mTestRule.loadUrl(UrlConstants.NATIVE_HISTORY_URL, PAGE_LOAD_TIMEOUT_S);
        NfcSimUtils.simNfcScanUntilVrEntry(mTestRule.getActivity());
        Assert.assertTrue("Browser is not in VR", VrShellDelegate.isInVr());
        Assert.assertTrue("Browser entered VR, but is not on a native page",
                mTestRule.getActivity().getActivityTab().isNativePage());
        VrBrowserTransitionUtils.forceExitVr();
    }
}
