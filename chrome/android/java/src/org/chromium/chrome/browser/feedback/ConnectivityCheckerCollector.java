// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.os.SystemClock;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Collections;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

/**
 * A utility class for checking if the device is currently connected to the Internet by using
 * both available network stacks, and checking over both HTTP and HTTPS.
 */
public class ConnectivityCheckerCollector {
    private static final String TAG = "Feedback";

    /**
     * The key for the data describing the timeout that was set as a maximum for collecting
     * the connection data. This is to better understand the connection data.
     * This string is user visible.
     */
    @VisibleForTesting
    static final String CONNECTION_CHECK_TIMEOUT_MS = "Connection check timeout (ms)";

    /**
     * The key for the data describing how long time from the connection check was started,
     * until the data was collected. This is to better understand the connection data.
     * This string is user visible.
     */
    @VisibleForTesting
    static final String CONNECTION_CHECK_ELAPSED_MS = "Connection check elapsed (ms)";

    /**
     * The key for the data describing whether Chrome was able to successfully connect to the
     * HTTP connection check URL using the Chrome network stack.
     * This string is user visible.
     */
    @VisibleForTesting
    static final String CHROME_HTTP_KEY = "HTTP connection check (Chrome network stack)";

    /**
     * The key for the data describing whether Chrome was able to successfully connect to the
     * HTTPS connection check URL using the Chrome network stack.
     * This string is user visible.
     */
    @VisibleForTesting
    static final String CHROME_HTTPS_KEY = "HTTPS connection check (Chrome network stack)";

    /**
     * The key for the data describing whether Chrome was able to successfully connect to the
     * HTTP connection check URL using the Android network stack.
     * This string is user visible.
     */
    @VisibleForTesting
    static final String SYSTEM_HTTP_KEY = "HTTP connection check (Android network stack)";

    /**
     * The key for the data describing whether Chrome was able to successfully connect to the
     * HTTPS connection check URL using the Android network stack.
     * This string is user visible.
     */
    @VisibleForTesting
    static final String SYSTEM_HTTPS_KEY = "HTTPS connection check (Android network stack)";

    /**
     * FeedbackData contains the set of information that is to be included in a feedback report.
     */
    public static final class FeedbackData {
        private final Map<Type, Result> mConnections;
        private final int mTimeoutMs;
        private final long mElapsedTimeMs;

        private static String getHumanReadableString(Type type) {
            switch (type) {
                case CHROME_HTTP:
                    return CHROME_HTTP_KEY;
                case CHROME_HTTPS:
                    return CHROME_HTTPS_KEY;
                case SYSTEM_HTTP:
                    return SYSTEM_HTTP_KEY;
                case SYSTEM_HTTPS:
                    return SYSTEM_HTTPS_KEY;
                default:
                    throw new IllegalArgumentException("Unknown connection type: " + type);
            }
        }

        FeedbackData(Map<Type, Result> connections, int timeoutMs, long elapsedTimeMs) {
            mConnections = connections;
            mTimeoutMs = timeoutMs;
            mElapsedTimeMs = elapsedTimeMs;
        }

        /**
         * @return a {@link Map} with information about connection status for different connection
         * types.
         */
        @VisibleForTesting
        Map<Type, Result> getConnections() {
            return Collections.unmodifiableMap(mConnections);
        }

        /**
         * @return the timeout that was used for data collection.
         */
        @VisibleForTesting
        int getTimeoutMs() {
            return mTimeoutMs;
        }

        /**
         * @return the time that was used from starting the check until data was gathered.
         */
        @VisibleForTesting
        long getElapsedTimeMs() {
            return mElapsedTimeMs;
        }

        /**
         * @return a {@link Map} with all the data fields for this feedback.
         */
        public Map<String, String> toMap() {
            Map<String, String> map = new HashMap<>();
            for (Map.Entry<Type, Result> entry : mConnections.entrySet()) {
                map.put(getHumanReadableString(entry.getKey()), entry.getValue().name());
            }
            map.put(CONNECTION_CHECK_TIMEOUT_MS, String.valueOf(mTimeoutMs));
            map.put(CONNECTION_CHECK_ELAPSED_MS, String.valueOf(mElapsedTimeMs));
            return map;
        }
    }

    /**
     * The type of network stack and connectivity check this result is about.
     */
    public enum Type { CHROME_HTTP, CHROME_HTTPS, SYSTEM_HTTP, SYSTEM_HTTPS }

