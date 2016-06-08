// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.os.Bundle;

/**
 * Class to put our custom task information into the task bundle.
 */
public class TaskExtrasPacker {
    /** Bundle key for the timestamp in milliseconds when the request started. */
    public static final String SCHEDULED_TIME_TAG = "Date";

    /** Puts current time into the input bundle. */
    public static void packTimeInBundle(Bundle bundle) {
        bundle.putLong(SCHEDULED_TIME_TAG, System.currentTimeMillis());
    }

    /** Extracts the time we put into the bundle. */
    public static long unpackTimeFromBundle(Bundle bundle) {
        return bundle.getLong(SCHEDULED_TIME_TAG);
    }
}
