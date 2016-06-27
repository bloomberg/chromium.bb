// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.app.Activity;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeApplication;

/**
 * The Client that harvests URLs from BLE signals.
 * This class is designed to scan URSs from Bluetooth Low Energy beacons.
 * This class is currently an empty implementation and must be extended by a
 * subclass.
 */
public class PhysicalWebBleClient {
    private static PhysicalWebBleClient sInstance = null;
    private static final String TAG = "PhysicalWeb";

    /**
     * Get a singleton instance of this class.
     * @param chromeApplication An instance of {@link ChromeApplication}, used to get the
     * appropriate PhysicalWebBleClient implementation.
     * @return an instance of this class (or subclass) as decided by the
     * application parameter
     */
    public static PhysicalWebBleClient getInstance(ChromeApplication chromeApplication) {
        if (sInstance == null) {
            sInstance = chromeApplication.createPhysicalWebBleClient();
        }
        return sInstance;
    }

    /**
     * Begin a background subscription to URLs broadcasted from BLE beacons.
     * This currently does nothing and should be overridden by a subclass.
     * @param callback Callback to be run when subscription task is done, regardless of whether it
     *         is successful.
     */
    void backgroundSubscribe(Runnable callback) {
        Log.d(TAG, "background subscribing in empty client");
        if (callback != null) {
            callback.run();
        }
    }

    /**
     * Begin a background subscription to URLs broadcasted from BLE beacons.
     * This currently does nothing and should be overridden by a subclass.
     */
    void backgroundSubscribe() {
        backgroundSubscribe(null);
    }

    /**
     * Cancel a background subscription to URLs broadcasted from BLE beacons.
     * This currently does nothing and should be overridden by a subclass.
     * @param callback Callback to be run when subscription cancellation task is done, regardless of
     *         whether it is successful.
     */
    void backgroundUnsubscribe(Runnable callback) {
        Log.d(TAG, "background unsubscribing in empty client");
        if (callback != null) {
            callback.run();
        }
    }

    /**
     * Cancel a background subscription to URLs broadcasted from BLE beacons.
     * This currently does nothing and should be overridden by a subclass.
     */
    void backgroundUnsubscribe() {
        backgroundUnsubscribe(null);
    }

    /**
     * Begin a foreground subscription to URLs broadcasted from BLE beacons.
     * This currently does nothing and should be overridden by a subclass.
     * @param activity The Activity that is performing the scan.
     */
    void foregroundSubscribe(Activity activity) {
        Log.d(TAG, "foreground subscribing in empty client");
    }

    /**
     * Cancel a foreground subscription to URLs broadcasted from BLE beacons.
     * This currently does nothing and should be overridden by a subclass.
     */
    void foregroundUnsubscribe() {
        Log.d(TAG, "foreground unsubscribing in empty client");
    }
}
