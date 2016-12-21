// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestBase;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;

/**
 * Tests that CustomTabActivity is hardware accelerated only high-end devices.
 */
public class CustomTabActivityHWATest extends CustomTabActivityTestBase {

    @SmallTest
    @RetryOnFailure
    public void testHardwareAcceleration() throws Exception {
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), "about:blank"));
        Utils.assertHardwareAcceleration(getActivity());
    }
}
