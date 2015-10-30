// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.telephony.TelephonyManager;
import android.util.Log;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

/**
 * Used by the NetworkChangeNotifier to listens to platform changes in connectivity.
 * Note that use of this class requires that the app have the platform
 * ACCESS_NETWORK_STATE permission.
 */
public class NetworkChangeNotifierAutoDetect extends BroadcastReceiver {
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

        /**
         * Returns connection type and status information about the current
         * default network.
         */
        NetworkState getNetworkState() {
            return getNetworkState(mConnectivityManager.getActiveNetworkInfo());
        }

        /**
         * Returns connection type and status information about |network|.
         * Only callable on Lollipop and newer releases.
         */
        @SuppressLint("NewApi")
        NetworkState getNetworkState(Network network) {
            return getNetworkState(mConnectivityManager.getNetworkInfo(network));
        }

        /**
         * Returns connection type and status information gleaned from networkInfo.
         */
        NetworkState getNetworkState(NetworkInfo networkInfo) {
            if (networkInfo == null || !networkInfo.isConnected()) {
                return new NetworkState(false, -1, -1);
            }
            return new NetworkState(true, networkInfo.getType(), networkInfo.getSubtype());
        }

        /**
         * Returns all connected networks.
         * Only callable on Lollipop and newer releases.
         */
        @SuppressLint("NewApi")
        Network[] getAllNetworks() {
            return mConnectivityManager.getAllNetworks();
        }

        /**
         * Registers networkCallback to receive notifications about networks
         * that satisfy networkRequest.
         * Only callable on Lollipop and newer releases.
         */
        @SuppressLint("NewApi")
        void registerNetworkCallback(
                NetworkRequest networkRequest, NetworkCallback networkCallback) {
            mConnectivityManager.registerNetworkCallback(networkRequest, networkCallback);
        }

        /**
         * Unregisters networkCallback from receiving notifications.
         * Only callable on Lollipop and newer releases.
         */
        @SuppressLint("NewApi")
        void unregisterNetworkCallback(NetworkCallback networkCallback) {
            mConnectivityManager.unregisterNetworkCallback(networkCallback);
        }

        /**
         * Returns the NetID of the current default network. Returns
         * NetId.INVALID if no current default network connected.
         * Only callable on Lollipop and newer releases.
         */
        @SuppressLint("NewApi")
        int getDefaultNetId() {
            // Android Lollipop had no API to get the default network; only an
            // API to return the NetworkInfo for the default network. To
            // determine the default network one can find the network with
            // type matching that of the default network.
            final NetworkInfo defaultNetworkInfo = mConnectivityManager.getActiveNetworkInfo();
            if (defaultNetworkInfo == null) {
                return NetId.INVALID;
            }
            final Network[] networks = getAllNetworks();
            int defaultNetId = NetId.INVALID;
            for (Network network : networks) {
                if (!hasInternetCapability(network)) {
                    continue;
                }
                final NetworkInfo networkInfo = mConnectivityManager.getNetworkInfo(network);
                if (networkInfo != null && networkInfo.getType() == defaultNetworkInfo.getType()) {
                    // There should not be multiple connected networks of the
                    // same type. At least as of Android Marshmallow this is
                    // not supported. If this becomes supported this assertion
                    // may trigger. At that point we could consider using
                    // ConnectivityManager.getDefaultNetwork() though this
                    // may give confusing results with VPNs and is only
                    // available with Android Marshmallow.
                    assert defaultNetId == NetId.INVALID;
                    defaultNetId = networkToNetId(network);
                }
            }
            return defaultNetId;
        }

