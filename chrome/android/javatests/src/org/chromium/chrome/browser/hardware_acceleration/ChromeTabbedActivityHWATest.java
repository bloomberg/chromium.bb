// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.DocumentModeTestBase;

/**
 * Tests that ChromeTabbedActivity is hardware accelerated only high-end devices.
 */
@CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
public class ChromeTabbedActivityHWATest extends DocumentModeTestBase {

    @SmallTest
    public void testHardwareAcceleration() throws Exception {
        Utils.assertHardwareAcceleration(startAndWaitActivity());
    }

    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @SmallTest
    public void testNoRenderThread() throws Exception {
        startAndWaitActivity();
        Utils.assertNoRenderThread();
    }

    private ChromeTabbedActivity startAndWaitActivity() throws Exception {
        ChromeTabbedActivity activity = startTabbedActivity(URL_1);
        waitForFullLoad(activity, "Page 1");
        return activity;
    }
}
