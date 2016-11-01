// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Test suite for media permissions requests.
 */
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
    @Smoke
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add(FAKE_DEVICE)
    public void testMicrophonePermissionsPlumbing() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, false, false, false, false);
    }

    /**
     * Verify asking for camera creates an InfoBar and works when the permission is granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add(FAKE_DEVICE)
    public void testCameraPermissionsPlumbing() throws Exception {
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
     * Verify microphone prompts with a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testMicrophonePersistenceOn() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, false, false, true, false);
    }

    /**
     * Verify microphone prompts with a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testMicrophonePersistenceOff() throws Exception {
        testMediaPermissionsPlumbing(
                "Mic count:", "initiate_getMicrophone()", 1, false, false, true, true);
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
     * Verify combined prompts show a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testCombinedPersistenceOn() throws Exception {
        testMediaPermissionsPlumbing(
                "Combined count:", "initiate_getCombined()", 1, false, false, true, false);
    }

    /**
     * Verify combined prompts show a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled of.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({FAKE_DEVICE, "enable-features=" + TOGGLE_FLAG})
    @Feature({"MediaPermissions"})
    public void testCombinedPersistenceOff() throws Exception {
        testMediaPermissionsPlumbing(
                "Combined count:", "initiate_getCombined()", 1, false, false, true, true);
    }
}
