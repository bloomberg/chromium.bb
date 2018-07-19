// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;

import android.graphics.PointF;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.rules.HeadTrackingMode;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.util.concurrent.TimeoutException;

/**
 * End-to-End test for capturing and comparing screen images for VR Browsering Dialogs
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=VrBrowsingNativeAndroidUi"})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
public class VrBrowserDialogTest {
    // A long enough sleep after entering VR to ensure that the VR entry animations are complete.
    private static final int VR_ENTRY_SLEEP_MS = 1000;
    // A long enough sleep after triggering/interacting with a dialog to ensure that the interaction
    // has propagated through the render pipeline, i.e. the result of the interaction will actually
    // be visible on the screen.
    // TODO(https://crbug.com/826841): Remove this in favor of notifications from the native code.
    private static final int VR_DIALOG_RENDER_SLEEP_MS = 500;
    private static final String TEST_IMAGE_DIR = "chrome/test/data/vr/UiCapture";
    private static final File sBaseDirectory =
            new File(UrlUtils.getIsolatedTestFilePath(TEST_IMAGE_DIR));

    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrBrowserTestFramework mVrBrowserTestFramework;
    private EmbeddedTestServer mServer;

    @Before
    public void setUp() throws Exception {
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);

        // Create UiCapture image directory.
        if (!sBaseDirectory.exists() && !sBaseDirectory.isDirectory()) {
            Assert.assertTrue(sBaseDirectory.mkdirs());
        }
    }

    @After
    public void tearDown() throws Exception {
        if (mServer != null) {
            mServer.stopAndDestroyServer();
        }
    }

    private boolean captureScreen(String filename) throws InterruptedException {
        // Ensure that any UI changes that have been rendered and submitted have actually propogated
        // to the screen.
        NativeUiUtils.waitNumFrames(2);
        // TODO(bsheedy): Make this work on Android P by drawing the view hierarchy to a bitmap.
        File screenshotFile = new File(sBaseDirectory, filename + ".png");
        if (screenshotFile.exists() && !screenshotFile.delete()) return false;

        final UiDevice uiDevice =
                UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        return uiDevice.takeScreenshot(screenshotFile);
    }

    private void displayPermissionPrompt(String initialPage, String navigationCommand)
            throws InterruptedException, TimeoutException {
        // Trying to grant permissions on file:// URLs ends up hitting DCHECKS, so load from a local
        // server instead.
        if (mServer == null) {
            mServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        }
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                mServer.getURL(
                        VrBrowserTestFramework.getEmbeddedServerPathForHtmlTestFile(initialPage)),
                PAGE_LOAD_TIMEOUT_S);

        // Display the given permission prompt.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mVrBrowserTestFramework.runJavaScriptOrFail(navigationCommand, POLL_TIMEOUT_SHORT_MS);
        VrBrowserTransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);

        // There is currently no way to know whether a dialog has been drawn yet,
        // so sleep long enough for it to show up.
        Thread.sleep(VR_ENTRY_SLEEP_MS);
    }

    private void displayJavascriptDialog(String initialPage, String navigationCommand)
            throws InterruptedException, TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile(initialPage), PAGE_LOAD_TIMEOUT_S);

        // Display the JavaScript dialog.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        // We can't use runJavaScriptOrFail here because JavaScript execution is blocked while a
        // JS dialog is visible, so runJavaScriptOrFail will always time out.
        JavaScriptUtils.executeJavaScript(
                mVrBrowserTestFramework.getFirstTabWebContents(), navigationCommand);
        VrBrowserTransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);

        // There is currently no way to know whether a dialog has been drawn yet,
        // so sleep long enough for it to show up.
        Thread.sleep(VR_ENTRY_SLEEP_MS);
    }

    private void clickElement(String initialPage, int elementName)
            throws InterruptedException, TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile(initialPage), PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        Thread.sleep(VR_ENTRY_SLEEP_MS);
        NativeUiUtils.clickElementAndWaitForUiQuiescence(elementName, new PointF(0, 0));
        // Technically not necessary, but clicking on native elements causes the laser to originate
        // from the head, not the controller, which looks strange. Since the point of most of these
        // tests is to verify that things look correct, better to have the laser in a normal
        // position before taking screenshots.
        NativeUiUtils.revertToRealControllerAndWaitForUiQuiescence();
    }

    /**
     * Test navigate to 2D page and launch the Microphone dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testMicrophonePermissionPrompt() throws InterruptedException, TimeoutException {
        // Display audio permissions prompt.
        displayPermissionPrompt(
                "test_navigation_2d_page", "navigator.getUserMedia({audio: true}, ()=>{}, ()=>{})");

        // Capture image
        Assert.assertTrue(captureScreen("MicrophonePermissionPrompt_Visible"));
        NativeUiUtils.clickFallbackUiPositiveButton();
        Assert.assertTrue(captureScreen("MicrophonePermissionPrompt_Granted"));
    }

    /**
     * Test navigate to 2D page and launch the Camera dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testCameraPermissionPrompt() throws InterruptedException, TimeoutException {
        // Display Camera permissions prompt.
        displayPermissionPrompt(
                "test_navigation_2d_page", "navigator.getUserMedia({video: true}, ()=>{}, ()=>{})");

        // Capture image
        Assert.assertTrue(captureScreen("CameraPermissionPrompt_Visible"));
    }

    /**
     * Test navigate to 2D page and launch the Location dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testLocationPermissionPrompt() throws InterruptedException, TimeoutException {
        // Display Location permissions prompt.
        displayPermissionPrompt("test_navigation_2d_page",
                "navigator.geolocation.getCurrentPosition(()=>{}, ()=>{})");

        // Capture image
        Assert.assertTrue(captureScreen("LocationPermissionPrompt_Visible"));
    }

    /**
     * Test navigate to 2D page and launch the Notifications dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testNotificationPermissionPrompt() throws InterruptedException, TimeoutException {
        // Display Notification permissions prompt.
        displayPermissionPrompt(
                "test_navigation_2d_page", "Notification.requestPermission(()=>{})");

        // Capture image
        Assert.assertTrue(captureScreen("NotificationPermissionPrompt_Visible"));
    }

    /**
     * Test navigate to 2D page and launch the MIDI dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testMidiPermisionPrompt() throws InterruptedException, TimeoutException {
        // Display MIDI permissions prompt.
        displayPermissionPrompt(
                "test_navigation_2d_page", "navigator.requestMIDIAccess({sysex: true})");

        // Capture image
        Assert.assertTrue(captureScreen("MidiPermissionPrompt_Visible"));
    }

    /**
     * Test navigate to 2D page and display a JavaScript alert().
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testJavaScriptAlert() throws InterruptedException, TimeoutException {
        // Display a JavaScript alert()
        displayJavascriptDialog(
                "test_navigation_2d_page", "alert('538 perf regressions detected')");

        // Capture image
        Assert.assertTrue(captureScreen("JavaScriptAlert_Visible"));
    }

    /**
     * Test navigate to 2D page and display a JavaScript confirm();
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testJavaScriptConfirm() throws InterruptedException, TimeoutException {
        // Display a JavaScript confirm()
        displayJavascriptDialog(
                "test_navigation_2d_page", "var c = confirm('This is a confirmation dialog')");

        // Capture image
        Assert.assertTrue(captureScreen("JavaScriptConfirm_Visible"));

        NativeUiUtils.clickFallbackUiNegativeButton();
        NativeUiUtils.revertToRealControllerAndWaitForUiQuiescence();
        // Ensure the cancel button was clicked.
        Assert.assertTrue("Negative button clicked",
                mVrBrowserTestFramework.runJavaScriptOrFail("c", POLL_TIMEOUT_SHORT_MS)
                        .equals("false"));
        Assert.assertTrue(captureScreen("JavaScriptConfirm_Dismissed"));
    }

    /**
     * Test navigate to 2D page and display a JavaScript prompt(). Then confirm that it behaves as
     * it would outside of VR.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testJavaScriptPrompt() throws InterruptedException, TimeoutException {
        // Display a JavaScript prompt()
        String expectedString = "Probably most likely yes";
        displayJavascriptDialog("test_navigation_2d_page",
                "var p = prompt('Are the Chrome controls broken?', '" + expectedString + "')");

        // Capture image
        Assert.assertTrue(captureScreen("JavaScriptPrompt_Visible"));
        NativeUiUtils.clickFallbackUiPositiveButton();
        NativeUiUtils.revertToRealControllerAndWaitForUiQuiescence();
        // This JavaScript will only run once the prompt has been dismissed, and the return value
        // will only be what we expect if the positive button was actually clicked (as opposed to
        // canceled).
        Assert.assertTrue("Positive button clicked",
                mVrBrowserTestFramework
                        .runJavaScriptOrFail("p == '" + expectedString + "'", POLL_TIMEOUT_SHORT_MS)
                        .equals("true"));
        Assert.assertTrue(captureScreen("JavaScriptPrompt_Dismissed"));
    }

    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testKeyboardAppearsOnUrlBarClick() throws InterruptedException, TimeoutException {
        clickElement("test_navigation_2d_page", UserFriendlyElementName.URL);
        Assert.assertTrue(captureScreen("KeyboardAppearsOnUrlBarClick_Visible"));
    }

    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testOverflowMenuAppears() throws InterruptedException, TimeoutException {
        clickElement("test_navigation_2d_page", UserFriendlyElementName.OVERFLOW_MENU);
        Assert.assertTrue(captureScreen("OverflowMenuAppears_Visible"));
    }

    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testPageInfoAppearsOnSecurityTokenClick()
            throws InterruptedException, TimeoutException {
        clickElement("test_navigation_2d_page", UserFriendlyElementName.PAGE_INFO_BUTTON);
        Assert.assertTrue(captureScreen("PageInfoAppearsOnSecurityTokenClick_Visible"));
    }
}