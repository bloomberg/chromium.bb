// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.NativeClassQualifiedName;
import org.chromium.base.ObserverList;

import java.util.ArrayList;

/**
 * Triggers updates to the underlying network state in Chrome.
 *
 * By default, connectivity is assumed and changes must pushed from the embedder via the
 * forceConnectivityState function.
 * Embedders may choose to have this class auto-detect changes in network connectivity by invoking
 * the setAutoDetectConnectivityState function.
 *
 * WARNING: This class is not thread-safe.
 */
@JNINamespace("net")
public class NetworkChangeNotifier {
    /**
     * Alerted when the connection type of the network changes.
     * The alert is fired on the UI thread.
     */
    public interface ConnectionTypeObserver {
        public void onConnectionTypeChanged(int connectionType);
    }

    private final Context mContext;
    private final ArrayList<Long> mNativeChangeNotifiers;
    private final ObserverList<ConnectionTypeObserver> mConnectionTypeObservers;
    private NetworkChangeNotifierAutoDetect mAutoDetector;
    private int mCurrentConnectionType = ConnectionType.CONNECTION_UNKNOWN;
    private double mCurrentMaxBandwidth = Double.POSITIVE_INFINITY;

    private static NetworkChangeNotifier sInstance;

    private NetworkChangeNotifier(Context context) {
        mContext = context.getApplicationContext();
        mNativeChangeNotifiers = new ArrayList<Long>();
        mConnectionTypeObservers = new ObserverList<ConnectionTypeObserver>();
    }

    /**
     * Initializes the singleton once.
     */
    @CalledByNative
    public static NetworkChangeNotifier init(Context context) {
        if (sInstance == null) {
            sInstance = new NetworkChangeNotifier(context);
        }
        return sInstance;
    }

    public static boolean isInitialized() {
        return sInstance != null;
    }

    static void resetInstanceForTests(Context context) {
        sInstance = new NetworkChangeNotifier(context);
    }

    @CalledByNative
    public int getCurrentConnectionType() {
        return mCurrentConnectionType;
    }

    @CalledByNative
    public double getCurrentMaxBandwidthInMbps() {
        return mCurrentMaxBandwidth;
    }

    /**
     * Calls a native map lookup of subtype to max bandwidth.
     */
    public static double getMaxBandwidthForConnectionSubtype(int subtype) {
        return nativeGetMaxBandwidthForConnectionSubtype(subtype);
    }

    /**
     * Adds a native-side observer.
     */
    @CalledByNative
    public void addNativeObserver(long nativeChangeNotifier) {
        mNativeChangeNotifiers.add(nativeChangeNotifier);
    }

    /**
     * Removes a native-side observer.
     */
    @CalledByNative
    public void removeNativeObserver(long nativeChangeNotifier) {
        mNativeChangeNotifiers.remove(nativeChangeNotifier);
    }

    /**
     * Returns the singleton instance.
     */
    public static NetworkChangeNotifier getInstance() {
        assert sInstance != null;
        return sInstance;
    }

    /**
     * Enables auto detection of the current network state based on notifications from the system.
     * Note that passing true here requires the embedding app have the platform ACCESS_NETWORK_STATE
     * permission.
     *
     * @param shouldAutoDetect true if the NetworkChangeNotifier should listen for system changes in
     *    network connectivity.
     */
    public static void setAutoDetectConnectivityState(boolean shouldAutoDetect) {
        getInstance().setAutoDetectConnectivityStateInternal(shouldAutoDetect, false);
    }

    private void destroyAutoDetector() {
        if (mAutoDetector != null) {
            mAutoDetector.destroy();
            mAutoDetector = null;
        }
    }

    /**
     * Registers to always receive network change notifications no matter if
     * the app is in the background or foreground.
     * Note that in normal circumstances, chrome embedders should use
     * {@code setAutoDetectConnectivityState} to listen to network changes only
     * when the app is in the foreground, because network change observers
     * might perform expensive work depending on the network connectivity.
     */
    public static void registerToReceiveNotificationsAlways() {
        getInstance().setAutoDetectConnectivityStateInternal(true, true);
    }

