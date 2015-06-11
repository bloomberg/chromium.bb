// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;

import javax.annotation.Nullable;

/**
 * A class which collects generic information about Chrome which is useful for all types of
 * feedback, and provides it as a {@link Bundle}.
 *
 * Creating a {@link FeedbackCollector} initiates asynchronous operations for gathering feedback
 * data, which may or not finish before the bundle is requested by calling {@link #getBundle()}.
 *
 * Interacting with the {@link FeedbackCollector} is only allowed on the main thread.
 */
public class FeedbackCollector {
    private static final String TAG = "cr.feedback";

    /**
     * A user visible string describing the current URL.
     */
    private static final String URL_KEY = "URL";

    /**
     * A user visible string describing whether the data reduction proxy is enabled.
     */
    private static final String DATA_REDUCTION_PROXY_ENABLED_KEY = "Data reduction proxy enabled";

    /**
     * The default timeout for gathering data asynchronously.
     */
    private static final int DEFAULT_ASYNC_COLLECTION_TIMEOUT_MS = 1000;

    private final Map<String, String> mData;
    private final Profile mProfile;
    private final String mUrl;
    private final Future<ConnectivityCheckerCollector.FeedbackData> mConnectivityFuture;

    /**
     * Creates a {@link FeedbackCollector} and starts asynchronous operations to gather extra data.
     * @param profile the current Profile.
     * @param url The URL of the current tab to include in the feedback the user sends, if any.
     *            This parameter may be null.
     * @return the created {@link FeedbackCollector}.
     */
    public static FeedbackCollector create(Profile profile, @Nullable String url) {
        ThreadUtils.assertOnUiThread();
        return new FeedbackCollector(profile, url);
    }

    private FeedbackCollector(Profile profile, String url) {
        mData = new HashMap<>();
        mProfile = profile;
        mUrl = url;
        mConnectivityFuture = ConnectivityCheckerCollector.startChecks(
                mProfile, DEFAULT_ASYNC_COLLECTION_TIMEOUT_MS);
    }

    /**
     * Adds a key-value pair of data to be included in the feedback report. This data
     * is user visible and should only contain single-line forms of data, not long Strings.
     * @param key the user visible key.
     * @param value the user visible value.
     */
    public void add(String key, String value) {
        ThreadUtils.assertOnUiThread();
        mData.put(key, value);
    }

    /**
     * @return the collected data as a {@link Bundle}.
     */
    public Bundle getBundle() {
        ThreadUtils.assertOnUiThread();
        addUrl();
        addConnectivityData();
        addDataReductionProxyData();
        return asBundle();
    }

    private void addUrl() {
        if (!TextUtils.isEmpty(mUrl)) {
            mData.put(URL_KEY, mUrl);
        }
    }

    private void addConnectivityData() {
        try {
            Map<String, String> connectivityMap = mConnectivityFuture.get().toMap();
            mData.putAll(connectivityMap);
        } catch (InterruptedException | ExecutionException e) {
            Log.w(TAG, "Failed to successfully get connectivity data.", e.getMessage());
        }
    }

    private void addDataReductionProxyData() {
        mData.put(DATA_REDUCTION_PROXY_ENABLED_KEY,
                String.valueOf(
                        DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()));
    }

    private Bundle asBundle() {
        Bundle bundle = new Bundle();
        for (Map.Entry<String, String> entry : mData.entrySet()) {
            bundle.putString(entry.getKey(), entry.getValue());
        }
        return bundle;
    }
}
