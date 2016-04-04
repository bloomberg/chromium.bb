// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.document.DocumentModeTestBase;

/**
 * Tests that DocumentActivity is hardware accelerated only high-end devices.
 */
@DisabledTest
public class DocumentActivityHWATest extends DocumentModeTestBase {

    @SmallTest
    public void testHardwareAcceleration() throws Exception {
        Utils.assertHardwareAcceleration(startAndWaitActivity(false));
    }

    @SmallTest
    public void testHardwareAccelerationIncognito() throws Exception {
        Utils.assertHardwareAcceleration(startAndWaitActivity(true));
    }

    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @SmallTest
    public void testNoRenderThread() throws Exception {
        startAndWaitActivity(false);
        Utils.assertNoRenderThread();
    }

    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @SmallTest
    public void testNoRenderThreadIncognito() throws Exception {
        startAndWaitActivity(true);
        Utils.assertNoRenderThread();
    }

    private DocumentActivity startAndWaitActivity(boolean incognito) throws Exception {
        launchViaViewIntent(incognito, URL_1, "Page 1");
        return (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }
}
