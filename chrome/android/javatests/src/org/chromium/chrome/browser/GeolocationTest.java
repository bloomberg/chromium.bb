// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;
import android.support.v7.widget.SwitchCompat;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.permissions.PermissionDialogController;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.device.geolocation.LocationProviderFactory;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Test suite for Geo-Location functionality.
 *
 * These tests rely on the device being specially setup (which the bots do automatically):
 * - Global location is enabled.
 * - Google location is enabled.
 */
@RetryOnFailure
public class GeolocationTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String LOCATION_PROVIDER_MOCK = "locationProviderMock";
    private static final double LATITUDE = 51.01;
    private static final double LONGITUDE = 0.23;
    private static final float ACCURACY = 10;
    private static final String TEST_FILE = "/content/test/data/android/geolocation.html";

    private static final String MODAL_FLAG = "ModalPermissionPrompts";
    private static final String TOGGLE_FLAG = "DisplayPersistenceToggleInPermissionPrompts";
    private static final String MODAL_TOGGLE_FLAG = MODAL_FLAG + "," + TOGGLE_FLAG;
    private static final String NO_GESTURE_FEATURE =
            "enable-features=ModalPermissionPrompts<ModalPrompts";
    private static final String FORCE_FIELDTRIAL = "force-fieldtrials=ModalPrompts/Group1";
    private static final String FORCE_FIELDTRIAL_PARAMS =
            "force-fieldtrial-params=ModalPrompts.Group1:require_gesture/false";

    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    /**
     * Waits till the geolocation JavaScript callback is called the specified number of times.
     */
    private class GeolocationUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private int mExpectedCount;

        public GeolocationUpdateWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String expectedTitle = "Count:" + mExpectedCount;
            if (getActivity().getActivityTab().getTitle().equals(expectedTitle)) {
                mCallbackHelper.notifyCalled();
            }
        }

        public void waitForNumUpdates(int numUpdates) throws Exception {
            mExpectedCount = numUpdates;
            mCallbackHelper.waitForCallback(0);
        }
    }

    /**
     * Criteria class to detect whether the permission dialog is shown.
     */
    private static class DialogShownCriteria extends Criteria {
        private AlertDialog mDialog;

        public DialogShownCriteria(String error) {
            super(error);
        }

        public AlertDialog getDialog() {
            return mDialog;
        }

        @Override
        public boolean isSatisfied() {
            try {
                return ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        mDialog = PermissionDialogController.getInstance()
                                          .getCurrentDialogForTesting();
                        return mDialog != null;
                    }
                });
            } catch (ExecutionException e) {
                return false;
            }
        }
    }

    public GeolocationTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        InfoBarContainer container =
                getActivity().getTabModelSelector().getCurrentTab().getInfoBarContainer();
        mListener = new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);

        LocationProviderFactory.setLocationProviderImpl(new MockLocationProvider());

        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Simulates clicking a button on an AlertDialog.
     */
    private void clickButton(final AlertDialog dialog, final int button) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                dialog.getButton(button).performClick();
            }
        });
    }

    /**
     * Runs a geolocation test that permits location access.
     * @param updateWaiter  The update waiter to wait for callbacks.
     * @param javascript    The JS function to run in the current tab to execute the test.
     * @param nUpdates      How many updates to wait for.
     * @param withGeature   True if we require a user gesture to trigger the prompt.
     * @param isDialog      True if we are expecting a permission dialog, false for an infobar.
     * @param hasSwitch     True if we are expecting a persistence switch, false otherwise.
     * @param toggleSwitch  True if we should toggle the switch off, false otherwise.
     * @throws Exception
     */
    private void runAllowTest(GeolocationUpdateWaiter updateWaiter, String javascript, int nUpdates,
            boolean withGesture, boolean isDialog, boolean hasSwitch, boolean toggleSwitch)
            throws Exception {
        final String url = mTestServer.getURL(TEST_FILE);
        loadUrl(url);

        if (withGesture) {
            runJavaScriptCodeInCurrentTab("functionToRun = '" + javascript + "'");
            singleClickView(getActivity().getActivityTab().getView());
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }

        if (isDialog) {
            // We need to have a user gesture for the modal to appear. Use a click listener to
            // trigger the JavaScript.
            DialogShownCriteria criteria = new DialogShownCriteria("Dialog not shown");
            CriteriaHelper.pollInstrumentationThread(criteria);
            replyToDialogAndWaitForUpdates(
                    updateWaiter, criteria.getDialog(), nUpdates, true, hasSwitch, toggleSwitch);
        } else {
            replyToInfoBarAndWaitForUpdates(updateWaiter, nUpdates, true, hasSwitch, toggleSwitch);
        }
    }

    private void replyToInfoBarAndWaitForUpdates(GeolocationUpdateWaiter updateWaiter, int nUpdates,
            boolean allow, boolean hasSwitch, boolean toggleSwitch) throws Exception {
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());
        InfoBar infobar = getInfoBars().get(0);
        assertNotNull(infobar);

        if (hasSwitch) {
            SwitchCompat persistSwitch = (SwitchCompat) infobar.getView().findViewById(
                    R.id.permission_infobar_persist_toggle);
            assertNotNull(persistSwitch);
            assertTrue(persistSwitch.isChecked());
            if (toggleSwitch) {
                singleClickView(persistSwitch);
                waitForCheckedState(persistSwitch, false);
            }
        }

        if (allow) {
            assertTrue("Allow button wasn't found", InfoBarUtil.clickPrimaryButton(infobar));
        } else {
            assertTrue("Block button wasn't found", InfoBarUtil.clickSecondaryButton(infobar));
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    private void replyToDialogAndWaitForUpdates(GeolocationUpdateWaiter updateWaiter,
            AlertDialog dialog, int nUpdates, boolean allow, boolean hasSwitch,
            boolean toggleSwitch) throws Exception {
        if (hasSwitch) {
            SwitchCompat persistSwitch =
                    (SwitchCompat) dialog.findViewById(R.id.permission_dialog_persist_toggle);
            assertNotNull(persistSwitch);
            assertTrue(persistSwitch.isChecked());
            if (toggleSwitch) {
                singleClickView(persistSwitch);
                waitForCheckedState(persistSwitch, false);
            }
        }

        if (allow) {
            clickButton(dialog, DialogInterface.BUTTON_POSITIVE);
        } else {
            clickButton(dialog, DialogInterface.BUTTON_NEGATIVE);
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    private void waitForCheckedState(final SwitchCompat persistSwitch, boolean isChecked)
            throws InterruptedException {
        CriteriaHelper.pollUiThread(Criteria.equals(isChecked, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return persistSwitch.isChecked();
            }
        }));
    }

    /**
     * Verify Geolocation creates an InfoBar and receives a mock location.
     * @throws Exception
     */
    @Smoke
    @MediumTest
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedInfoBar() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_getCurrentPosition()", 1, false, false, false, false);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates a dialog and receives a mock location.
     * @throws Exception
     */
    @Smoke
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + MODAL_FLAG)
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedDialog() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_getCurrentPosition()", 1, true, true, false, false);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates a dialog and receives a mock location when dialogs are explicitly
     * enabled and permitted to trigger without a gesture.
     * @throws Exception
     */
    @Smoke
    @MediumTest
    @CommandLineFlags.Add({NO_GESTURE_FEATURE, FORCE_FIELDTRIAL, FORCE_FIELDTRIAL_PARAMS})
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedDialogNoGesture() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_getCurrentPosition()", 1, false, true, false, false);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation shows an infobar and receives a mock location if the modal flag is on but
     * no user gesture is specified.
     * @throws Exception
     */
    @Smoke
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + MODAL_FLAG)
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbingAllowedNoGestureShowsInfoBar() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_getCurrentPosition()", 1, false, false, false, false);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates an InfoBar and receives multiple locations.
     * @throws Exception
     */
    @MediumTest
    @Feature({"Location"})
    public void testGeolocationWatchInfoBar() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_watchPosition()", 2, false, false, false, false);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates a dialog and receives multiple locations.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + MODAL_FLAG)
    @Feature({"Location"})
    public void testGeolocationWatchDialog() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_watchPosition()", 2, true, true, false, false);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates an infobar with a persistence toggle if that feature is enabled.
     * Check the switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + TOGGLE_FLAG)
    @Feature({"Location"})
    public void testGeolocationPersistenceAllowedInfoBar() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_getCurrentPosition()", 1, false, false, true, false);
        tab.removeObserver(updateWaiter);
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
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_getCurrentPosition()", 1, true, true, true, false);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates an infobar with a persistence toggle if that feature is enabled.
     * Check the switch toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=" + TOGGLE_FLAG)
    @Feature({"Location"})
    public void testGeolocationPersistenceOffAllowedInfoBar() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_getCurrentPosition()", 1, false, false, true, true);

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
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_getCurrentPosition()", 1, true, true, true, true);

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
    @CommandLineFlags.Add("enable-features=" + TOGGLE_FLAG)
    @Feature({"Location"})
    public void testGeolocationWatchPersistenceOffAllowedInfoBar() throws Exception {
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_watchPosition()", 2, false, false, true, true);
        tab.removeObserver(updateWaiter);
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
        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, "initiate_watchPosition()", 2, true, true, true, true);
        tab.removeObserver(updateWaiter);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
