// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;
import android.support.v7.widget.SwitchCompat;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Test case base for permissions UI testing on Android.
 *
 * This class allows for easy testing of permissions infobar and dialog prompts. Writing a test
 * simply requires a HTML file containing JavaScript methods which trigger a permission prompt. The
 * methods should update the page's title with <prefix>: <count>, where <count> is the number of
 * updates expected (usually 1, although some APIs like Geolocation's watchPosition may trigger
 * callbacks repeatedly).
 *
 * Subclasses may then call runAllowTest to start a test server, navigate to the provided HTML page,
 * and run the JavaScript method. The permission will be granted, and the test will verify that the
 * page title is updated as expected.
 *
 * runAllowTest has several parameters to specify the conditions of the test, including whether
 * a persistence toggle is expected, whether it should be explicitly toggled, whether to trigger the
 * JS call with a gesture, and whether an infobar or a dialog is expected.
 */
public class PermissionTestCaseBase extends ChromeActivityTestCaseBase<ChromeActivity> {
    protected static final String MODAL_FLAG = "ModalPermissionPrompts";
    protected static final String TOGGLE_FLAG = "DisplayPersistenceToggleInPermissionPrompts";
    protected static final String MODAL_TOGGLE_FLAG = MODAL_FLAG + "," + TOGGLE_FLAG;
    protected static final String NO_GESTURE_FEATURE =
            "enable-features=ModalPermissionPrompts<ModalPrompts";
    protected static final String FORCE_FIELDTRIAL = "force-fieldtrials=ModalPrompts/Group1";
    protected static final String FORCE_FIELDTRIAL_PARAMS =
            "force-fieldtrial-params=ModalPrompts.Group1:require_gesture/false";

    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    /**
     * Waits till a JavaScript callback which updates the page title is called the specified number
     * of times. The page title is expected to be of the form <prefix>: <count>.
     */
    protected class PermissionUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mPrefix;
        private int mExpectedCount;

        public PermissionUpdateWaiter(String prefix) {
            mCallbackHelper = new CallbackHelper();
            mPrefix = prefix;
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String expectedTitle = mPrefix + mExpectedCount;
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

    public PermissionTestCaseBase() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        InfoBarContainer container =
                getActivity().getTabModelSelector().getCurrentTab().getInfoBarContainer();
        mListener = new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);
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
     * Runs a permission prompt test that grants the permission and expects the page title to be
     * updated in response.
     * @param updateWaiter  The update waiter to wait for callbacks. Should be added as an observer
     *                      to the current tab prior to calling this method.
     * @param javascript    The JS function to run in the current tab to execute the test and update
     *                      the page title.
     * @param nUpdates      How many updates of the page title to wait for.
     * @param withGeature   True if we require a user gesture to trigger the prompt.
     * @param isDialog      True if we are expecting a permission dialog, false for an infobar.
     * @param hasSwitch     True if we are expecting a persistence switch, false otherwise.
     * @param toggleSwitch  True if we should toggle the switch off, false otherwise.
     * @throws Exception
     */
    protected void runAllowTest(PermissionUpdateWaiter updateWaiter, final String url,
            String javascript, int nUpdates, boolean withGesture, boolean isDialog,
            boolean hasSwitch, boolean toggleSwitch) throws Exception {
        final String test_url = mTestServer.getURL(url);
        loadUrl(test_url);

        if (withGesture) {
            runJavaScriptCodeInCurrentTab("functionToRun = '" + javascript + "'");
            singleClickView(getActivity().getActivityTab().getView());
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }

        if (isDialog) {
            DialogShownCriteria criteria = new DialogShownCriteria("Dialog not shown");
            CriteriaHelper.pollUiThread(criteria);
            replyToDialogAndWaitForUpdates(
                    updateWaiter, criteria.getDialog(), nUpdates, true, hasSwitch, toggleSwitch);
        } else {
            replyToInfoBarAndWaitForUpdates(updateWaiter, nUpdates, true, hasSwitch, toggleSwitch);
        }
    }

    /**
     * Replies to an infobar permission prompt, optionally checking for the presence of a
     * persistence switch and toggling it. Waits for a provided number of updates to the page title
     * in response.
     */
    private void replyToInfoBarAndWaitForUpdates(PermissionUpdateWaiter updateWaiter, int nUpdates,
            boolean allow, boolean hasSwitch, boolean toggleSwitch) throws Exception {
        mListener.addInfoBarAnimationFinished("InfoBar not added.");
        InfoBar infobar = getInfoBars().get(0);
        assertNotNull(infobar);

        if (hasSwitch) {
            SwitchCompat persistSwitch = (SwitchCompat) infobar.getView().findViewById(
                    R.id.permission_infobar_persist_toggle);
            checkAndToggleSwitch(persistSwitch, toggleSwitch);
        }

        if (allow) {
            assertTrue("Allow button wasn't found", InfoBarUtil.clickPrimaryButton(infobar));
        } else {
            assertTrue("Block button wasn't found", InfoBarUtil.clickSecondaryButton(infobar));
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    /**
     * Replies to a dialog permission prompt, optionally checking for the presence of a
     * persistence switch and toggling it. Waits for a provided number of updates to the page title
     * in response.
     */
    private void replyToDialogAndWaitForUpdates(PermissionUpdateWaiter updateWaiter,
            AlertDialog dialog, int nUpdates, boolean allow, boolean hasSwitch,
            boolean toggleSwitch) throws Exception {
        if (hasSwitch) {
            SwitchCompat persistSwitch =
                    (SwitchCompat) dialog.findViewById(R.id.permission_dialog_persist_toggle);
            checkAndToggleSwitch(persistSwitch, toggleSwitch);
        }

        if (allow) {
            clickButton(dialog, DialogInterface.BUTTON_POSITIVE);
        } else {
            clickButton(dialog, DialogInterface.BUTTON_NEGATIVE);
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    private void checkAndToggleSwitch(final SwitchCompat persistSwitch, boolean toggleSwitch) {
        assertNotNull(persistSwitch);
        assertTrue(persistSwitch.isChecked());
        if (toggleSwitch) {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    persistSwitch.toggle();
                }
            });
        }
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
