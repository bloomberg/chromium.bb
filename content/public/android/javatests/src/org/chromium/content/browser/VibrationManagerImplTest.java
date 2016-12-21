// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Vibrator;
import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.device.vibration.VibrationManagerImpl;

/**
 * Tests java implementation of VibrationManager mojo service on android.
 */
public class VibrationManagerImplTest extends ContentShellTestBase {
    private static final String URL_VIBRATOR_VIBRATE = UrlUtils.encodeHtmlDataUri("<html><body>"
            + "  <script type=\"text/javascript\">"
            + "    navigator.vibrate(3000);"
            + "  </script>"
            + "</body></html>");
    private static final String URL_VIBRATOR_CANCEL = UrlUtils.encodeHtmlDataUri("<html><body>"
            + "  <script type=\"text/javascript\">"
            + "    navigator.vibrate(10000);"
            + "    navigator.vibrate(0);"
            + "  </script>"
            + "</body></html>");

    private FakeAndroidVibratorWrapper mFakeWrapper;

    // Override AndroidVibratorWrapper API to record the calling.
    private static class FakeAndroidVibratorWrapper
            extends VibrationManagerImpl.AndroidVibratorWrapper {
        // Record the parameters of vibrate() and cancel().
        public long mMilliSeconds;
        public boolean mCancelled;

        protected FakeAndroidVibratorWrapper() {
            super();
            mMilliSeconds = -1;
            mCancelled = false;
        }

        @Override
        public void vibrate(Vibrator vibrator, long milliseconds) {
            mMilliSeconds = milliseconds;
        }

        @Override
        public void cancel(Vibrator vibrator) {
            mCancelled = true;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl("about:blank");
        waitForActiveShellToBeDoneLoading();

        mFakeWrapper = new FakeAndroidVibratorWrapper();
        VibrationManagerImpl.setVibratorWrapperForTesting(mFakeWrapper);
        assertEquals(-1, mFakeWrapper.mMilliSeconds);
        assertFalse(mFakeWrapper.mCancelled);
    }

    /**
     * Inject our fake wrapper into VibrationManagerImpl,
     * load the webpage which will request vibrate for 3000 milliseconds,
     * the fake wrapper vibrate() should be called and 3000 milliseconds should be recorded
     * correctly.
     */
    // @MediumTest
    // @Feature({"Vibration"})
    @DisabledTest
    public void testVibrate() throws Throwable {
        loadNewShell(URL_VIBRATOR_VIBRATE);

        // Waits until VibrationManagerImpl.Vibrate() got called.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mFakeWrapper.mMilliSeconds != -1;
            }
        });

        assertEquals(
                "Did not get vibrate mMilliSeconds correctly", 3000, mFakeWrapper.mMilliSeconds);
    }

    /**
     * Inject our fake wrapper into VibrationManagerImpl,
     * load the webpage which will request vibrate and then request cancel,
     * the fake wrapper cancel() should be called.
     */
    @MediumTest
    @Feature({"Vibration"})
    public void testCancel() throws Throwable {
        loadNewShell(URL_VIBRATOR_CANCEL);

        // Waits until VibrationManagerImpl.Cancel() got called.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mFakeWrapper.mCancelled;
            }
        });

        assertTrue("Did not get cancelled", mFakeWrapper.mCancelled);
    }
}
