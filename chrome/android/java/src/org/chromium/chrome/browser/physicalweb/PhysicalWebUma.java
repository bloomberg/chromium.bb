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
public class PhysicalWebUma {
    private static final String TAG = "PhysicalWeb";
    private static final String OPT_IN_DECLINE_BUTTON_PRESS_COUNT =
            "PhysicalWeb.OptIn.DeclineButtonPressed";
    private static final String OPT_IN_ENABLE_BUTTON_PRESS_COUNT =
            "PhysicalWeb.OptIn.EnableButtonPressed";
    private static final String OPT_IN_HIGH_PRIORITY_NOTIFICATION_COUNT =
            "PhysicalWeb.OptIn.HighPriorityNotificationShown";
    private static final String OPT_IN_MIN_PRIORITY_NOTIFICATION_COUNT =
            "PhysicalWeb.OptIn.MinPriorityNotificationShown";
    private static final String OPT_IN_NOTIFICATION_PRESS_COUNT =
            "PhysicalWeb.OptIn.NotificationPressed";
    private static final String PREFS_FEATURE_DISABLED_COUNT = "PhysicalWeb.Prefs.FeatureDisabled";
    private static final String PREFS_FEATURE_ENABLED_COUNT = "PhysicalWeb.Prefs.FeatureEnabled";
    private static final String PREFS_LOCATION_DENIED_COUNT = "PhysicalWeb.Prefs.LocationDenied";
    private static final String PREFS_LOCATION_GRANTED_COUNT = "PhysicalWeb.Prefs.LocationGranted";
    private static final String PWS_BACKGROUND_RESOLVE_TIMES = "PhysicalWeb.ResolveTime.Background";
    private static final String PWS_FOREGROUND_RESOLVE_TIMES = "PhysicalWeb.ResolveTime.Foreground";
    private static final String OPT_IN_NOTIFICATION_PRESS_DELAYS =
            "PhysicalWeb.ReferralDelay.OptInNotification";
    private static final String STANDARD_NOTIFICATION_PRESS_DELAYS =
            "PhysicalWeb.ReferralDelay.StandardNotification";
    private static final String URL_SELECTED_COUNT = "PhysicalWeb.UrlSelected";
    private static final String TOTAL_URLS_INITIAL_COUNTS =
            "PhysicalWeb.TotalUrls.OnInitialDisplay";
    private static final String TOTAL_URLS_REFRESH_COUNTS =
            "PhysicalWeb.TotalUrls.OnRefresh";
    private static final String ACTIVITY_REFERRALS = "PhysicalWeb.ActivityReferral";
    private static boolean sUploadAllowed = false;

    /**
     * Records a URL selection.
     */
    public static void onUrlSelected(Context context) {
        handleAction(context, URL_SELECTED_COUNT);
    }

    /**
     * Records a tap on the opt-in decline button.
     */
    public static void onOptInDeclineButtonPressed(Context context) {
        handleAction(context, OPT_IN_DECLINE_BUTTON_PRESS_COUNT);
    }

    /**
     * Records a tap on the opt-in enable button.
     */
    public static void onOptInEnableButtonPressed(Context context) {
        handleAction(context, OPT_IN_ENABLE_BUTTON_PRESS_COUNT);
    }

    /**
     * Records a display of a high priority opt-in notification.
     */
    public static void onOptInHighPriorityNotificationShown(Context context) {
        handleAction(context, OPT_IN_HIGH_PRIORITY_NOTIFICATION_COUNT);
    }

    /**
     * Records a display of a min priority opt-in notification.
     */
    public static void onOptInMinPriorityNotificationShown(Context context) {
        handleAction(context, OPT_IN_MIN_PRIORITY_NOTIFICATION_COUNT);
    }

    /**
     * Records a display of the opt-in activity.
     */
    public static void onOptInNotificationPressed(Context context) {
        handleAction(context, OPT_IN_NOTIFICATION_PRESS_COUNT);
    }

    /**
     * Records when the user disables the Physical Web fetaure.
     */
    public static void onPrefsFeatureDisabled(Context context) {
        handleAction(context, PREFS_FEATURE_DISABLED_COUNT);
    }

    /**
     * Records when the user enables the Physical Web fetaure.
     */
    public static void onPrefsFeatureEnabled(Context context) {
        handleAction(context, PREFS_FEATURE_ENABLED_COUNT);
    }

    /**
     * Records when the user denies the location permission when enabling the Physical Web from the
     * privacy settings menu.
     */
    public static void onPrefsLocationDenied(Context context) {
        handleAction(context, PREFS_LOCATION_DENIED_COUNT);
    }

