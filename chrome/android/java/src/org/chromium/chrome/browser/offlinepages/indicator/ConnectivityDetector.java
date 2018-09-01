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
import android.os.Handler;
import android.os.SystemClock;
import android.support.annotation.IntDef;

import org.chromium.base.AsyncTask;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Class that detects the network connectivity. We will get the connectivity info from Android
 * connection manager if available, as in Marshmallow and above. Otherwise, we will do http probes
 * to verify that a well-known URL returns an expected result. If the result can't be validated,
 * we will retry with exponential backoff.
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

    // The result of the HTTP probing.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ProbeResult.NO_INTERNET, ProbeResult.SERVER_ERROR, ProbeResult.CAPTIVE_PORTAL,
            ProbeResult.VALIDATED})
    private @interface ProbeResult {
        // The network is connected, but it can't reach the Internet, i.e. connecting to a hotspot
        // that is not conencted to Internet.
        int NO_INTERNET = 1;
        // Server returns response code >= 400.
        int SERVER_ERROR = 2;
        // Capitve portal is determined.
        int CAPTIVE_PORTAL = 3;
        // Received the expected result from server.
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

    private static final String TAG = "OfflineIndicator";

    private static final String USER_AGENT_HEADER_NAME = "User-Agent";
    // Send the HTTPS probe first since the captive portals cannot see the encrypted URL path.
    private static final String DEFAULT_PROBE_URL = "https://www.google.com/generate_204";
    private static final String FALLBACK_PROBE_URL =
            "http://connectivitycheck.gstatic.com/generate_204";
    private static final String PROBE_METHOD = "GET";
    private static final int SOCKET_TIMEOUT_MS = 5000;
    private static final int CONNECTIVITY_CHECK_INITIAL_DELAY_MS = 5000;
    private static final int CONNECTIVITY_CHECK_MAX_DELAY_MS = 2 * 60 * 1000;

    private static boolean sSkipSystemCheckForTesting = false;
    private static String sDefaultProbeUrl = DEFAULT_PROBE_URL;
    private static String sFallbackProbeUrl = FALLBACK_PROBE_URL;
    private static String sProbeMethod = PROBE_METHOD;

    private Observer mObserver;

    private @ConnectionType int mConnectionType = ConnectionType.CONNECTION_UNKNOWN;
    private @ConnectionState int mConnectionState = ConnectionState.NONE;

    private String mUserAgentString = null;
    private int mConnectivityCheckDelayMs = 0;
    private Handler mHandler = null;
    private Runnable mRunnable = null;

    public ConnectivityDetector(Observer observer) {
        mObserver = observer;
        mHandler = new Handler();
        NetworkChangeNotifier.addConnectionTypeObserver(this);
        detect();
    }

    public void detect() {
        onConnectionTypeChanged(NetworkChangeNotifier.getInstance().getCurrentConnectionType());
    }

    public @ConnectionType int getConnectionState() {
        return mConnectionState;
    }

    @Override
    public void onConnectionTypeChanged(@ConnectionType int connectionType) {
        // This method may be called multiple times with same |connectionType|.
        if (mConnectionType == connectionType) {
            return;
        }
        mConnectionType = connectionType;
        Log.d(TAG, "onConnectionTypeChanged " + mConnectionType);

        // If not connected at all, no further check is needed.
        if (connectionType == ConnectionType.CONNECTION_NONE) {
            updateConnectionState(ConnectionState.DISCONNECTED);
            return;
        }

        // Check the Android system to determine the network connectivity. If unavailable, as in
        // Android version below Marshmallow, we will kick off our own probes.
        @ConnectionState
        int newConnectionState = getConnectionStateFromSystem();
        if (newConnectionState != ConnectionState.NONE) {
            updateConnectionState(newConnectionState);
            return;
        }

        // Do manual check via sending HTTP probes to server.
        mConnectivityCheckDelayMs = 0;
        performConnectivityCheck();
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
            Log.d(TAG, "Reported by system: " + capabilities.toString());
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

    private void performConnectivityCheck() {
        if (mUserAgentString == null) {
            mUserAgentString = ContentUtils.getBrowserUserAgent();
        }
        checkConnectivityViaDefaultUrl();
    }

    @VisibleForTesting
    void checkConnectivityViaDefaultUrl() {
        sendHttpProbe(sDefaultProbeUrl, SOCKET_TIMEOUT_MS, (result) -> {
            updateConnectionStatePerProbeResult(result);

            // Probe again with the fallback URL. The captive portal may react differently for
            // different url.
            if (result != ProbeResult.VALIDATED) {
                checkConnectivityViaFallbackUrl();
            }
        });
    }

    @VisibleForTesting
    void checkConnectivityViaFallbackUrl() {
        sendHttpProbe(sFallbackProbeUrl, SOCKET_TIMEOUT_MS, (result) -> {
            updateConnectionStatePerProbeResult(result);

            // Schedule the retry with backoff.
            if (result != ProbeResult.VALIDATED) {
                scheduleNextConnectivityCheck();
            }
        });
    }

    /**
     * Fetch a URL from a well-known server using Android system network stack.
     * Check asynchronously whether the device is currently connected to the Internet using the
     * Android system network stack. |callback| will be invoked with the boolean result to
     * denote if the connectivity is validated.
     */
    private void sendHttpProbe(
            final String urlString, final int timeoutMs, final Callback<Integer> callback) {
        new AsyncTask<Integer>() {
            @Override
            protected Integer doInBackground() {
                HttpURLConnection urlConnection = null;
                try {
                    URL url = new URL(urlString);
                    urlConnection = (HttpURLConnection) url.openConnection();
                    urlConnection.setInstanceFollowRedirects(false);
                    urlConnection.setRequestMethod(sProbeMethod);
                    urlConnection.setConnectTimeout(timeoutMs);
                    urlConnection.setReadTimeout(timeoutMs);
                    urlConnection.setUseCaches(false);
                    urlConnection.setRequestProperty(USER_AGENT_HEADER_NAME, mUserAgentString);

                    long requestTimestamp = SystemClock.elapsedRealtime();
                    urlConnection.connect();
                    long responseTimestamp = SystemClock.elapsedRealtime();
                    int responseCode = urlConnection.getResponseCode();

                    Log.d(TAG,
                            "Probe " + urlString + " time=" + (responseTimestamp - requestTimestamp)
                                    + "ms ret=" + responseCode
                                    + " headers=" + urlConnection.getHeaderFields());

                    if (responseCode == HttpURLConnection.HTTP_NO_CONTENT) {
                        return ProbeResult.VALIDATED;
                    } else if (responseCode >= 400) {
                        return ProbeResult.SERVER_ERROR;
                    } else if (responseCode == HttpURLConnection.HTTP_OK) {
                        // Treat 200 response with zero content length to not be a captive portal
                        // because the user cannot sign in to an empty page. Probably this is due to
                        // a broken transparent proxy.
                        if (urlConnection.getContentLength() == 0) {
                            return ProbeResult.VALIDATED;
                        } else if (urlConnection.getContentLength() == -1) {
                            // When no Content-length (default value == -1), attempt to read a byte
                            // from the response.
                            if (urlConnection.getInputStream().read() == -1) {
                                return ProbeResult.VALIDATED;
                            }
                        }
                    }
                } catch (IOException e) {
                    Log.d(TAG, "Probe " + urlString + " failed w/ exception " + e);
                    // Most likyly the exception is thrown due to host name not resolved or socket
                    // timeout.
                    return ProbeResult.NO_INTERNET;
                } finally {
                    if (urlConnection != null) {
                        urlConnection.disconnect();
                    }
                }
                return ProbeResult.CAPTIVE_PORTAL;
            }

            @Override
            protected void onPostExecute(Integer result) {
                callback.onResult(result);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void scheduleNextConnectivityCheck() {
        if (mConnectivityCheckDelayMs == 0) {
            mConnectivityCheckDelayMs = CONNECTIVITY_CHECK_INITIAL_DELAY_MS;
        } else {
            mConnectivityCheckDelayMs *= 2;
        }

        // Give up after exceeding the maximum delay.
        if (mConnectivityCheckDelayMs >= CONNECTIVITY_CHECK_MAX_DELAY_MS) {
            Log.d(TAG, "No more retry after exceeding " + CONNECTIVITY_CHECK_MAX_DELAY_MS + "ms");
            if (mConnectionState == ConnectionState.NONE) {
                updateConnectionState(ConnectionState.NO_INTERNET);
            }
            return;
        }
        Log.d(TAG, "Retry after " + mConnectivityCheckDelayMs + "ms");

        mRunnable = new Runnable() {
            @Override
            public void run() {
                performConnectivityCheck();
            }
        };
        mHandler.postDelayed(mRunnable, mConnectivityCheckDelayMs);
    }

    private void updateConnectionStatePerProbeResult(@ProbeResult int result) {
        @ConnectionState
        int newConnectionState = mConnectionState;
        switch (result) {
            case ProbeResult.VALIDATED:
                newConnectionState = ConnectionState.VALIDATED;
                break;
            case ProbeResult.CAPTIVE_PORTAL:
                newConnectionState = ConnectionState.CAPTIVE_PORTAL;
                break;
            case ProbeResult.NO_INTERNET:
                newConnectionState = ConnectionState.NO_INTERNET;
                break;
            case ProbeResult.SERVER_ERROR:
                // Don't update the connection state if there is a server error which should
                // be rare.
                break;
        }
        updateConnectionState(newConnectionState);
    }

    @VisibleForTesting
    void updateConnectionState(@ConnectionState int connectionState) {
        if (mConnectionState == connectionState) return;
        mConnectionState = connectionState;
        mObserver.onConnectionStateChanged(mConnectionState);
    }

    @VisibleForTesting
    static void skipSystemCheckForTesting() {
        sSkipSystemCheckForTesting = true;
    }

    @VisibleForTesting
    static void overrideDefaultProbeUrlForTesting(String url) {
        sDefaultProbeUrl = url;
    }

    @VisibleForTesting
    static void overrideFallbackProbeUrlForTesting(String url) {
        sFallbackProbeUrl = url;
    }

    @VisibleForTesting
    static void overrideProbeMethodForTesting(String method) {
        sProbeMethod = method;
    }

    @VisibleForTesting
    void forceConnectionStateForTesting(@ConnectionState int connectionState) {
        mConnectionState = connectionState;
    }

    @VisibleForTesting
    Handler getHandlerForTesting() {
        return mHandler;
    }
}
