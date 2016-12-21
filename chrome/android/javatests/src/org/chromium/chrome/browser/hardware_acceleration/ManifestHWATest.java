// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.chrome.browser.ChromeActivity;

/**
 * Hardware acceleration-related manifest tests.
 */
public class ManifestHWATest extends InstrumentationTestCase {

    @SmallTest
    public void testAccelerationDisabled() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        PackageInfo info = context.getPackageManager().getPackageInfo(
                context.getApplicationInfo().packageName,
                PackageManager.GET_ACTIVITIES);
        for (ActivityInfo activityInfo : info.activities) {
            String activityName = activityInfo.targetActivity != null
                    ? activityInfo.targetActivity
                    : activityInfo.name;
            Class<?> activityClass = Class.forName(activityName);
            if (ChromeActivity.class.isAssignableFrom(activityClass)) {
                // Every activity derived from ChromeActivity must disable hardware
                // acceleration in the manifest.
                assertTrue(0 == (activityInfo.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED));
            }
        }
    }
}