    /**
     * Records when the user grants the location permission when enabling the Physical Web from the
     * privacy settings menu.
     */
    public static void onPrefsLocationGranted(Context context) {
        handleAction(context, PREFS_LOCATION_GRANTED_COUNT);
    }

    /**
     * Records a response time from PWS for a resolution during a background scan.
     * @param duration The length of time PWS took to respond.
     */
    public static void onBackgroundPwsResolution(Context context, long duration) {
        handleTime(context, PWS_BACKGROUND_RESOLVE_TIMES, duration, TimeUnit.MILLISECONDS);
    }

    /**
     * Records a response time from PWS for a resolution during a foreground scan.
     * @param duration The length of time PWS took to respond.
     */
    public static void onForegroundPwsResolution(Context context, long duration) {
        handleTime(context, PWS_FOREGROUND_RESOLVE_TIMES, duration, TimeUnit.MILLISECONDS);
    }

    /**
     * Records number of URLs displayed to a user when the URL list is first displayed.
     * @param numUrls The number of URLs displayed to a user.
     */
    public static void onUrlsDisplayed(Context context, int numUrls) {
        if (sUploadAllowed) {
            RecordHistogram.recordCountHistogram(TOTAL_URLS_INITIAL_COUNTS, numUrls);
        } else {
            storeValue(context, TOTAL_URLS_INITIAL_COUNTS, numUrls);
        }
    }

    /**
     * Records number of URLs displayed to a user when the user refreshes the URL list.
     * @param numUrls The number of URLs displayed to a user.
     */
    public static void onUrlsRefreshed(Context context, int numUrls) {
        if (sUploadAllowed) {
            RecordHistogram.recordCountHistogram(TOTAL_URLS_REFRESH_COUNTS, numUrls);
        } else {
            storeValue(context, TOTAL_URLS_REFRESH_COUNTS, numUrls);
        }
    }

