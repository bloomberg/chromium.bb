// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.EmbedContentViewActivity;
import org.chromium.chrome.test.util.ActivityUtils;

/**
 * Tests that EmbedContentViewActivity is hardware accelerated only high-end devices.
 */
public class EmbedContentViewActivityHWATest extends InstrumentationTestCase {

    @SmallTest
    public void testHardwareAcceleration() throws Exception {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                EmbedContentViewActivity.show(getInstrumentation().getTargetContext(),
                        R.string.terms_of_service_title, R.string.chrome_terms_of_service_url);
            }
        };
        EmbedContentViewActivity activity = ActivityUtils.waitForActivity(getInstrumentation(),
                EmbedContentViewActivity.class,
                runnable);
        Utils.assertHardwareAcceleration(activity);
    }
}
