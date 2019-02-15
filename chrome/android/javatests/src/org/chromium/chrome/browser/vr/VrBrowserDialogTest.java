// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.os.Build;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.website.ContentSettingValues;
import org.chromium.chrome.browser.preferences.website.PermissionInfo;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.RenderTestUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/**
 * End-to-End test for capturing and comparing screen images for VR Browsering Dialogs
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserDialogTest {
    // We need to make sure the port is constant, otherwise the URL changes between test runs, which
    // is really bad for image diff tests. There's nothing special about this port other than that
    // it shouldn't be in use by anything.
    private static final int SERVER_PORT = 39558;

    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("components/test/data/permission_dialogs/render_tests");

    private VrBrowserTestFramework mVrBrowserTestFramework;

    @Before
    public void setUp() throws Exception {
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
        mVrTestRule.getEmbeddedTestServerRule().setServerPort(SERVER_PORT);

        // Notifications on O+ are handled via Android Notification Channels, and thus can cause
        // tests to be non-hermetic. Clear any existing channel before starting the test in case
        // something else has left one lying around, and clear again after the test in order to not
        // leak a channel ourselves. Handled in setUp/tearDown instead of just in the notification
        // test so that cleanup always happens even if the test fails.
        clearNotificationChannel();
    }

    @After
    public void tearDown() throws Exception {
        clearNotificationChannel();
    }

    private void clearNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            PermissionInfo notificationSettings =
                    new PermissionInfo(PermissionInfo.Type.NOTIFICATION,
                            "http://127.0.0.1:" + String.valueOf(SERVER_PORT), null, false);
            ThreadUtils.runOnUiThread(
                    () -> notificationSettings.setContentSetting(ContentSettingValues.DEFAULT));
        }
    }

    private void navigateAndDisplayPermissionPrompt(String page, final String promptCommand)
            throws InterruptedException, TimeoutException {
        // Trying to grant permissions on file:// URLs ends up hitting DCHECKS, so load from a local
        // server instead.
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                mVrBrowserTestFramework.getEmbeddedServerUrlForHtmlTestFile(page),
                PAGE_LOAD_TIMEOUT_S);

        // Display the given permission prompt.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        // Wait for any residual animations from entering VR to finish so that they don't get caught
        // later.
        NativeUiUtils.waitForUiQuiescence();
        NativeUiUtils.performActionAndWaitForUiQuiescence(() -> {
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    UserFriendlyElementName.BROWSING_DIALOG, true /* visible */, () -> {
                        mVrBrowserTestFramework.runJavaScriptOrFail(
                                promptCommand, POLL_TIMEOUT_LONG_MS);
                    });
        });
    }

    private void permissionPromptTestImpl(String promptSnippet, String nameBase, int indicatorName,
            final boolean grant) throws InterruptedException, TimeoutException, IOException {
        // Display the requested permission.
        navigateAndDisplayPermissionPrompt("2d_permission_page", promptSnippet);
        // Capture an image with the permission prompt displayed
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                nameBase + "_visible_browser_ui", mRenderTestRule);
        // Some permissions don't result in a permission indicator.
        if (indicatorName == UserFriendlyElementName.NONE) {
            if (grant) {
                NativeUiUtils.clickFallbackUiPositiveButton();
            } else {
                NativeUiUtils.clickFallbackUiNegativeButton();
            }
            // Don't bother capturing an image after granting if we there won't be an indicator.
            // Instead, just check whether interacting with the prompt triggered the correct
            // callback.
        } else {
            NativeUiUtils.performActionAndWaitForVisibilityStatus(
                    indicatorName, grant /* visible */, () -> {
                        try {
                            if (grant) {
                                NativeUiUtils.clickFallbackUiPositiveButton();
                            } else {
                                NativeUiUtils.clickFallbackUiNegativeButton();
                            }
                        } catch (InterruptedException e) {
                            Assert.fail("Interrupted while interacting with permission prompt: "
                                    + e.toString());
                        }
                    });
            NativeUiUtils.waitForUiQuiescence();
            // Capture an image with the permission prompt dismissed.
            RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                    nameBase + (grant ? "_granted" : "_denied") + "_browser_ui", mRenderTestRule);
        }
        // Special case location because the callbacks never fire on swarming, likely because
        // location is disabled at the system level during device provisioning.
        if (!nameBase.equals("location_permission_prompt")) {
            mVrBrowserTestFramework.waitOnJavaScriptStep();
            Assert.assertEquals("Last permission interaction did not have expected grant result",
                    grant,
                    Boolean.valueOf(mVrBrowserTestFramework.runJavaScriptOrFail(
                            "lastPermissionGranted", POLL_TIMEOUT_SHORT_MS)));
            mVrBrowserTestFramework.assertNoJavaScriptErrors();
        }
    }

    /**
     * Test navigate to 2D page and launch the Microphone dialog.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testMicrophonePermissionPrompt()
            throws InterruptedException, TimeoutException, IOException {
        permissionPromptTestImpl("navigator.getUserMedia({audio: true}, onGranted, onDenied)",
                "microphone_permission_prompt",
                UserFriendlyElementName.MICROPHONE_PERMISSION_INDICATOR, true /* grant */);
    }

    /**
     * Test navigate to 2D page and launch the Camera dialog. Not valid on standalones because
     * there is no camera permission.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testCameraPermissionPrompt()
            throws InterruptedException, TimeoutException, IOException {
        permissionPromptTestImpl("navigator.getUserMedia({video: true}, onGranted, onDenied)",
                "camera_permission_prompt", UserFriendlyElementName.CAMERA_PERMISSION_INDICATOR,
                true /* grant */);
    }

    /**
     * Test navigate to 2D page and launch the Location dialog.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testLocationPermissionPrompt()
            throws InterruptedException, TimeoutException, IOException {
        permissionPromptTestImpl("navigator.geolocation.watchPosition(onGranted, onDenied, "
                        + "{enableHighAccuracy:true})",
                "location_permission_prompt", UserFriendlyElementName.LOCATION_PERMISSION_INDICATOR,
                true /* grant */);
    }

    /**
     * Test navigate to 2D page and launch the Notifications dialog.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "RenderTest"})
    public void testNotificationPermissionPrompt()
            throws InterruptedException, TimeoutException, IOException {
        permissionPromptTestImpl("Notification.requestPermission(onGranted, onDenied)",
                "notification_permission_prompt", UserFriendlyElementName.NONE, true /* grant */);
    }
}
