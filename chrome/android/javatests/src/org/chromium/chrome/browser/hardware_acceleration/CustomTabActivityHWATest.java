// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestBase;

/**
 * Tests that CustomTabActivity is hardware accelerated only high-end devices.
 */
public class CustomTabActivityHWATest extends CustomTabActivityTestBase {

    @SmallTest
    public void testHardwareAcceleration() throws Exception {
        CustomTabActivity activity = getActivity();
        Utils.assertHardwareAcceleration(activity);
    }
}
