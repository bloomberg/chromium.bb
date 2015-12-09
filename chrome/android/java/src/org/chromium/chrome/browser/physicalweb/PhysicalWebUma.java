// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.preference.PreferenceManager;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import org.json.JSONArray;
import org.json.JSONException;

import java.util.concurrent.TimeUnit;

import javax.annotation.concurrent.ThreadSafe;

/**
 * Centralizes UMA data collection for the Physical Web feature.
 */
@ThreadSafe
class PhysicalWebUma {
    private static final String TAG = "PhysicalWeb";
    private static final String NOTIFICATION_PRESS_COUNT = "PhysicalWeb.NotificationPressed";
    private static final String PWS_RESPONSE_TIMES = "PhysicalWeb.RoundTripTimeMilliseconds";
    private static final String URL_SELECTED_COUNT = "PhysicalWeb.UrlSelected";
    private static final String URLS_DISPLAYED_COUNTS = "PhysicalWeb.TotalBeaconsDetected";
    private static boolean sUploadAllowed = false;

    /**
     * Records a notification press.
     */
    public static void onNotificationPressed(Context context) {
        handleAction(context, NOTIFICATION_PRESS_COUNT);
    }

    /**
     * Records a URL selection.
     */
    public static void onUrlSelected(Context context) {
        handleAction(context, URL_SELECTED_COUNT);
    }

    /**
     * Records a response time from PWS.
     * @param duration The length of time PWS took to respond.
     */
    public static void onPwsResponse(Context context, long duration) {
        if (sUploadAllowed) {
            RecordHistogram.recordTimesHistogram(PWS_RESPONSE_TIMES, duration,
                                                 TimeUnit.MILLISECONDS);
        } else {
            storeValue(context, PWS_RESPONSE_TIMES, duration);
        }
    }

    /**
     * Records number of URLs displayed to a user.
     * @param numUrls The number of URLs displayed to a user.
     */
    public static void onUrlsDisplayed(Context context, int numUrls) {
        if (sUploadAllowed) {
            RecordHistogram.recordCountHistogram(URLS_DISPLAYED_COUNTS, numUrls);
        } else {
            storeValue(context, URLS_DISPLAYED_COUNTS, numUrls);
        }
    }

    /**
     * Uploads metrics that we have deferred for uploading.
     * Additionally, this method will cause future stat records not to be deferred and instead
     * uploaded immediately.
     */
    public static void uploadDeferredMetrics(Context context) {
        // If uploads have been explicitely requested, they are now allowed.
        sUploadAllowed = true;

        // Read the metrics.
        UmaUploader uploader = new UmaUploader();
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        uploader.notificationPressCount = prefs.getInt(NOTIFICATION_PRESS_COUNT, 0);
        uploader.urlSelectedCount = prefs.getInt(URL_SELECTED_COUNT, 0);
        uploader.pwsResponseTimes = prefs.getString(PWS_RESPONSE_TIMES, "[]");
        uploader.urlsDisplayedCounts = prefs.getString(URLS_DISPLAYED_COUNTS, "[]");

        // If the metrics are empty, we are done.
        if (uploader.isEmpty()) {
            return;
        }

        // Clear out the stored deferred metrics that we are about to upload.
        prefs.edit()
                .remove(NOTIFICATION_PRESS_COUNT)
                .remove(URL_SELECTED_COUNT)
                .remove(PWS_RESPONSE_TIMES)
                .remove(URLS_DISPLAYED_COUNTS)
                .apply();

        // Finally, upload the metrics.
        AsyncTask.THREAD_POOL_EXECUTOR.execute(uploader);
    }

    private static void storeAction(Context context, String key) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        SharedPreferences.Editor prefsEditor = prefs.edit();
        int count = prefs.getInt(key, 0);
        prefsEditor.putInt(key, count + 1).apply();
    }

    private static void storeValue(Context context, String key, Object value) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        SharedPreferences.Editor prefsEditor = prefs.edit();
        JSONArray values = null;
        try {
            values = new JSONArray(prefs.getString(key, "[]"));
            values.put(value);
        } catch (JSONException e) {
            Log.e(TAG, "JSONException when storing " + key + " stats", e);
            prefsEditor.remove(key).apply();
            return;
        }
        prefsEditor.putString(key, values.toString()).apply();
    }

    private static void handleAction(Context context, String key) {
        if (sUploadAllowed) {
            RecordUserAction.record(key);
        } else {
            storeAction(context, key);
        }
    }

    private static class UmaUploader implements Runnable {
        public int notificationPressCount;
        public int urlSelectedCount;
        public String pwsResponseTimes;
        public String urlsDisplayedCounts;

        public boolean isEmpty() {
            return notificationPressCount == 0
                    && urlSelectedCount == 0
                    && pwsResponseTimes.equals("[]")
                    && urlsDisplayedCounts.equals("[]");
        }

        UmaUploader() {
        }

        @Override
        public void run() {
            uploadActions(notificationPressCount, NOTIFICATION_PRESS_COUNT);
            uploadActions(urlSelectedCount, URL_SELECTED_COUNT);
            uploadTimes(pwsResponseTimes, PWS_RESPONSE_TIMES, TimeUnit.MILLISECONDS);
            uploadCounts(urlsDisplayedCounts, URLS_DISPLAYED_COUNTS);
        }

        private static void uploadActions(int count, String key) {
            for (int i = 0; i < count; i++) {
                RecordUserAction.record(key);
            }
        }

        private static Object[] parseJsonArray(String jsonArrayStr, Class<?> itemType) {
            try {
                JSONArray values = new JSONArray(jsonArrayStr);
                Object[] arr = new Object[values.length()];
                for (int i = 0; i < values.length(); i++) {
                    arr[i] = values.get(i);
                    if (arr[i].getClass() != itemType) return null;
                }
                return arr;
            } catch (JSONException e) {
                return null;
            }
        }

        private static void uploadTimes(String jsonTimesStr, final String key, final TimeUnit tu) {
            Long[] times = (Long[]) parseJsonArray(jsonTimesStr, Long.class);
            if (times == null) {
                Log.e(TAG, "Error reporting " + key);
                return;
            }
            for (Long time : times) {
                RecordHistogram.recordTimesHistogram(key, time, TimeUnit.MILLISECONDS);
            }
        }

        private static void uploadCounts(String jsonCountsStr, final String key) {
            Integer[] counts = (Integer[]) parseJsonArray(jsonCountsStr, Long.class);
            if (counts == null) {
                Log.e(TAG, "Error reporting " + key);
                return;
            }
            for (Integer count: counts) {
                RecordHistogram.recordCountHistogram(key, count);
            }
        }
    }
}