    /**
     * The result from the connectivity check. Will be {@link Result#UNKNOWN} if the result is not
     * ready yet.
     */
    public enum Result { UNKNOWN, NOT_CONNECTED, CONNECTED }

    private static class ConnectivityCheckerFuture implements Future<FeedbackData> {
        private final Map<Type, Result> mResult = new EnumMap<Type, Result>(Type.class);
        // These timing values are set to -1 to signify an error until they are correctly set
        // when starting the checks and gathering the data.
        private int mTimeoutMs = -1;
        private long mStartCheckTimeMs = -1;

        private class SingleTypeTask implements ConnectivityChecker.ConnectivityCheckerCallback {
            private final Type mType;

            public SingleTypeTask(Type type) {
                mType = type;
            }

            /**
             * Starts the current task by calling the appropriate method on the
             * {@link ConnectivityChecker}.
             * The result will be put in {@link #mResult} when it comes back from the network stack.
             */
            public void start(Profile profile, int timeoutMs) {
                Log.v(TAG, "Starting task for " + mType);
                switch (mType) {
                    case CHROME_HTTP:
                        ConnectivityChecker.checkConnectivityChromeNetworkStack(
                                profile, false, timeoutMs, this);
                        break;
                    case CHROME_HTTPS:
                        ConnectivityChecker.checkConnectivityChromeNetworkStack(
                                profile, true, timeoutMs, this);
                        break;
                    case SYSTEM_HTTP:
                        ConnectivityChecker.checkConnectivitySystemNetworkStack(
                                false, timeoutMs, this);
                        break;
                    case SYSTEM_HTTPS:
                        ConnectivityChecker.checkConnectivitySystemNetworkStack(
                                true, timeoutMs, this);
                        break;
                    default:
                        Log.e(TAG, "Failed to recognize type " + mType);
                }
            }

            @Override
            public void onResult(boolean connected) {
                Log.v(TAG, "Got result for " + mType + ": connected = " + connected);
                Result result = connected ? Result.CONNECTED : Result.NOT_CONNECTED;
                mResult.put(mType, result);
            }
        }

        @Override
        public boolean cancel(boolean mayInterruptIfRunning) {
            // Cancel is not supported.
            return false;
        }

        @Override
        public boolean isCancelled() {
            // Cancel is not supported.
            return false;
        }

        @Override
        public boolean isDone() {
            ThreadUtils.assertOnUiThread();
            return mResult.size() == Type.values().length;
        }

        @Override
        public FeedbackData get() {
            ThreadUtils.assertOnUiThread();
            Map<Type, Result> result = new EnumMap<Type, Result>(Type.class);
            // Ensure the map is filled with a result for all {@link Type}s.
            for (Type type : Type.values()) {
                if (mResult.containsKey(type)) {
                    result.put(type, mResult.get(type));
                } else {
                    result.put(type, Result.UNKNOWN);
                }
            }
            long elapsedTimeMs = SystemClock.elapsedRealtime() - mStartCheckTimeMs;
            return new FeedbackData(result, mTimeoutMs, elapsedTimeMs);
        }

        @Override
        public FeedbackData get(long l, TimeUnit timeUnit) {
            return get();
        }

        /**
         * Starts a separate connectivity check for each {@link Type}.
         */
        public void startChecks(Profile profile, int timeoutMs) {
            mTimeoutMs = timeoutMs;
            mStartCheckTimeMs = SystemClock.elapsedRealtime();
            for (Type t : Type.values()) {
                SingleTypeTask task = new SingleTypeTask(t);
                task.start(profile, timeoutMs);
            }
        }
    }

    /**
     * Starts an asynchronous request for checking whether the device is currently connected to the
     * Internet using both the Chrome and the Android system network stack. The result is available
     * in the {@link Future} returned from this method.
     *
     * The {@link Future} returned from this method is not cancelable, and regardless of which
     * method is used to get the result, whatever has already been gathered of data from the
     * different requests will be returned. This means that there will be no wait-time for calling
     * get() and it must be called from the main thread. The {@link Future#isDone} method can be
     * used to see if all requests have been completed. It is OK to get the result before
     * {@link Future#isDone()} returns true.
     *
     * @param profile the context to do the check in.
     * @param timeoutMs number of milliseconds to wait before giving up waiting for a connection.
     * @return a Future to retrieve the results.
     */
    public static Future<FeedbackData> startChecks(Profile profile, int timeoutMs) {
        ThreadUtils.assertOnUiThread();
        ConnectivityCheckerFuture future = new ConnectivityCheckerFuture();
        future.startChecks(profile, timeoutMs);
        return future;
    }
}
