// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.os.Build;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;

/**
 * Tests for org.chromium.net.NetworkChangeNotifier without native code. This class specifically
 * does not have a setUp() method that loads native libraries.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@SuppressLint("NewApi")
public class NetworkChangeNotifierNoNativeTest {
    @After
    public void tearDown() {
        // Destroy NetworkChangeNotifierAutoDetect
        NetworkChangeNotifier.setAutoDetectConnectivityState(false);
    }

    /**
     * Verify NetworkChangeNotifier can initialize without calling into native code. This test
     * will crash if any native calls are made during NetworkChangeNotifier initialization.
     */
    @Test
    @MediumTest
    public void testNoNativeDependence() {
        Looper.prepare();
        NetworkChangeNotifier.init();
        NetworkChangeNotifier.registerToReceiveNotificationsAlways();
    }

    /**
     * Verify NetworkChangeNotifier.registerNetworkCallbackFailed() and
     * NetworkChangeNotifier.isProcessBoundToNetwork() return false under normal circumstances.
     */
    @Test
    @MediumTest
    public void testDefaultState() {
        Looper.prepare();
        NetworkChangeNotifier ncn = NetworkChangeNotifier.init();
        Assert.assertFalse(ncn.registerNetworkCallbackFailed());
        Assert.assertFalse(NetworkChangeNotifier.isProcessBoundToNetwork());
        NetworkChangeNotifier.registerToReceiveNotificationsAlways();
        Assert.assertFalse(ncn.registerNetworkCallbackFailed());
        Assert.assertFalse(NetworkChangeNotifier.isProcessBoundToNetwork());
    }

    /**
     * Verify NetworkChangeNotifier.registerNetworkCallbackFailed() catches exception properly.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    public void testRegisterNetworkCallbackFail() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) InstrumentationRegistry.getTargetContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        Looper.prepare();
        NetworkChangeNotifier ncn = NetworkChangeNotifier.init();
        Assert.assertFalse(ncn.registerNetworkCallbackFailed());

        // Use up all NetworkRequests
        for (int i = 0; i < 99; i++) {
            connectivityManager.registerDefaultNetworkCallback(new NetworkCallback());
        }

        NetworkChangeNotifier.registerToReceiveNotificationsAlways();
        Assert.assertTrue(ncn.registerNetworkCallbackFailed());
    }
}
