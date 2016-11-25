// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Test suite for media permissions requests.
 */
@RetryOnFailure
public class MediaTest extends PermissionTestCaseBase {
    private static final String FAKE_DEVICE = ChromeSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM;
    private static final String TEST_FILE = "/content/test/data/android/media_permissions.html";

    public MediaTest() {}

    private void testMediaPermissionsPlumbing(String prefix, String script, int numUpdates,
            boolean withGesture, boolean isDialog, boolean hasSwitch, boolean toggleSwitch)
            throws Exception {
        Tab tab = getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter = new PermissionUpdateWaiter(prefix);
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, TEST_FILE, script, numUpdates, withGesture, isDialog, hasSwitch,
                toggleSwitch);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify asking for microphone creates an InfoBar and works when the permission is granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add(FAKE_DEVICE)
    public void testMicrophonePermissionsPlumbingInfoBar() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, false, false, false, false);
    }

    /**
     * Verify asking for microphone creates a dialog and works when the permission is granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + MODAL_FLAG})
    public void testMicrophoneMediaPermissionsPlumbingDialog() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, true, true, false, false);
    }

    /**
     * Verify asking for camera creates an InfoBar and works when the permission is granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add(FAKE_DEVICE)
    public void testCameraPermissionsPlumbingInfoBar() throws Exception {
        testMediaPermissionsPlumbing(
                "Camera count:", "initiate_getCamera()", 1, false, false, false, false);
    }

    /**
     * Verify asking for camera with no gesture creates an infobar when the modal flag is turned on,
     * and works when the permission is granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + MODAL_FLAG})
    public void testCameraPermissionsPlumbingNoGestureCreatesInfoBar() throws Exception {
        testMediaPermissionsPlumbing(
                "Camera count:", "initiate_getCamera()", 1, false, false, false, false);
    }

    /**
     * Verify asking for both mic and camera creates a combined InfoBar and works when the
     * permissions are granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add(FAKE_DEVICE)
    public void testCombinedPermissionsPlumbing() throws Exception {
        testMediaPermissionsPlumbing(
                "Combined count:", "initiate_getCombined()", 1, false, false, false, false);
    }

    /**
     * Verify asking for both mic and camera creates a combined dialog and works when the
     * permissions are granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + MODAL_FLAG})
    public void testCombinedPermissionsPlumbingDialog() throws Exception {
        testMediaPermissionsPlumbing(
                "Combined count:", "initiate_getCombined()", 1, true, true, false, false);
    }

    /**
     * Verify microphone creates an InfoBar with a persistence toggle if that feature is enabled.
     * Check the switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testMicrophonePersistenceOnInfoBar() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, false, false, true, false);
    }

    /**
     * Verify microphone creates an InfoBar with a persistence toggle if that feature is enabled.
     * Check the switch appears and that permission is granted with it toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testMicrophonePersistenceOffInfoBar() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, false, false, true, true);
    }

    /**
     * Verify microphone creates a dialog with a persistence toggle if that feature is enabled.
     * Check the switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + MODAL_TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testMicrophonePersistenceOnDialog() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, true, true, true, false);
    }

    /**
     * Verify microphone creates a dialog with a persistence toggle if that feature is enabled.
     * Check the switch appears and that permission is granted with it toggled off.
     * @throws Exception
     */
    /*
     * @MediumTest
     * @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + MODAL_TOGGLE_FLAG})
     * BUG=https://crbug.com/668700
     */
    @DisabledTest
    @Feature({"MediaPermissions"})
    public void testMicrophonePersistenceOffDialog() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, true, true, true, true);
    }

    /**
     * Verify camera prompts with a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testCameraPersistenceOn() throws Exception {
        testMediaPermissionsPlumbing(
                "Camera count:", "initiate_getCamera()", 1, false, false, true, false);
    }

    /**
     * Verify camera prompts with a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testCameraPersistenceOff() throws Exception {
        testMediaPermissionsPlumbing(
                "Camera count:", "initiate_getCamera()", 1, false, false, true, true);
    }

    /**
     * Verify combined creates an InfoBar with a persistence toggle if that feature is enabled.
     * Check the switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testCombinedPersistenceOnInfoBar() throws Exception {
        testMediaPermissionsPlumbing(
                "Combined count:", "initiate_getCombined()", 1, false, false, true, false);
    }

    /**
     * Verify combined creates an InfoBar with a persistence toggle if that feature is enabled.
     * Check the switch appears and that permission is granted with it toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testCombinedPersistenceOffInfoBar() throws Exception {
        testMediaPermissionsPlumbing(
                "Combined count:", "initiate_getCombined()", 1, false, false, true, true);
    }

    /**
     * Verify combined creates a dialog with a persistence toggle if that feature is enabled. Check
     * the switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + MODAL_TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testCombinedPersistenceOnDialog() throws Exception {
        testMediaPermissionsPlumbing(
                "Combined count:", "initiate_getCombined()", 1, true, true, true, false);
    }

    /**
     * Verify combined creates a dialog with a persistence toggle if that feature is enabled. Check
     * the switch appears and that permission is granted with it toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + MODAL_TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testCombinedPersistenceOffDialog() throws Exception {
        testMediaPermissionsPlumbing(
                "Combined count:", "initiate_getCombined()", 1, false, false, true, true);
    }
}
