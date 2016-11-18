// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_NON_DAYDREAM;

import android.os.SystemClock;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;

/**
 * Instrumentation tests for VR Shell (Chrome VR)
 */
public class VrShellTest extends ChromeTabbedActivityTestBase {
    private VrShellDelegate mDelegate;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDelegate = getActivity().getVrShellDelegate();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void forceEnterVr() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDelegate.enterVRIfNecessary();
            }
        });
    }

    private void forceExitVr() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDelegate.exitVRIfNecessary(false);
            }
        });
    }

    private void testEnterExitVrMode(boolean supported) {
        forceEnterVr();
        // TODO(bsheedy): Remove sleep once crbug.com/644533 is fixed
        SystemClock.sleep(1500);
        if (supported) {
            assertTrue(mDelegate.isInVR());
        } else {
            assertFalse(mDelegate.isInVR());
        }
        forceExitVr();
        assertFalse(mDelegate.isInVR());
    }

    /**
     * Verifies that browser successfully enters and exits VR mode when told to
     * on Daydream-ready devices. Requires that the phone is unlocked.
     */
    @Restriction(RESTRICTION_TYPE_DAYDREAM)
    @MediumTest
    public void testEnterExitVrModeSupported() {
        testEnterExitVrMode(true);
    }

    /**
     * Verifies that browser does not enter VR mode on Non-Daydream-ready devices.
     */
    @Restriction(RESTRICTION_TYPE_NON_DAYDREAM)
    @MediumTest
    public void testEnterExitVrModeUnsupported() {
        testEnterExitVrMode(false);
    }
}
