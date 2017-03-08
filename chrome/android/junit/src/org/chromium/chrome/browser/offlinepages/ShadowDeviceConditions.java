// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Custom shadow for the OfflinePageUtils. */
@Implements(DeviceConditions.class)
public class ShadowDeviceConditions {
    /** Device conditions for testing. */
    private static DeviceConditions sDeviceConditions = new DeviceConditions();

    /** Sets device conditions that will be used in test. */
    public static void setCurrentConditions(DeviceConditions deviceConditions) {
        sDeviceConditions = deviceConditions;
    }

    @Implementation
    public static DeviceConditions getCurrentConditions(Context context) {
        return sDeviceConditions;
    }

    @Implementation
    public static boolean isPowerConnected(Context context) {
        return sDeviceConditions.isPowerConnected();
    }

    @Implementation
    public static int getBatteryPercentage(Context context) {
        return sDeviceConditions.getBatteryPercentage();
    }

    @Implementation
    public static int getNetConnectionType(Context context) {
        return sDeviceConditions.getNetConnectionType();
    }
}
