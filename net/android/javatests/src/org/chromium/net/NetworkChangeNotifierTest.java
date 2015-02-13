// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.telephony.TelephonyManager;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ApplicationState;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.Feature;
import org.chromium.net.NetworkChangeNotifierAutoDetect.NetworkState;

/**
 * Tests for org.chromium.net.NetworkChangeNotifier.
 */
public class NetworkChangeNotifierTest extends InstrumentationTestCase {
    /**
     * Listens for alerts fired by the NetworkChangeNotifier when network status changes.
     */
    private static class NetworkChangeNotifierTestObserver
            implements NetworkChangeNotifier.ConnectionTypeObserver {
        private boolean mReceivedNotification = false;

        @Override
        public void onConnectionTypeChanged(int connectionType) {
            mReceivedNotification = true;
        }

        public boolean hasReceivedNotification() {
            return mReceivedNotification;
        }

        public void resetHasReceivedNotification() {
            mReceivedNotification = false;
        }
    }

    /**
     * Mocks out calls to the ConnectivityManager.
     */
    class MockConnectivityManagerDelegate
            extends NetworkChangeNotifierAutoDetect.ConnectivityManagerDelegate {
        private boolean mActiveNetworkExists;
        private int mNetworkType;
        private int mNetworkSubtype;

        @Override
        NetworkState getNetworkState() {
            return new NetworkState(mActiveNetworkExists, mNetworkType, mNetworkSubtype);
        }

        void setActiveNetworkExists(boolean networkExists) {
            mActiveNetworkExists = networkExists;
        }

        void setNetworkType(int networkType) {
            mNetworkType = networkType;
        }

        void setNetworkSubtype(int networkSubtype) {
            mNetworkSubtype = networkSubtype;
        }
    }

