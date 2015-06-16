// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.HashMap;
import java.util.Map;

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
    private final ConnectivityTask mConnectivityTask;

    /**
     * An optional description for the feedback report.
     */
    private String mDescription;

    /**
     * An optional screenshot for the feedback report.
     */
    private Bitmap mScreenshot;

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
        mConnectivityTask = ConnectivityTask.create(mProfile, DEFAULT_ASYNC_COLLECTION_TIMEOUT_MS);
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
     * Sets the default description to invoke feedback with.
     * @param description the user visible description.
     */
    public void setDescription(String description) {
        ThreadUtils.assertOnUiThread();
        mDescription = description;
    }

    /**
     * @return the default description to invoke feedback with.
     */
    public String getDescription() {
        ThreadUtils.assertOnUiThread();
        return mDescription;
    }

    /**
     * Sets the screenshot to use for the feedback report.
     * @param screenshot the user visible screenshot.
     */
    public void setScreenshot(Bitmap screenshot) {
        ThreadUtils.assertOnUiThread();
        mScreenshot = screenshot;
    }

    /**
     * @return the screenshot to use for the feedback report.
     */
    public Bitmap getScreenshot() {
        ThreadUtils.assertOnUiThread();
        return mScreenshot;
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
        Map<String, String> connectivityMap = mConnectivityTask.get().toMap();
        mData.putAll(connectivityMap);
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
