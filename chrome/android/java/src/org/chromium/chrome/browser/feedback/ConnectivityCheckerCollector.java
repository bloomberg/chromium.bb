// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.EnumMap;
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
     * The type of network stack and connectivity check this result is about.
     */
    public enum Type { CHROME_HTTP, CHROME_HTTPS, SYSTEM_HTTP, SYSTEM_HTTPS }

    /**
     * The result from the connectivity check. Will be {@link Result#UNKNOWN} if the result is not
     * ready yet.
     */
    public enum Result { UNKNOWN, NOT_CONNECTED, CONNECTED }

    private static class ConnectivityCheckerFuture implements Future<Map<Type, Result>> {
        private final Map<Type, Result> mResult = new EnumMap<Type, Result>(Type.class);

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
        public Map<Type, Result> get() {
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
            return result;
        }

        @Override
        public Map<Type, Result> get(long l, TimeUnit timeUnit) {
            return get();
        }

        /**
         * Starts a separate connectivity check for each {@link Type}.
         */
        public void startChecks(Profile profile, int timeoutMs) {
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
    public static Future<Map<Type, Result>> startChecks(Profile profile, int timeoutMs) {
        ThreadUtils.assertOnUiThread();
        ConnectivityCheckerFuture future = new ConnectivityCheckerFuture();
        future.startChecks(profile, timeoutMs);
        return future;
    }
}
