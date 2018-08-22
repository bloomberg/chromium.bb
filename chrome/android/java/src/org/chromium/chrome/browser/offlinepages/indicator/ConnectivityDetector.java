// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.support.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that detects the network connectivity.
 */
public class ConnectivityDetector implements NetworkChangeNotifier.ConnectionTypeObserver {
    // Denotes the connection state.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ConnectionState.NONE, ConnectionState.DISCONNECTED, ConnectionState.NO_INTERNET,
            ConnectionState.CAPTIVE_PORTAL, ConnectionState.VALIDATED})
    public @interface ConnectionState {
        // Initial state or connection state can't be evaluated.
        int NONE = 0;
        // The network is disconnected.
        int DISCONNECTED = 1;
        // The network is connected, but it can't reach the Internet, i.e. connecting to a hotspot
        // that is not conencted to Internet.
        int NO_INTERNET = 2;
        // The network is connected, but capitive portal is detected and the user has not signed
        // into it.
        int CAPTIVE_PORTAL = 3;
        // The network is connected to Internet and validated. If the connected network imposes
        // capitive portal, the user has already signed into it.
        int VALIDATED = 4;
    }

    /**
     * Interface for observing network connectivity changes.
     */
    public interface Observer {
        /**
         * Called when the network connection state changes.
         * @param connectionState Current connection state.
         */
        void onConnectionStateChanged(@ConnectionState int connectionState);
    }

    private static boolean sSkipSystemCheckForTesting = false;

    private Observer mObserver;

    private @ConnectionType int mConnectionType = ConnectionType.CONNECTION_UNKNOWN;
    private @ConnectionState int mConnectionState = ConnectionState.NONE;

    public ConnectivityDetector(Observer observer) {
        mObserver = observer;
        NetworkChangeNotifier.addConnectionTypeObserver(this);
        detect();
    }

    public void detect() {
        onConnectionTypeChanged(NetworkChangeNotifier.getInstance().getCurrentConnectionType());
    }

    @Override
    public void onConnectionTypeChanged(@ConnectionType int connectionType) {
        // This method may be called multiple times with same |connectionType|.
        if (mConnectionType == connectionType) {
            return;
        }
        mConnectionType = connectionType;

        // If not connected at all, no further check is needed.
        if (connectionType == ConnectionType.CONNECTION_NONE) {
            mConnectionState = ConnectionState.DISCONNECTED;
            notifyConnectionStateChanged();
            return;
        }

        // Check the Android system to determine the network connectivity. If unavailable, as in
        // Android version below Marshmallow, we will kick off our own probes.
        @ConnectionState
        int connectionState = getConnectionStateFromSystem();
        if (connectionState != ConnectionState.NONE) {
            mConnectionState = connectionState;
            notifyConnectionStateChanged();
            return;
        }

        // TODO(jianli): Do manual check via sending HTTP probes to server.
        mConnectionState = ConnectionState.VALIDATED;
        notifyConnectionStateChanged();
    }

    /**
     * Consults with the Android connection manager to find out the network state.
     */
    @TargetApi(Build.VERSION_CODES.M)
    private static @ConnectionState int getConnectionStateFromSystem() {
        if (sSkipSystemCheckForTesting) return ConnectionState.NONE;

        // NET_CAPABILITY_VALIDATED and NET_CAPABILITY_CAPTIVE_PORTAL are only available on
        // Marshmallow and later versions.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return ConnectionState.NONE;
        ConnectivityManager connectivityManager =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        if (connectivityManager == null) return ConnectionState.NONE;

        boolean isCapitivePortal = false;
        Network[] networks = connectivityManager.getAllNetworks();
        if (networks.length == 0) return ConnectionState.DISCONNECTED;

        for (Network network : networks) {
            NetworkCapabilities capabilities = connectivityManager.getNetworkCapabilities(network);
            if (capabilities == null) continue;
            if (capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
                    && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                    && capabilities.hasCapability(
                               NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED)) {
                return ConnectionState.VALIDATED;
            }
            if (capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL)) {
                isCapitivePortal = true;
            }
        }

        return isCapitivePortal ? ConnectionState.CAPTIVE_PORTAL : ConnectionState.NO_INTERNET;
    }

    private void notifyConnectionStateChanged() {
        mObserver.onConnectionStateChanged(mConnectionState);
    }

    @VisibleForTesting
    static void skipSystemCheckForTesting() {
        sSkipSystemCheckForTesting = true;
    }
}
