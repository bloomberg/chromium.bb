// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.browser.vr_shell.TestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr_shell.VrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Manual;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr_shell.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr_shell.util.TransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.io.File;
import java.util.concurrent.TimeoutException;

/**
 * End-to-End test for capturing and comparing screen images for VR Browsering Dialogs
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=VrBrowsingNativeAndroidUi"})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
public class VrShellDialogTest {
    private static final String TEST_IMAGE_DIR = "chrome/test/data/vr/UiCapture";
    private static final File sBaseDirectory =
            new File(UrlUtils.getIsolatedTestFilePath(TEST_IMAGE_DIR));

    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrTestFramework mVrTestFramework;

    @Before
    public void setUp() throws Exception {
        mVrTestFramework = new VrTestFramework(mVrTestRule);

        // Create UiCapture image directory.
        if (!sBaseDirectory.exists() && !sBaseDirectory.isDirectory()) {
            Assert.assertTrue(sBaseDirectory.mkdirs());
        }
    }

    private boolean captureScreen(String filename) {
        File screenshotFile = new File(sBaseDirectory, filename + ".png");
        if (screenshotFile.exists() && !screenshotFile.delete()) return false;

        final UiDevice uiDevice =
                UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        return uiDevice.takeScreenshot(screenshotFile);
    }

    private void displayDialog(String initialPage, String navigationCommand)
            throws InterruptedException, TimeoutException {
        mVrTestFramework.loadUrlAndAwaitInitialization(
                VrTestFramework.getHtmlTestFile(initialPage), PAGE_LOAD_TIMEOUT_S);

        // Display audio permissions prompt.
        Assert.assertTrue(TransitionUtils.forceEnterVr());
        TransitionUtils.waitForVrEntry(POLL_TIMEOUT_LONG_MS);
        VrTestFramework.runJavaScriptOrFail(navigationCommand, POLL_TIMEOUT_SHORT_MS,
                mVrTestFramework.getFirstTabWebContents());
        TransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);

        // There is currently no way to know whether a dialog has been drawn yet,
        // so sleep long enough for it to show up.
        Thread.sleep(1000);
    }

    /**
     *Test navigate to 2D page and launch the Microphone dialog.
     */
    @Test
    @Manual
    @LargeTest
    public void microphoneDialogTest() throws InterruptedException, TimeoutException {
        // Display audio permissions prompt.
        displayDialog(
                "test_navigation_2d_page", "navigator.getUserMedia({audio: true}, ()=>{}, ()=>{})");

        // Capture image
        Assert.assertTrue(captureScreen("MicrophoneDialogTestImage"));
    }

    /**
     *Test navigate to 2D page and launch the Camera dialog.
     */
    @Test
    @Manual
    @LargeTest
    public void cameraDialogTest() throws InterruptedException, TimeoutException {
        // Display Camera permissions prompt.
        displayDialog(
                "test_navigation_2d_page", "navigator.getUserMedia({video: true}, ()=>{}, ()=>{})");

        // Capture image
        Assert.assertTrue(captureScreen("CameraDialogTestImage"));
    }

    /**
     *Test navigate to 2D page and launch the Location dialog.
     */
    @Test
    @Manual
    @LargeTest
    public void locationDialogTest() throws InterruptedException, TimeoutException {
        // Display Location permissions prompt.
        displayDialog("test_navigation_2d_page",
                "navigator.geolocation.getCurrentPosition(()=>{}, ()=>{})");

        // Capture image
        Assert.assertTrue(captureScreen("LocationDialogTestImage"));
    }

    /**
     *Test navigate to 2D page and launch the Notifications dialog.
     */
    @Test
    @Manual
    @LargeTest
    public void notificationDialogTest() throws InterruptedException, TimeoutException {
        // Display Notification permissions prompt.
        displayDialog("test_navigation_2d_page", "Notification.requestPermission(()=>{})");

        // Capture image
        Assert.assertTrue(captureScreen("NotificationDialogTestImage"));
    }

    /**
     *Test navigate to 2D page and launch the MIDI dialog.
     */
    @Test
    @Manual
    @LargeTest
    public void midiDialogTest() throws InterruptedException, TimeoutException {
        // Display MIDI permissions prompt.
        displayDialog("test_navigation_2d_page", "navigator.requestMIDIAccess({sysex: true})");

        // Capture image
        Assert.assertTrue(captureScreen("MidiDialogTestImage"));
    }
}