    private void setAutoDetectConnectivityStateInternal(
            boolean shouldAutoDetect, boolean alwaysWatchForChanges) {
        if (shouldAutoDetect) {
            if (mAutoDetector == null) {
                mAutoDetector = new NetworkChangeNotifierAutoDetect(
                    new NetworkChangeNotifierAutoDetect.Observer() {
                        @Override
                        public void onConnectionTypeChanged(int newConnectionType) {
                            updateCurrentConnectionType(newConnectionType);
                        }
                        @Override
                        public void onMaxBandwidthChanged(double maxBandwidthMbps) {
                            updateCurrentMaxBandwidth(maxBandwidthMbps);
                        }
                    },
                    mContext,
                    alwaysWatchForChanges);
                final NetworkChangeNotifierAutoDetect.NetworkState networkState =
                        mAutoDetector.getCurrentNetworkState();
                updateCurrentConnectionType(mAutoDetector.getCurrentConnectionType(networkState));
                updateCurrentMaxBandwidth(mAutoDetector.getCurrentMaxBandwidthInMbps(networkState));
            }
        } else {
            destroyAutoDetector();
        }
    }

    /**
     * Updates the perceived network state when not auto-detecting changes to connectivity.
     *
     * @param networkAvailable True if the NetworkChangeNotifier should perceive a "connected"
     *    state, false implies "disconnected".
     */
    @CalledByNative
    public static void forceConnectivityState(boolean networkAvailable) {
        setAutoDetectConnectivityState(false);
        getInstance().forceConnectivityStateInternal(networkAvailable);
    }

    private void forceConnectivityStateInternal(boolean forceOnline) {
        boolean connectionCurrentlyExists =
                mCurrentConnectionType != ConnectionType.CONNECTION_NONE;
        if (connectionCurrentlyExists != forceOnline) {
            updateCurrentConnectionType(forceOnline ? ConnectionType.CONNECTION_UNKNOWN
                    : ConnectionType.CONNECTION_NONE);
            updateCurrentMaxBandwidth(forceOnline ? Double.POSITIVE_INFINITY : 0.0);
        }
    }

    private void updateCurrentConnectionType(int newConnectionType) {
        mCurrentConnectionType = newConnectionType;
        notifyObserversOfConnectionTypeChange(newConnectionType);
    }

    private void updateCurrentMaxBandwidth(double maxBandwidthMbps) {
        if (maxBandwidthMbps == mCurrentMaxBandwidth) return;
        mCurrentMaxBandwidth = maxBandwidthMbps;
        notifyObserversOfMaxBandwidthChange(maxBandwidthMbps);
    }

    /**
     * Alerts all observers of a connection change.
     */
    void notifyObserversOfConnectionTypeChange(int newConnectionType) {
        for (Long nativeChangeNotifier : mNativeChangeNotifiers) {
            nativeNotifyConnectionTypeChanged(nativeChangeNotifier, newConnectionType);
        }
        for (ConnectionTypeObserver observer : mConnectionTypeObservers) {
            observer.onConnectionTypeChanged(newConnectionType);
        }
    }

    /**
     * Alerts all observers of a bandwidth change.
     */
    void notifyObserversOfMaxBandwidthChange(double maxBandwidthMbps) {
        for (Long nativeChangeNotifier : mNativeChangeNotifiers) {
            nativeNotifyMaxBandwidthChanged(nativeChangeNotifier, maxBandwidthMbps);
        }
    }

    /**
     * Adds an observer for any connection type changes.
     */
    public static void addConnectionTypeObserver(ConnectionTypeObserver observer) {
        getInstance().addConnectionTypeObserverInternal(observer);
    }

    private void addConnectionTypeObserverInternal(ConnectionTypeObserver observer) {
        mConnectionTypeObservers.addObserver(observer);
    }

    /**
     * Removes an observer for any connection type changes.
     */
    public static void removeConnectionTypeObserver(ConnectionTypeObserver observer) {
        getInstance().removeConnectionTypeObserverInternal(observer);
    }

    private void removeConnectionTypeObserverInternal(ConnectionTypeObserver observer) {
        mConnectionTypeObservers.removeObserver(observer);
    }

    @NativeClassQualifiedName("NetworkChangeNotifierDelegateAndroid")
    private native void nativeNotifyConnectionTypeChanged(long nativePtr, int newConnectionType);

    @NativeClassQualifiedName("NetworkChangeNotifierDelegateAndroid")
    private native void nativeNotifyMaxBandwidthChanged(long nativePtr, double maxBandwidthMbps);

    private static native double nativeGetMaxBandwidthForConnectionSubtype(int subtype);

    // For testing only.
    public static NetworkChangeNotifierAutoDetect getAutoDetectorForTest() {
        return getInstance().mAutoDetector;
    }

    /**
     * Checks if there currently is connectivity.
     */
    public static boolean isOnline() {
        int connectionType = getInstance().getCurrentConnectionType();
        return connectionType != ConnectionType.CONNECTION_UNKNOWN
                && connectionType != ConnectionType.CONNECTION_NONE;
    }
}