    /**
     * Mocks out calls to the WifiManager.
     */
    class MockWifiManagerDelegate
            extends NetworkChangeNotifierAutoDetect.WifiManagerDelegate {
        private String mWifiSSID;
        private int mLinkSpeedMbps;

        @Override
        String getWifiSSID() {
            return mWifiSSID;
        }

        void setWifiSSID(String wifiSSID) {
            mWifiSSID = wifiSSID;
        }

        @Override
        int getLinkSpeedInMbps() {
            return mLinkSpeedMbps;
        }

        void setLinkSpeedInMbps(int linkSpeedInMbps) {
            mLinkSpeedMbps = linkSpeedInMbps;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        createTestNotifier(WatchForChanges.ONLY_WHEN_APP_IN_FOREGROUND);
    }

    private NetworkChangeNotifierAutoDetect mReceiver;
    private MockConnectivityManagerDelegate mConnectivityDelegate;
    private MockWifiManagerDelegate mWifiDelegate;

    private static enum WatchForChanges {
        ALWAYS,
        ONLY_WHEN_APP_IN_FOREGROUND,
    }

    /**
     * Helper method to create a notifier and delegates for testing.
     * @param watchForChanges indicates whether app wants to watch for changes always or only when
     *            it is in the foreground.
     */
    private void createTestNotifier(WatchForChanges watchForChanges) {
        Context context = getInstrumentation().getTargetContext();
        NetworkChangeNotifier.resetInstanceForTests(context);
        if (watchForChanges == WatchForChanges.ALWAYS) {
            NetworkChangeNotifier.registerToReceiveNotificationsAlways();
        } else {
            NetworkChangeNotifier.setAutoDetectConnectivityState(true);
        }
        mReceiver = NetworkChangeNotifier.getAutoDetectorForTest();
        assertNotNull(mReceiver);

        mConnectivityDelegate =
                new MockConnectivityManagerDelegate();
        mConnectivityDelegate.setActiveNetworkExists(true);
        mReceiver.setConnectivityManagerDelegateForTests(mConnectivityDelegate);

        mWifiDelegate = new MockWifiManagerDelegate();
        mReceiver.setWifiManagerDelegateForTests(mWifiDelegate);
        mWifiDelegate.setWifiSSID("foo");
    }

    private double getCurrentMaxBandwidthInMbps() {
        final NetworkChangeNotifierAutoDetect.NetworkState networkState =
                mReceiver.getCurrentNetworkState();
        return mReceiver.getCurrentMaxBandwidthInMbps(networkState);
    }

    private int getCurrentConnectionType() {
        final NetworkChangeNotifierAutoDetect.NetworkState networkState =
                mReceiver.getCurrentNetworkState();
        return mReceiver.getCurrentConnectionType(networkState);
    }

    /**
     * Tests that changing the RSSI_CHANGED_ACTION intent updates MaxBandwidth.
     */
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierRSSIEventUpdatesMaxBandwidthForWiFi()
            throws InterruptedException {
        NetworkChangeNotifier notifier = NetworkChangeNotifier.getInstance();
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
        mWifiDelegate.setLinkSpeedInMbps(42);
        Intent intent = new Intent(WifiManager.RSSI_CHANGED_ACTION);
        mReceiver.onReceive(getInstrumentation().getTargetContext(), intent);

        assertEquals(42.0, notifier.getCurrentMaxBandwidthInMbps());

        // Changing the link speed has no effect until the intent fires.
        mWifiDelegate.setLinkSpeedInMbps(80);
        assertEquals(42.0, notifier.getCurrentMaxBandwidthInMbps());

        // Fire the intent.
        mReceiver.onReceive(getInstrumentation().getTargetContext(), intent);
        assertEquals(80.0, notifier.getCurrentMaxBandwidthInMbps());

        // Firing a network type change intent also causes max bandwidth to update.
        mWifiDelegate.setLinkSpeedInMbps(20);
        intent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        mReceiver.onReceive(getInstrumentation().getTargetContext(), intent);
        assertEquals(20.0, notifier.getCurrentMaxBandwidthInMbps());
    }

    /**
     * Tests that changing the network type changes the maxBandwidth.
     */
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierMaxBandwidthEthernet() throws InterruptedException {
        // Show that for Ethernet the link speed is unknown (+Infinity).
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_ETHERNET);
        assertEquals(ConnectionType.CONNECTION_ETHERNET, getCurrentConnectionType());
        assertEquals(Double.POSITIVE_INFINITY, getCurrentMaxBandwidthInMbps());
    }

    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierMaxBandwidthWifi() throws InterruptedException {
        // Test that for wifi types the link speed is read from the WifiManager.
        mWifiDelegate.setLinkSpeedInMbps(42);
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
        assertEquals(ConnectionType.CONNECTION_WIFI, getCurrentConnectionType());
        assertEquals(42.0, getCurrentMaxBandwidthInMbps());
    }

    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierMaxBandwidthWiMax() throws InterruptedException {
        // Show that for WiMax the link speed is unknown (+Infinity), although the type is 4g.
        // TODO(jkarlin): Add support for CONNECTION_WIMAX as specified in
        // http://w3c.github.io/netinfo/.
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIMAX);
        assertEquals(ConnectionType.CONNECTION_4G, getCurrentConnectionType());
        assertEquals(Double.POSITIVE_INFINITY, getCurrentMaxBandwidthInMbps());
    }

    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierMaxBandwidthBluetooth() throws InterruptedException {
        // Show that for bluetooth the link speed is unknown (+Infinity).
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_BLUETOOTH);
        assertEquals(ConnectionType.CONNECTION_BLUETOOTH, getCurrentConnectionType());
        assertEquals(Double.POSITIVE_INFINITY, getCurrentMaxBandwidthInMbps());
    }

    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierMaxBandwidthMobile() throws InterruptedException {
        // Test that for mobile types the subtype is used to determine the maxBandwidth.
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_MOBILE);
        mConnectivityDelegate.setNetworkSubtype(TelephonyManager.NETWORK_TYPE_LTE);
        assertEquals(ConnectionType.CONNECTION_4G, getCurrentConnectionType());
        assertEquals(100.0, getCurrentMaxBandwidthInMbps());
    }

    /**
     * Tests that when Chrome gets an intent indicating a change in network connectivity, it sends a
     * notification to Java observers.
     */
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testNetworkChangeNotifierJavaObservers() throws InterruptedException {
        // Initialize the NetworkChangeNotifier with a connection.
        Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        mReceiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);

        // We shouldn't be re-notified if the connection hasn't actually changed.
        NetworkChangeNotifierTestObserver observer = new NetworkChangeNotifierTestObserver();
        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        mReceiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
        assertFalse(observer.hasReceivedNotification());

        // We shouldn't be notified if we're connected to non-Wifi and the Wifi SSID changes.
        mWifiDelegate.setWifiSSID("bar");
        mReceiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
        assertFalse(observer.hasReceivedNotification());
        // We should be notified when we change to Wifi.
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
        mReceiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
        assertTrue(observer.hasReceivedNotification());
        observer.resetHasReceivedNotification();
        // We should be notified when the Wifi SSID changes.
        mWifiDelegate.setWifiSSID("foo");
        mReceiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
        assertTrue(observer.hasReceivedNotification());
        observer.resetHasReceivedNotification();
        // We shouldn't be re-notified if the Wifi SSID hasn't actually changed.
        mReceiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
        assertFalse(observer.hasReceivedNotification());

        // Mimic that connectivity has been lost and ensure that Chrome notifies our observer.
        mConnectivityDelegate.setActiveNetworkExists(false);
        Intent noConnectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        mReceiver.onReceive(getInstrumentation().getTargetContext(), noConnectivityIntent);
        assertTrue(observer.hasReceivedNotification());

        observer.resetHasReceivedNotification();
        // Pretend we got moved to the background.
        mReceiver.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        // Change the state.
        mConnectivityDelegate.setActiveNetworkExists(true);
        mConnectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
        // The NetworkChangeNotifierAutoDetect doesn't receive any notification while we are in the
        // background, but when we get back to the foreground the state changed should be detected
        // and a notification sent.
        mReceiver.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        assertTrue(observer.hasReceivedNotification());
    }

    /**
     * Tests that when setting {@code registerToReceiveNotificationsAlways()},
     * a NetworkChangeNotifierAutoDetect object is successfully created.
     */
    @UiThreadTest
    @MediumTest
    @Feature({"Android-AppBase"})
    public void testCreateNetworkChangeNotifierAlwaysWatchForChanges() throws InterruptedException {
        createTestNotifier(WatchForChanges.ALWAYS);
        // Make sure notifications can be received.
        NetworkChangeNotifierTestObserver observer = new NetworkChangeNotifierTestObserver();
        NetworkChangeNotifier.addConnectionTypeObserver(observer);
        Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
        mReceiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
        assertTrue(observer.hasReceivedNotification());
    }
}
