// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_HAS_CAMERA;

import android.support.v7.widget.SwitchCompat;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;

/**
 * Test suite for media permissions requests.
 */
public class MediaPermissionsTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String TEST_FILE = "/content/test/data/android/media_permissions.html";

    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    /**
     * Waits till the media JavaScript callback is called the specified number of times.
     */
    private class MediaPermissionsUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private int mExpectedCount;
        private String mPrefix;

        public MediaPermissionsUpdateWaiter(String prefix) {
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

    public MediaPermissionsTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        InfoBarContainer container =
                getActivity().getTabModelSelector().getCurrentTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);

        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Verify asking for microphone creates an InfoBar and works when the permission is granted.
     * @throws Exception
     */
    @Smoke
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add(ChromeSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testMicrophoneMediaPermissionsPlumbing() throws Exception {
        testMediaPermissionsPlumbing("Mic count:", "initiate_getMicrophone()", 1, false, false);
    }

    /**
     * Verify asking for camera creates an InfoBar and works when the permission is granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add(ChromeSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testCameraMediaPermissionsPlumbing() throws Exception {
        testMediaPermissionsPlumbing("Camera count:", "initiate_getCamera()", 1, false, false);
    }

    /**
     * Verify asking for both mic and camera creates a combined InfoBar and works when the
     * permissions are granted.
     * @throws Exception
     */
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add(ChromeSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testCombinedMediaPermissionsPlumbing() throws Exception {
        testMediaPermissionsPlumbing("Combined count:", "initiate_getCombined()", 1, false, false);
    }

    /**
     * Verify microphone prompts with a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=DisplayPersistenceToggleInPermissionPrompts")
    @Feature({"MediaPermissions"})
    @RetryOnFailure
    public void testMicrophonePersistenceOn() throws Exception {
        testMediaPermissionsPlumbing("Mic count:", "initiate_getMicrophone()", 1, true, false);
    }

    /**
     * Verify microphone prompts with a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled off.
     * @throws Exception
     */
    //@MediumTest
    //@CommandLineFlags.Add("enable-features=DisplayPersistenceToggleInPermissionPrompts")
    //@Feature({"MediaPermissions"})
    @DisabledTest
    public void testMicrophonePersistenceOff() throws Exception {
        testMediaPermissionsPlumbing("Mic count:", "initiate_getMicrophone()", 1, true, true);
    }

    /**
     * Verify camera prompts with a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=DisplayPersistenceToggleInPermissionPrompts")
    @Feature({"MediaPermissions"})
    @Restriction(RESTRICTION_TYPE_HAS_CAMERA)
    @RetryOnFailure
    public void testCameraPersistenceOn() throws Exception {
        testMediaPermissionsPlumbing("Camera count:", "initiate_getCamera()", 1, true, false);
    }

    /**
     * Verify camera prompts with a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled off.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=DisplayPersistenceToggleInPermissionPrompts")
    @Restriction(RESTRICTION_TYPE_HAS_CAMERA)
    @Feature({"MediaPermissions"})
    public void testCameraPersistenceOff() throws Exception {
        testMediaPermissionsPlumbing("Camera count:", "initiate_getCamera()", 1, true, true);
    }

    /**
     * Verify combined prompts show a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled on.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=DisplayPersistenceToggleInPermissionPrompts")
    @Feature({"MediaPermissions"})
    public void testCombinedPersistenceOn() throws Exception {
        testMediaPermissionsPlumbing("Combined count:", "initiate_getCombined()", 1, true, false);
    }

    /**
     * Verify combined prompts show a persistence toggle if that feature is enabled. Check the
     * switch appears and that permission is granted with it toggled of.
     * @throws Exception
     */
    @MediumTest
    @CommandLineFlags.Add("enable-features=DisplayPersistenceToggleInPermissionPrompts")
    @Feature({"MediaPermissions"})
    public void testCombinedPersistenceOff() throws Exception {
        testMediaPermissionsPlumbing("Combined count:", "initiate_getCombined()", 1, true, true);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void testMediaPermissionsPlumbing(String prefix, String script, int numUpdates,
            boolean expectsSwitch, boolean toggleSwitch) throws Exception {
        final String url = mTestServer.getURL(TEST_FILE);

        Tab tab = getActivity().getActivityTab();
        MediaPermissionsUpdateWaiter updateWaiter = new MediaPermissionsUpdateWaiter(prefix);
        tab.addObserver(updateWaiter);

        loadUrl(url);
        runJavaScriptCodeInCurrentTab(script);
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());
        InfoBar infobar = getInfoBars().get(0);

        SwitchCompat persistSwitch = (SwitchCompat) infobar.getView().findViewById(
                R.id.permission_infobar_persist_toggle);
        if (expectsSwitch) {
            assertNotNull(persistSwitch);
            assertTrue(persistSwitch.isChecked());
            if (toggleSwitch) {
                TouchCommon.singleClickView(persistSwitch);
                waitForCheckedState(persistSwitch, false);
            }
        } else {
            assertNull(persistSwitch);
        }

        assertTrue("OK button wasn't found", InfoBarUtil.clickPrimaryButton(infobar));
        updateWaiter.waitForNumUpdates(numUpdates);

        tab.removeObserver(updateWaiter);
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
}
