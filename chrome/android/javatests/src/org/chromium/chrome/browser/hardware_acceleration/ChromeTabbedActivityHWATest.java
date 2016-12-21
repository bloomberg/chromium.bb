// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;

/**
 * Tests that ChromeTabbedActivity is hardware accelerated only high-end devices.
 */
public class ChromeTabbedActivityHWATest extends ChromeTabbedActivityTestBase {
    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    @RetryOnFailure
    public void testHardwareAcceleration() throws Exception {
        Utils.assertHardwareAcceleration(getActivity());
    }
}