        /**
         * Returns true if {@code network} can provide Internet access. Can be used to
         * ignore specialized networks (e.g. IMS, FOTA).
         */
        @SuppressLint("NewApi")
        boolean hasInternetCapability(Network network) {
            final NetworkCapabilities capabilities =
                    mConnectivityManager.getNetworkCapabilities(network);
            return capabilities != null && capabilities.hasCapability(NET_CAPABILITY_INTERNET);
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

    // This class gets called back by ConnectivityManager whenever networks come
    // and go. It gets called back on a special handler thread
    // ConnectivityManager creates for making the callbacks. The callbacks in
    // turn post to the UI thread where mObserver lives.
    @SuppressLint("NewApi")
    private class MyNetworkCallback extends NetworkCallback {
        @Override
        public void onAvailable(Network network) {
            final int netId = networkToNetId(network);
            final int connectionType =
                    getCurrentConnectionType(mConnectivityManagerDelegate.getNetworkState(network));
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onNetworkConnect(netId, connectionType);
                }
            });
        }

        @Override
        public void onCapabilitiesChanged(
                Network network, NetworkCapabilities networkCapabilities) {
            // A capabilities change may indicate the ConnectionType has changed,
            // so forward the new ConnectionType along to observer.
            final int netId = networkToNetId(network);
            final int connectionType =
                    getCurrentConnectionType(mConnectivityManagerDelegate.getNetworkState(network));
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onNetworkConnect(netId, connectionType);
                }
            });
        }

        @Override
        public void onLosing(Network network, int maxMsToLive) {
            final int netId = networkToNetId(network);
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onNetworkSoonToDisconnect(netId);
                }
            });
        }

        @Override
        public void onLost(Network network) {
            final int netId = networkToNetId(network);
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mObserver.onNetworkDisconnect(netId);
                }
            });
        }
    }

    /**
     * Abstract class for providing a policy regarding when the NetworkChangeNotifier
     * should listen for network changes.
     */
    public abstract static class RegistrationPolicy {
        private NetworkChangeNotifierAutoDetect mNotifier;

        /**
         * Start listening for network changes.
         */
        protected final void register() {
            assert mNotifier != null;
            mNotifier.register();
        }

        /**
         * Stop listening for network changes.
         */
        protected final void unregister() {
            assert mNotifier != null;
            mNotifier.unregister();
        }

        /**
         * Initializes the policy with the notifier, overriding subclasses should always
         * call this method.
         */
        protected void init(NetworkChangeNotifierAutoDetect notifier) {
            mNotifier = notifier;
        }

        protected abstract void destroy();
    }

    private static final String TAG = "NetworkChangeNotifierAutoDetect";
    private static final int UNKNOWN_LINK_SPEED = -1;

    private final NetworkConnectivityIntentFilter mIntentFilter;
    private final Observer mObserver;
    private final Context mContext;
    private final RegistrationPolicy mRegistrationPolicy;

    // mConnectivityManagerDelegates and mWifiManagerDelegate are only non-final for testing.
    private ConnectivityManagerDelegate mConnectivityManagerDelegate;
    private WifiManagerDelegate mWifiManagerDelegate;
    // mNetworkCallback and mNetworkRequest are only non-null in Android L and above.
    private final NetworkCallback mNetworkCallback;
    private final NetworkRequest mNetworkRequest;
    private boolean mRegistered;
    private int mConnectionType;
    private String mWifiSSID;
    private double mMaxBandwidthMbps;

    /**
     * Observer interface by which observer is notified of network changes.
     */
    public static interface Observer {
        /**
         * Called when default network changes.
         */
        public void onConnectionTypeChanged(int newConnectionType);
        /**
         * Called when maximum bandwidth of default network changes.
         */
        public void onMaxBandwidthChanged(double maxBandwidthMbps);
        /**
         * Called when device connects to network with NetID netId. For
         * example device associates with a WiFi access point.
         * connectionType is the type of the network; a member of
         * ConnectionType. Only called on Android L and above.
         */
        public void onNetworkConnect(int netId, int connectionType);
        /**
         * Called when device determines the connection to the network with
         * NetID netId is no longer preferred, for example when a device
         * transitions from cellular to WiFi it might deem the cellular
         * connection no longer preferred. The device will disconnect from
         * the network in 30s allowing network communications on that network
         * to wrap up. Only called on Android L and above.
         */
        public void onNetworkSoonToDisconnect(int netId);
        /**
         * Called when device disconnects from network with NetID netId.
         * Only called on Android L and above.
         */
        public void onNetworkDisconnect(int netId);
        /**
         * Called to cause a purge of cached lists of active networks, of any
         * networks not in the accompanying list of active networks. This is
         * issued if a period elapsed where disconnected notifications may have
         * been missed, and acts to keep cached lists of active networks
         * accurate. Only called on Android L and above.
         */
        public void updateActiveNetworkList(int[] activeNetIds);
    }

    /**
     * Constructs a NetworkChangeNotifierAutoDetect. Should only be called on UI thread.
     * @param policy The RegistrationPolicy which determines when this class should watch
     *     for network changes (e.g. see (@link RegistrationPolicyAlwaysRegister} and
     *     {@link RegistrationPolicyApplicationStatus}).
     */
    @SuppressLint("NewApi")
    public NetworkChangeNotifierAutoDetect(
            Observer observer, Context context, RegistrationPolicy policy) {
        // Since BroadcastReceiver is always called back on UI thread, ensure
        // running on UI thread so notification logic can be single-threaded.
        ThreadUtils.assertOnUiThread();
        mObserver = observer;
        mContext = context.getApplicationContext();
        mConnectivityManagerDelegate = new ConnectivityManagerDelegate(context);
        mWifiManagerDelegate = new WifiManagerDelegate(context);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            mNetworkCallback = new MyNetworkCallback();
            mNetworkRequest =
                    new NetworkRequest.Builder().addCapability(NET_CAPABILITY_INTERNET).build();
        } else {
            mNetworkCallback = null;
            mNetworkRequest = null;
        }
        final NetworkState networkState = mConnectivityManagerDelegate.getNetworkState();
        mConnectionType = getCurrentConnectionType(networkState);
        mWifiSSID = getCurrentWifiSSID(networkState);
        mMaxBandwidthMbps = getCurrentMaxBandwidthInMbps(networkState);
        mIntentFilter =
                new NetworkConnectivityIntentFilter(mWifiManagerDelegate.getHasWifiPermission());
        mRegistrationPolicy = policy;
        mRegistrationPolicy.init(this);
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

    @VisibleForTesting
    RegistrationPolicy getRegistrationPolicy() {
        return mRegistrationPolicy;
    }

    /**
     * Returns whether the object has registered to receive network connectivity intents.
     */
    @VisibleForTesting
    boolean isReceiverRegisteredForTesting() {
        return mRegistered;
    }

    public void destroy() {
        mRegistrationPolicy.destroy();
        unregister();
    }

    /**
     * Registers a BroadcastReceiver in the given context.
     */
    public void register() {
        if (mRegistered) return;

        final NetworkState networkState = getCurrentNetworkState();
        connectionTypeChanged(networkState);
        maxBandwidthChanged(networkState);
        mContext.registerReceiver(this, mIntentFilter);
        mRegistered = true;

        if (mNetworkCallback != null) {
            mConnectivityManagerDelegate.registerNetworkCallback(mNetworkRequest, mNetworkCallback);
            // registerNetworkCallback() will rematch our NetworkRequest
            // against active networks, so a cached list of active networks
            // will be repopulated immediatly after this. However we need to
            // purge any cached networks as they may have been disconnected
            // while mNetworkCallback was unregistered.
            final Network[] networks = mConnectivityManagerDelegate.getAllNetworks();
            // Convert Networks to NetIDs.
            final int[] netIds = new int[networks.length];
            for (int i = 0; i < networks.length; i++) {
                netIds[i] = networkToNetId(networks[i]);
            }
            mObserver.updateActiveNetworkList(netIds);
        }
    }

    /**
     * Unregisters a BroadcastReceiver in the given context.
     */
    public void unregister() {
        if (!mRegistered) return;
        mContext.unregisterReceiver(this);
        mRegistered = false;
        if (mNetworkCallback != null) {
            mConnectivityManagerDelegate.unregisterNetworkCallback(mNetworkCallback);
        }
    }

    public NetworkState getCurrentNetworkState() {
        return mConnectivityManagerDelegate.getNetworkState();
    }

    /**
     * Returns an array of all of the device's currently connected
     * networks and ConnectionTypes. Array elements are a repeated sequence of:
     *   NetID of network
     *   ConnectionType of network
     * Only available on Lollipop and newer releases and when auto-detection has
     * been enabled.
     */
    public int[] getNetworksAndTypes() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return new int[0];
        }
        final Network networks[] = mConnectivityManagerDelegate.getAllNetworks();
        final int networksAndTypes[] = new int[networks.length * 2];
        int index = 0;
        for (Network network : networks) {
            if (!mConnectivityManagerDelegate.hasInternetCapability(network)) {
                continue;
            }
            networksAndTypes[index++] = networkToNetId(network);
            networksAndTypes[index++] =
                    getCurrentConnectionType(mConnectivityManagerDelegate.getNetworkState(network));
        }
        final int shortenedNetworksAndTypes[] = new int[index];
        System.arraycopy(networksAndTypes, 0, shortenedNetworksAndTypes, 0, index);
        return shortenedNetworksAndTypes;
    }

    /**
     * Returns NetID of device's current default connected network used for
     * communication.
     * Only implemented on Lollipop and newer releases, returns NetId.INVALID
     * when not implemented.
     */
    public int getDefaultNetId() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return NetId.INVALID;
        }
        return mConnectivityManagerDelegate.getDefaultNetId();
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
     * Returns the bandwidth of the current connection in Mbps. The result is
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

    /**
     * Extracts NetID of network. Only available on Lollipop and newer releases.
     */
    @SuppressLint("NewApi")
    private static int networkToNetId(Network network) {
        // NOTE(pauljensen): This depends on Android framework implementation details.
        // Fortunately this functionality is unlikely to ever change.
        // TODO(pauljensen): When we update to Android M SDK, use Network.getNetworkHandle().
        return Integer.parseInt(network.toString());
    }
}