    /**
     * Records a ListUrlActivity referral.
     * @param refer The type of referral.  This enum is listed as PhysicalWebActivityReferer in
     *     histograms.xml.
     */
    public static void onActivityReferral(Context context, int referer) {
        if (sUploadAllowed) {
            RecordHistogram.recordEnumeratedHistogram(
                    ACTIVITY_REFERRALS, referer, ListUrlsActivity.REFERER_BOUNDARY);
        } else {
            storeValue(context, ACTIVITY_REFERRALS, referer);
        }
        long delay;
        switch (referer) {
            case ListUrlsActivity.NOTIFICATION_REFERER:
                delay = UrlManager.getInstance(context).getTimeSinceNotificationUpdate();
                handleTime(context, STANDARD_NOTIFICATION_PRESS_DELAYS, delay,
                        TimeUnit.MILLISECONDS);
                break;
            case ListUrlsActivity.OPTIN_REFERER:
                delay = UrlManager.getInstance(context).getTimeSinceNotificationUpdate();
                handleTime(context, OPT_IN_NOTIFICATION_PRESS_DELAYS, delay,
                        TimeUnit.MILLISECONDS);
                break;
            default:
                break;
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
        uploader.urlSelectedCount = prefs.getInt(URL_SELECTED_COUNT, 0);
        uploader.optInDeclineButtonTapCount = prefs.getInt(OPT_IN_DECLINE_BUTTON_PRESS_COUNT, 0);
        uploader.optInEnableButtonTapCount = prefs.getInt(OPT_IN_ENABLE_BUTTON_PRESS_COUNT, 0);
        uploader.optInHighPriorityNotificationCount =
                prefs.getInt(OPT_IN_HIGH_PRIORITY_NOTIFICATION_COUNT, 0);
        uploader.optInMinPriorityNotificationCount =
                prefs.getInt(OPT_IN_MIN_PRIORITY_NOTIFICATION_COUNT, 0);
        uploader.optInNotificationPressCount = prefs.getInt(OPT_IN_NOTIFICATION_PRESS_COUNT, 0);
        uploader.prefsFeatureDisabledCount = prefs.getInt(PREFS_FEATURE_DISABLED_COUNT, 0);
        uploader.prefsFeatureEnabledCount = prefs.getInt(PREFS_FEATURE_ENABLED_COUNT, 0);
        uploader.prefsLocationDeniedCount = prefs.getInt(PREFS_LOCATION_DENIED_COUNT, 0);
        uploader.prefsLocationGrantedCount = prefs.getInt(PREFS_LOCATION_GRANTED_COUNT, 0);
        uploader.pwsBackgroundResolveTimes = prefs.getString(PWS_BACKGROUND_RESOLVE_TIMES, "[]");
        uploader.pwsForegroundResolveTimes = prefs.getString(PWS_FOREGROUND_RESOLVE_TIMES, "[]");
        uploader.standardNotificationPressDelays =
                prefs.getString(STANDARD_NOTIFICATION_PRESS_DELAYS, "[]");
        uploader.optInNotificationPressDelays =
                prefs.getString(OPT_IN_NOTIFICATION_PRESS_DELAYS, "[]");
        uploader.totalUrlsInitialCounts = prefs.getString(TOTAL_URLS_INITIAL_COUNTS, "[]");
        uploader.totalUrlsRefreshCounts = prefs.getString(TOTAL_URLS_REFRESH_COUNTS, "[]");
        uploader.activityReferrals = prefs.getString(ACTIVITY_REFERRALS, "[]");

        // If the metrics are empty, we are done.
        if (uploader.isEmpty()) {
            return;
        }

        // Clear out the stored deferred metrics that we are about to upload.
        prefs.edit()
                .remove(URL_SELECTED_COUNT)
                .remove(OPT_IN_DECLINE_BUTTON_PRESS_COUNT)
                .remove(OPT_IN_ENABLE_BUTTON_PRESS_COUNT)
                .remove(OPT_IN_HIGH_PRIORITY_NOTIFICATION_COUNT)
                .remove(OPT_IN_MIN_PRIORITY_NOTIFICATION_COUNT)
                .remove(OPT_IN_NOTIFICATION_PRESS_COUNT)
                .remove(PREFS_FEATURE_DISABLED_COUNT)
                .remove(PREFS_FEATURE_ENABLED_COUNT)
                .remove(PREFS_LOCATION_DENIED_COUNT)
                .remove(PREFS_LOCATION_GRANTED_COUNT)
                .remove(PWS_BACKGROUND_RESOLVE_TIMES)
                .remove(PWS_FOREGROUND_RESOLVE_TIMES)
                .remove(STANDARD_NOTIFICATION_PRESS_DELAYS)
                .remove(OPT_IN_NOTIFICATION_PRESS_DELAYS)
                .remove(TOTAL_URLS_INITIAL_COUNTS)
                .remove(TOTAL_URLS_REFRESH_COUNTS)
                .remove(ACTIVITY_REFERRALS)
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

    private static void handleTime(Context context, String key, long duration, TimeUnit tu) {
        if (sUploadAllowed) {
            RecordHistogram.recordTimesHistogram(key, duration, tu);
        } else {
            storeValue(context, key, duration);
        }
    }

    private static class UmaUploader implements Runnable {
        public int urlSelectedCount;
        public int optInDeclineButtonTapCount;
        public int optInEnableButtonTapCount;
        public int optInHighPriorityNotificationCount;
        public int optInMinPriorityNotificationCount;
        public int optInNotificationPressCount;
        public int prefsFeatureDisabledCount;
        public int prefsFeatureEnabledCount;
        public int prefsLocationDeniedCount;
        public int prefsLocationGrantedCount;
        public String pwsBackgroundResolveTimes;
        public String pwsForegroundResolveTimes;
        public String standardNotificationPressDelays;
        public String optInNotificationPressDelays;
        public String totalUrlsInitialCounts;
        public String totalUrlsRefreshCounts;
        public String activityReferrals;

        public boolean isEmpty() {
            return urlSelectedCount == 0
                    && optInDeclineButtonTapCount == 0
                    && optInEnableButtonTapCount == 0
                    && optInHighPriorityNotificationCount == 0
                    && optInMinPriorityNotificationCount == 0
                    && optInNotificationPressCount == 0
                    && prefsFeatureDisabledCount == 0
                    && prefsFeatureEnabledCount == 0
                    && prefsLocationDeniedCount == 0
                    && prefsLocationGrantedCount == 0
                    && pwsBackgroundResolveTimes.equals("[]")
                    && pwsForegroundResolveTimes.equals("[]")
                    && standardNotificationPressDelays.equals("[]")
                    && optInNotificationPressDelays.equals("[]")
                    && totalUrlsInitialCounts.equals("[]")
                    && totalUrlsRefreshCounts.equals("[]")
                    && activityReferrals.equals("[]");
        }

        UmaUploader() {
        }

        @Override
        public void run() {
            uploadActions(urlSelectedCount, URL_SELECTED_COUNT);
            uploadActions(optInDeclineButtonTapCount, OPT_IN_DECLINE_BUTTON_PRESS_COUNT);
            uploadActions(optInEnableButtonTapCount, OPT_IN_ENABLE_BUTTON_PRESS_COUNT);
            uploadActions(optInHighPriorityNotificationCount,
                    OPT_IN_HIGH_PRIORITY_NOTIFICATION_COUNT);
            uploadActions(optInMinPriorityNotificationCount,
                    OPT_IN_MIN_PRIORITY_NOTIFICATION_COUNT);
            uploadActions(optInNotificationPressCount, OPT_IN_NOTIFICATION_PRESS_COUNT);
            uploadActions(prefsFeatureDisabledCount, PREFS_FEATURE_DISABLED_COUNT);
            uploadActions(prefsFeatureEnabledCount, PREFS_FEATURE_ENABLED_COUNT);
            uploadActions(prefsLocationDeniedCount, PREFS_LOCATION_DENIED_COUNT);
            uploadActions(prefsLocationGrantedCount, PREFS_LOCATION_GRANTED_COUNT);
            uploadTimes(pwsBackgroundResolveTimes, PWS_BACKGROUND_RESOLVE_TIMES,
                    TimeUnit.MILLISECONDS);
            uploadTimes(pwsForegroundResolveTimes, PWS_FOREGROUND_RESOLVE_TIMES,
                    TimeUnit.MILLISECONDS);
            uploadTimes(standardNotificationPressDelays, STANDARD_NOTIFICATION_PRESS_DELAYS,
                    TimeUnit.MILLISECONDS);
            uploadTimes(optInNotificationPressDelays, OPT_IN_NOTIFICATION_PRESS_DELAYS,
                    TimeUnit.MILLISECONDS);
            uploadCounts(totalUrlsInitialCounts, TOTAL_URLS_INITIAL_COUNTS);
            uploadCounts(totalUrlsRefreshCounts, TOTAL_URLS_REFRESH_COUNTS);
            uploadEnums(activityReferrals, ACTIVITY_REFERRALS, ListUrlsActivity.REFERER_BOUNDARY);
        }

        private static void uploadActions(int count, String key) {
            for (int i = 0; i < count; i++) {
                RecordUserAction.record(key);
            }
        }

        private static Number[] parseJsonNumberArray(String jsonArrayStr) {
            try {
                JSONArray values = new JSONArray(jsonArrayStr);
                Number[] array = new Number[values.length()];
                for (int i = 0; i < values.length(); i++) {
                    Object object = values.get(i);
                    if (!(object instanceof Number)) {
                        return null;
                    }
                    array[i] = (Number) object;
                }
                return array;
            } catch (JSONException e) {
                return null;
            }
        }

        private static Long[] parseJsonLongArray(String jsonArrayStr) {
            Number[] numbers = parseJsonNumberArray(jsonArrayStr);
            if (numbers == null) {
                return null;
            }
            Long[] array = new Long[numbers.length];
            for (int i = 0; i < numbers.length; i++) {
                array[i] = numbers[i].longValue();
            }
            return array;
        }

        private static Integer[] parseJsonIntegerArray(String jsonArrayStr) {
            Number[] numbers = parseJsonNumberArray(jsonArrayStr);
            if (numbers == null) {
                return null;
            }
            Integer[] array = new Integer[numbers.length];
            for (int i = 0; i < numbers.length; i++) {
                array[i] = numbers[i].intValue();
            }
            return array;
        }

        private static void uploadTimes(String jsonTimesStr, final String key, final TimeUnit tu) {
            Long[] times = parseJsonLongArray(jsonTimesStr);
            if (times == null) {
                Log.e(TAG, "Error reporting " + key + " with values: " + jsonTimesStr);
                return;
            }
            for (Long time : times) {
                RecordHistogram.recordTimesHistogram(key, time, TimeUnit.MILLISECONDS);
            }
        }

        private static void uploadCounts(String jsonCountsStr, final String key) {
            Integer[] counts = parseJsonIntegerArray(jsonCountsStr);
            if (counts == null) {
                Log.e(TAG, "Error reporting " + key + " with values: " + jsonCountsStr);
                return;
            }
            for (Integer count: counts) {
                RecordHistogram.recordCountHistogram(key, count);
            }
        }

        private static void uploadEnums(String jsonEnumsStr, final String key, int boundary) {
            Integer[] values = parseJsonIntegerArray(jsonEnumsStr);
            if (values == null) {
                Log.e(TAG, "Error reporting " + key + " with values: " + jsonEnumsStr);
                return;
            }
            for (Integer value: values) {
                RecordHistogram.recordEnumeratedHistogram(key, value, boundary);
            }
        }
    }
}
