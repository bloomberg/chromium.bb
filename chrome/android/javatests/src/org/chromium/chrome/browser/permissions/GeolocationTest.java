// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;

import org.junit.Assert;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.device.geolocation.LocationProviderFactory;
import org.chromium.device.geolocation.MockLocationProvider;

/**
 * Test suite for Geo-Location functionality.
 *
 * These tests rely on the device being specially setup (which the bots do automatically):
 * - Global location is enabled.
 * - Google location is enabled.
 */
@RetryOnFailure
public class GeolocationTest extends PermissionTestCaseBase {
    private static final String LOCATION_PROVIDER_MOCK = "locationProviderMock";
    private static final double LATITUDE = 51.01;
    private static final double LONGITUDE = 0.23;
    private static final float ACCURACY = 10;
    private static final String TEST_FILE = "/content/test/data/android/geolocation.html";
    private static final String PERSIST_ACCEPT_HISTOGRAM =
            "Permissions.Prompt.Accepted.Persisted.Geolocation";

    public GeolocationTest() {}

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderFactory.setLocationProviderImpl(new MockLocationProvider());
    }

    private void runTest(String javascript, int nUpdates, boolean withGesture, boolean isDialog,
            boolean hasSwitch, boolean toggleSwitch) throws Exception {
        Tab tab = getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter = new PermissionUpdateWaiter("Count:");
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, TEST_FILE, javascript, nUpdates, withGesture, isDialog,
                hasSwitch, toggleSwitch);
        tab.removeObserver(updateWaiter);
        if (hasSwitch) {
            int bucket = toggleSwitch ? 0 : 1;
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            PERSIST_ACCEPT_HISTOGRAM, bucket));
        }
    }

    /**
     * Verify Geolocation creates an InfoBar and receives a mock location.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("disable-features=" + MODAL_FLAG)
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedInfoBar() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, false, false, false, false);
    }

    /**
     * Verify Geolocation creates a dialog and receives a mock location.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + MODAL_FLAG)
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedDialog() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, true, true, false, false);
    }

    /**
     * Verify Geolocation creates a dialog and receives a mock location when dialogs are
     * enabled and there is no user gesture.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + MODAL_FLAG)
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedDialogNoGesture() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, false, true, false, false);
    }

    /**
     * Verify Geolocation creates an InfoBar and receives multiple locations.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("disable-features=" + MODAL_FLAG)
    @Feature({"Location"})
    public void testGeolocationWatchInfoBar() throws Exception {
        runTest("initiate_watchPosition()", 2, false, false, false, false);
    }

    /**
     * Verify Geolocation creates a dialog and receives multiple locations.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + MODAL_FLAG)
    @Feature({"Location"})
    public void testGeolocationWatchDialog() throws Exception {
        runTest("initiate_watchPosition()", 2, true, true, false, false);
    }

    /**
     * Verify Geolocation creates an infobar with a persistence toggle if that feature is enabled.
     * Check the switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({"enable-features=" + TOGGLE_FLAG, "disable-features=" + MODAL_FLAG})
    @Feature({"Location"})
    public void testGeolocationPersistenceAllowedInfoBar() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, false, false, true, false);
    }

    /**
     * Verify Geolocation creates a dialog with a persistence toggle if both features are enabled.
     * Check the switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + MODAL_TOGGLE_FLAG)
    @Feature({"Location"})
    public void testGeolocationPersistenceAllowedDialog() throws Exception {
        runTest("initiate_getCurrentPosition()", 1, true, true, true, false);
    }

    /**
     * Verify Geolocation creates an infobar with a persistence toggle if that feature is enabled.
     * Check the switch toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add({"enable-features=" + TOGGLE_FLAG, "disable-features=" + MODAL_FLAG})
    @Feature({"Location"})
    public void testGeolocationPersistenceOffAllowedInfoBar() throws Exception {
        Tab tab = getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter = new PermissionUpdateWaiter("Count:");
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, TEST_FILE, "initiate_getCurrentPosition()", 1, false, false,
                true, true);

        // Ask for permission again and make sure it doesn't prompt again (grant is cached in the
        // Blink layer).
        runJavaScriptCodeInCurrentTab("initiate_getCurrentPosition()");
        updateWaiter.waitForNumUpdates(2);

        // Ask for permission a third time and make sure it doesn't prompt again.
        runJavaScriptCodeInCurrentTab("initiate_getCurrentPosition()");
        updateWaiter.waitForNumUpdates(3);

        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates a dialog with a persistence toggle if that feature is enabled.
     * Check the switch toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + MODAL_TOGGLE_FLAG)
    @Feature({"Location"})
    public void testGeolocationPersistenceOffAllowedDialog() throws Exception {
        Tab tab = getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter = new PermissionUpdateWaiter("Count:");
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, TEST_FILE, "initiate_getCurrentPosition()", 1, true, true, true,
                true);

        // Ask for permission again and make sure it doesn't prompt again (grant is cached in the
        // Blink layer).
        singleClickView(getActivity().getActivityTab().getView());
        updateWaiter.waitForNumUpdates(2);

        // Ask for permission a third time and make sure it doesn't prompt again.
        singleClickView(getActivity().getActivityTab().getView());
        updateWaiter.waitForNumUpdates(3);

        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation prompts once and sends multiple locations with a persistence toggle if
     * that feature is enabled. Use an infobar.
     * @throws Exception
     */
    @LargeTest
    @CommandLineFlags.Add({"enable-features=" + TOGGLE_FLAG, "disable-features=" + MODAL_FLAG})
    @Feature({"Location"})
    public void testGeolocationWatchPersistenceOffAllowedInfoBar() throws Exception {
        runTest("initiate_watchPosition()", 2, false, false, true, true);
    }

    /**
     * Verify Geolocation prompts once and sends multiple locations with a persistence toggle if
     * that feature is enabled. Use a dialog.
     * @throws Exception
     */
    @LargeTest
    @CommandLineFlags.Add("enable-features=" + MODAL_TOGGLE_FLAG)
    @Feature({"Location"})
    public void testGeolocationWatchPersistenceOffAllowedDialog() throws Exception {
        runTest("initiate_watchPosition()", 2, true, true, true, true);
    }
}
