// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.support.test.filters.SmallTest;

import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappActivityTestBase;

/**
 * Tests that WebappActivity is hardware accelerated only high-end devices.
 */
public class WebappActivityHWATest extends WebappActivityTestBase {
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startWebappActivity();
    }

    @SmallTest
    public void testHardwareAcceleration() throws Exception {
        WebappActivity activity = getActivity();
        Utils.assertHardwareAcceleration(activity);
    }
}
