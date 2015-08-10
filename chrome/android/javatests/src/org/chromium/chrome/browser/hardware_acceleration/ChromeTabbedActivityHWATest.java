// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;

/**
 * Tests that ChromeTabbedActivity is hardware accelerated only high-end devices.
 */
public class ChromeTabbedActivityHWATest extends ChromeTabbedActivityTestBase {

    private static final String URL = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <title>Test Page</title>"
            + "  </head>"
            + "  <body style='margin: 0em; background: #ffff00;'></body>"
            + "</html>");

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(URL);
    }

    @SmallTest
    public void testHardwareAcceleration() throws Exception {
        Utils.assertHardwareAcceleration(getActivity());
    }

    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @SmallTest
    public void testNoRenderThread() throws Exception {
        Utils.assertNoRenderThread();
    }
}
