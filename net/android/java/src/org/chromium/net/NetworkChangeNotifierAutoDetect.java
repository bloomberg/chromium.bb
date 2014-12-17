// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.Manifest.permission;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.telephony.TelephonyManager;
import android.util.Log;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;

/**
 * Used by the NetworkChangeNotifier to listens to platform changes in connectivity.
 * Note that use of this class requires that the app have the platform
 * ACCESS_NETWORK_STATE permission.
 */
public class NetworkChangeNotifierAutoDetect extends BroadcastReceiver
        implements ApplicationStatus.ApplicationStateListener {

    static class NetworkState {
        private final boolean mConnected;
        private final int mType;
        private final int mSubtype;

        public NetworkState(boolean connected, int type, int subtype) {
            mConnected = connected;
            mType = type;
            mSubtype = subtype;
        }

        public boolean isConnected() {
            return mConnected;
        }

        public int getNetworkType() {
            return mType;
        }

        public int getNetworkSubType() {
            return mSubtype;
        }
    }

    /** Queries the ConnectivityManager for information about the current connection. */
    static class ConnectivityManagerDelegate {
        private final ConnectivityManager mConnectivityManager;

        ConnectivityManagerDelegate(Context context) {
            mConnectivityManager =
                    (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        }

        // For testing.
        ConnectivityManagerDelegate() {
            // All the methods below should be overridden.
            mConnectivityManager = null;
        }

        NetworkState getNetworkState() {
            final NetworkInfo networkInfo = mConnectivityManager.getActiveNetworkInfo();
            if (networkInfo == null || !networkInfo.isConnected()) {
                return new NetworkState(false, -1, -1);
            }
            return new NetworkState(true, networkInfo.getType(), networkInfo.getSubtype());
        }
    }

    /** Queries the WifiManager for SSID of the current Wifi connection. */
    static class WifiManagerDelegate {
        private final Context mContext;
        private final WifiManager mWifiManager;
        private final boolean mHasWifiPermission;

        WifiManagerDelegate(Context context) {
            mContext = context;
            // TODO(jkarlin): If the embedder doesn't have ACCESS_WIFI_STATE permission then inform
            // native code and fail if native NetworkChangeNotifierAndroid::GetMaxBandwidth() is
            // called.
            mHasWifiPermission = mContext.getPackageManager().checkPermission(
                    permission.ACCESS_WIFI_STATE, mContext.getPackageName())
                    == PackageManager.PERMISSION_GRANTED;
            mWifiManager = mHasWifiPermission
                    ? (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE) : null;
        }

        // For testing.
        WifiManagerDelegate() {
            // All the methods below should be overridden.
            mContext = null;
            mWifiManager = null;
            mHasWifiPermission = false;
        }

        String getWifiSSID() {
            final Intent intent = mContext.registerReceiver(null,
                    new IntentFilter(WifiManager.NETWORK_STATE_CHANGED_ACTION));
            if (intent != null) {
                final WifiInfo wifiInfo = intent.getParcelableExtra(WifiManager.EXTRA_WIFI_INFO);
                if (wifiInfo != null) {
                    final String ssid = wifiInfo.getSSID();
                    if (ssid != null) {
                        return ssid;
                    }
                }
            }
            return "";
        }

        /*
         * Requires ACCESS_WIFI_STATE permission to get the real link speed, else returns
         * UNKNOWN_LINK_SPEED.
         */
        int getLinkSpeedInMbps() {
            if (!mHasWifiPermission || mWifiManager == null) return UNKNOWN_LINK_SPEED;
            final WifiInfo wifiInfo = mWifiManager.getConnectionInfo();
            if (wifiInfo == null) return UNKNOWN_LINK_SPEED;

            // wifiInfo.getLinkSpeed returns the current wifi linkspeed, which can change even
            // though the connection type hasn't changed.
            return wifiInfo.getLinkSpeed();
        }

        boolean getHasWifiPermission() {
            return mHasWifiPermission;
        }
    }

    private static final String TAG = "NetworkChangeNotifierAutoDetect";
    private static final int UNKNOWN_LINK_SPEED = -1;
    private final NetworkConnectivityIntentFilter mIntentFilter;

    private final Observer mObserver;

    private final Context mContext;
    private ConnectivityManagerDelegate mConnectivityManagerDelegate;
    private WifiManagerDelegate mWifiManagerDelegate;
    private boolean mRegistered;
    private int mConnectionType;
    private String mWifiSSID;
    private double mMaxBandwidthMbps;

    /**
     * Observer notified on the UI thread whenever a new connection type was detected or max
     * bandwidth is changed.
     */
    public static interface Observer {
        public void onConnectionTypeChanged(int newConnectionType);
        public void onMaxBandwidthChanged(double maxBandwidthMbps);
    }

    /**
     * Constructs a NetworkChangeNotifierAutoDetect.
     * @param alwaysWatchForChanges If true, always watch for network changes.
     *    Otherwise, only watch if app is in foreground.
     */
    public NetworkChangeNotifierAutoDetect(Observer observer, Context context,
            boolean alwaysWatchForChanges) {
        mObserver = observer;
        mContext = context.getApplicationContext();
        mConnectivityManagerDelegate = new ConnectivityManagerDelegate(context);
        mWifiManagerDelegate = new WifiManagerDelegate(context);
        final NetworkState networkState = mConnectivityManagerDelegate.getNetworkState();
        mConnectionType = getCurrentConnectionType(networkState);
        mWifiSSID = getCurrentWifiSSID(networkState);
        mMaxBandwidthMbps = getCurrentMaxBandwidthInMbps(networkState);
        mIntentFilter =
                new NetworkConnectivityIntentFilter(mWifiManagerDelegate.getHasWifiPermission());

        if (alwaysWatchForChanges) {
            registerReceiver();
        } else {
            ApplicationStatus.registerApplicationStateListener(this);
        }
    }

    /**
     * Allows overriding the ConnectivityManagerDelegate for tests.
     */
    void setConnectivityManagerDelegateForTests(ConnectivityManagerDelegate delegate) {
        mConnectivityManagerDelegate = delegate;
    }

    /**
     * Allows overriding the WifiManagerDelegate for tests.
     */
    void setWifiManagerDelegateForTests(WifiManagerDelegate delegate) {
        mWifiManagerDelegate = delegate;
    }

    public void destroy() {
        unregisterReceiver();
    }

    /**
     * Register a BroadcastReceiver in the given context.
     */
    private void registerReceiver() {
        if (!mRegistered) {
            mRegistered = true;
            mContext.registerReceiver(this, mIntentFilter);
        }
    }

    /**
     * Unregister the BroadcastReceiver in the given context.
     */
    private void unregisterReceiver() {
        if (mRegistered) {
            mRegistered = false;
            mContext.unregisterReceiver(this);
        }
    }

    public NetworkState getCurrentNetworkState() {
        return mConnectivityManagerDelegate.getNetworkState();
    }

    public int getCurrentConnectionType(NetworkState networkState) {
        if (!networkState.isConnected()) {
            return ConnectionType.CONNECTION_NONE;
        }

        switch (networkState.getNetworkType()) {
            case ConnectivityManager.TYPE_ETHERNET:
                return ConnectionType.CONNECTION_ETHERNET;
            case ConnectivityManager.TYPE_WIFI:
                return ConnectionType.CONNECTION_WIFI;
            case ConnectivityManager.TYPE_WIMAX:
                return ConnectionType.CONNECTION_4G;
            case ConnectivityManager.TYPE_BLUETOOTH:
                return ConnectionType.CONNECTION_BLUETOOTH;
            case ConnectivityManager.TYPE_MOBILE:
                // Use information from TelephonyManager to classify the connection.
                switch (networkState.getNetworkSubType()) {
                    case TelephonyManager.NETWORK_TYPE_GPRS:
                    case TelephonyManager.NETWORK_TYPE_EDGE:
                    case TelephonyManager.NETWORK_TYPE_CDMA:
                    case TelephonyManager.NETWORK_TYPE_1xRTT:
                    case TelephonyManager.NETWORK_TYPE_IDEN:
                        return ConnectionType.CONNECTION_2G;
                    case TelephonyManager.NETWORK_TYPE_UMTS:
                    case TelephonyManager.NETWORK_TYPE_EVDO_0:
                    case TelephonyManager.NETWORK_TYPE_EVDO_A:
                    case TelephonyManager.NETWORK_TYPE_HSDPA:
                    case TelephonyManager.NETWORK_TYPE_HSUPA:
                    case TelephonyManager.NETWORK_TYPE_HSPA:
                    case TelephonyManager.NETWORK_TYPE_EVDO_B:
                    case TelephonyManager.NETWORK_TYPE_EHRPD:
                    case TelephonyManager.NETWORK_TYPE_HSPAP:
                        return ConnectionType.CONNECTION_3G;
                    case TelephonyManager.NETWORK_TYPE_LTE:
                        return ConnectionType.CONNECTION_4G;
                    default:
                        return ConnectionType.CONNECTION_UNKNOWN;
                }
            default:
                return ConnectionType.CONNECTION_UNKNOWN;
        }
    }

    /*
     * Returns the bandwidth of the current connection in Mbps.  The result is
     * derived from the NetInfo v3 specification's mapping from network type to
     * max link speed. In cases where more information is available, such as wifi,
     * that is used instead. For more on NetInfo, see http://w3c.github.io/netinfo/.
     */
    public double getCurrentMaxBandwidthInMbps(NetworkState networkState) {
        if (getCurrentConnectionType(networkState) == ConnectionType.CONNECTION_WIFI) {
            final int link_speed = mWifiManagerDelegate.getLinkSpeedInMbps();
            if (link_speed != UNKNOWN_LINK_SPEED) {
                return link_speed;
            }
        }

        return NetworkChangeNotifier.getMaxBandwidthForConnectionSubtype(
                getCurrentConnectionSubtype(networkState));
    }

    private int getCurrentConnectionSubtype(NetworkState networkState) {
        if (!networkState.isConnected()) {
            return ConnectionSubtype.SUBTYPE_NONE;
        }

        switch (networkState.getNetworkType()) {
            case ConnectivityManager.TYPE_ETHERNET:
            case ConnectivityManager.TYPE_WIFI:
            case ConnectivityManager.TYPE_WIMAX:
            case ConnectivityManager.TYPE_BLUETOOTH:
                return ConnectionSubtype.SUBTYPE_UNKNOWN;
            case ConnectivityManager.TYPE_MOBILE:
                // Use information from TelephonyManager to classify the connection.
                switch (networkState.getNetworkSubType()) {
                    case TelephonyManager.NETWORK_TYPE_GPRS:
                        return ConnectionSubtype.SUBTYPE_GPRS;
                    case TelephonyManager.NETWORK_TYPE_EDGE:
                        return ConnectionSubtype.SUBTYPE_EDGE;
                    case TelephonyManager.NETWORK_TYPE_CDMA:
                        return ConnectionSubtype.SUBTYPE_CDMA;
                    case TelephonyManager.NETWORK_TYPE_1xRTT:
                        return ConnectionSubtype.SUBTYPE_1XRTT;
                    case TelephonyManager.NETWORK_TYPE_IDEN:
                        return ConnectionSubtype.SUBTYPE_IDEN;
                    case TelephonyManager.NETWORK_TYPE_UMTS:
                        return ConnectionSubtype.SUBTYPE_UMTS;
                    case TelephonyManager.NETWORK_TYPE_EVDO_0:
                        return ConnectionSubtype.SUBTYPE_EVDO_REV_0;
                    case TelephonyManager.NETWORK_TYPE_EVDO_A:
                        return ConnectionSubtype.SUBTYPE_EVDO_REV_A;
                    case TelephonyManager.NETWORK_TYPE_HSDPA:
                        return ConnectionSubtype.SUBTYPE_HSDPA;
                    case TelephonyManager.NETWORK_TYPE_HSUPA:
                        return ConnectionSubtype.SUBTYPE_HSUPA;
                    case TelephonyManager.NETWORK_TYPE_HSPA:
                        return ConnectionSubtype.SUBTYPE_HSPA;
                    case TelephonyManager.NETWORK_TYPE_EVDO_B:
                        return ConnectionSubtype.SUBTYPE_EVDO_REV_B;
                    case TelephonyManager.NETWORK_TYPE_EHRPD:
                        return ConnectionSubtype.SUBTYPE_EHRPD;
                    case TelephonyManager.NETWORK_TYPE_HSPAP:
                        return ConnectionSubtype.SUBTYPE_HSPAP;
                    case TelephonyManager.NETWORK_TYPE_LTE:
                        return ConnectionSubtype.SUBTYPE_LTE;
                    default:
                        return ConnectionSubtype.SUBTYPE_UNKNOWN;
                }
            default:
                return ConnectionSubtype.SUBTYPE_UNKNOWN;
        }
    }

    private String getCurrentWifiSSID(NetworkState networkState) {
        if (getCurrentConnectionType(networkState) != ConnectionType.CONNECTION_WIFI) return "";
        return mWifiManagerDelegate.getWifiSSID();
    }

    // BroadcastReceiver
    @Override
    public void onReceive(Context context, Intent intent) {
        final NetworkState networkState = getCurrentNetworkState();
        if (ConnectivityManager.CONNECTIVITY_ACTION.equals(intent.getAction())) {
            connectionTypeChanged(networkState);
            maxBandwidthChanged(networkState);
        } else if (WifiManager.RSSI_CHANGED_ACTION.equals(intent.getAction())) {
            maxBandwidthChanged(networkState);
        }
    }

    // ApplicationStatus.ApplicationStateListener
    @Override
    public void onApplicationStateChange(int newState) {
        final NetworkState networkState = getCurrentNetworkState();
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            connectionTypeChanged(networkState);
            maxBandwidthChanged(networkState);
            registerReceiver();
        } else if (newState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            unregisterReceiver();
        }
    }

    private void connectionTypeChanged(NetworkState networkState) {
        int newConnectionType = getCurrentConnectionType(networkState);
        String newWifiSSID = getCurrentWifiSSID(networkState);
        if (newConnectionType == mConnectionType && newWifiSSID.equals(mWifiSSID)) return;

        mConnectionType = newConnectionType;
        mWifiSSID = newWifiSSID;
        Log.d(TAG, "Network connectivity changed, type is: " + mConnectionType);
        mObserver.onConnectionTypeChanged(newConnectionType);
    }

    private void maxBandwidthChanged(NetworkState networkState) {
        double newMaxBandwidthMbps = getCurrentMaxBandwidthInMbps(networkState);
        if (newMaxBandwidthMbps == mMaxBandwidthMbps) return;
        mMaxBandwidthMbps = newMaxBandwidthMbps;
        mObserver.onMaxBandwidthChanged(newMaxBandwidthMbps);
    }

    private static class NetworkConnectivityIntentFilter extends IntentFilter {
        NetworkConnectivityIntentFilter(boolean monitorRSSI) {
            addAction(ConnectivityManager.CONNECTIVITY_ACTION);
            if (monitorRSSI) addAction(WifiManager.RSSI_CHANGED_ACTION);
        }
    }
}
