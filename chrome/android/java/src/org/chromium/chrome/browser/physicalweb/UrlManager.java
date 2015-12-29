// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.SystemClock;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;

import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * This class stores URLs which are discovered by scanning for Physical Web beacons, and updates a
 * Notification as the set changes.
 *
 * There are two sets of URLs maintained:
 * - Those which are currently nearby, as tracked by calls to addUrl/removeUrl
 * - Those which have ever resolved through the Physical Web Service (e.g. are known to produce
 *     good results).
 *
 * Whenever either list changes, we update the Physical Web Notification, based on the intersection
 * of currently-nearby and known-resolved URLs.
 */
class UrlManager {
    private static final String TAG = "PhysicalWeb";
    private static final String PREFS_NAME = "org.chromium.chrome.browser.physicalweb.URL_CACHE";
    private static final String PREFS_VERSION_KEY = "version";
    private static final String PREFS_NEARBY_URLS_KEY = "nearby_urls";
    private static final String PREFS_RESOLVED_URLS_KEY = "resolved_urls";
    private static final String DEPRECATED_PREFS_URLS_KEY = "urls";
    private static final int PREFS_VERSION = 2;
    private static UrlManager sInstance = null;
    private final Context mContext;
    private final NotificationManagerCompat mNotificationManager;
    private final PwsClient mPwsClient;

    /**
     * Construct the UrlManager.
     * @param context An instance of android.content.Context
     */
    public UrlManager(Context context) {
        mContext = context;
        mNotificationManager = NotificationManagerCompat.from(context);
        mPwsClient = new PwsClient();
        initSharedPreferences();
    }

    /**
     * Get a singleton instance of this class.
     * @param context An instance of android.content.Context.
     * @return A singleton instance of this class.
     */
    public static UrlManager getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new UrlManager(context);
        }
        return sInstance;
    }

    /**
     * Add a URL to the store of URLs.
     * This method additionally updates the Physical Web notification.
     * @param url The URL to add.
     */
    public void addUrl(String url) {
        Log.d(TAG, "URL found: " + url);
        boolean isOnboarding = PhysicalWeb.isOnboarding(mContext);
        Set<String> nearbyUrls = getCachedNearbyUrls();

        boolean wasUrlListEmpty = (isOnboarding && nearbyUrls.isEmpty())
                || (!isOnboarding && getUrls().isEmpty());

        nearbyUrls.add(url);
        putCachedNearbyUrls(nearbyUrls);

        if (!isOnboarding) {
            resolveUrl(url);
        }

        boolean isUrlListEmpty = !isOnboarding && getUrls().isEmpty();

        if (wasUrlListEmpty && !isUrlListEmpty) {
            showNotification();
        }
    }

    /**
     * Remove a URL to the store of URLs.
     * This method additionally updates the Physical Web notification.
     * @param url The URL to remove.
     */
    public void removeUrl(String url) {
        Log.d(TAG, "URL lost: " + url);
        boolean isOnboarding = PhysicalWeb.isOnboarding(mContext);
        Set<String> nearbyUrls = getCachedNearbyUrls();
        nearbyUrls.remove(url);
        putCachedNearbyUrls(nearbyUrls);

        boolean isUrlListEmpty = (isOnboarding && nearbyUrls.isEmpty())
                || (!isOnboarding && getUrls().isEmpty());

        if (isUrlListEmpty) {
            clearNotification();
        }
    }

    /**
     * Get the list of URLs which are both nearby and resolved through PWS.
     * @return A set of nearby and resolved URLs.
     */
    public Set<String> getUrls() {
        return getUrls(false);
    }

    /**
     * Get the list of URLs which are both nearby and resolved through PWS.
     * @param allowUnresolved If true, include unresolved URLs only if the
     * resolved URL list is empty.
     * @return A set of nearby URLs.
     */
    public Set<String> getUrls(boolean allowUnresolved) {
        Set<String> nearbyUrls = getCachedNearbyUrls();
        Set<String> resolvedUrls = getCachedResolvedUrls();
        Set<String> intersection = new HashSet<String>(nearbyUrls);
        intersection.retainAll(resolvedUrls);
        Log.d(TAG, "Get URLs With: " + nearbyUrls.size() + " nearby, "
                      + resolvedUrls.size() + " resolved, and "
                      + intersection.size() + " in intersection.");

        if (allowUnresolved && resolvedUrls.isEmpty()) {
            intersection = nearbyUrls;
        }

        return intersection;
    }

    /**
     * Forget all stored URLs and clear the notification.
     */
    public void clearUrls() {
        Set<String> emptySet = Collections.emptySet();
        putCachedNearbyUrls(emptySet);
        putCachedResolvedUrls(emptySet);
        clearNotification();
    }

    private void addResolvedUrl(String url) {
        Log.d(TAG, "PWS resolved: " + url);
        Set<String> resolvedUrls = getCachedResolvedUrls();

        boolean wasUrlListEmpty = getUrls().isEmpty();

        resolvedUrls.add(url);
        putCachedResolvedUrls(resolvedUrls);

        boolean isUrlListEmpty = getUrls().isEmpty();

        if (wasUrlListEmpty && !isUrlListEmpty) {
            showNotification();
        }
    }

    private void removeResolvedUrl(String url) {
        Log.d(TAG, "PWS unresolved: " + url);
        Set<String> resolvedUrls = getCachedResolvedUrls();
        resolvedUrls.remove(url);
        putCachedResolvedUrls(resolvedUrls);

        if (getUrls().isEmpty()) {
            clearNotification();
        }
    }

    private void initSharedPreferences() {
        SharedPreferences prefs = mContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        int prefsVersion = prefs.getInt(PREFS_VERSION_KEY, 0);

        // Check the version.
        if (prefsVersion == PREFS_VERSION) {
            return;
        }

        // Stored preferences are old, upgrade to the current version.
        SharedPreferences.Editor editor = prefs.edit();
        editor.remove(DEPRECATED_PREFS_URLS_KEY);
        editor.putInt(PREFS_VERSION_KEY, PREFS_VERSION);
        editor.apply();

        clearUrls();
    }

    private Set<String> getStringSetFromSharedPreferences(String preferenceName) {
        // Check the version.
        SharedPreferences prefs = mContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        return prefs.getStringSet(preferenceName, new HashSet<String>());
    }

    private void setStringSetInSharedPreferences(String preferenceName,
                                                 Set<String> preferenceValue) {
        // Write the version.
        SharedPreferences prefs = mContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(preferenceName, preferenceValue);
        editor.apply();
    }

    private Set<String> getCachedNearbyUrls() {
        return getStringSetFromSharedPreferences(PREFS_NEARBY_URLS_KEY);
    }

    private void putCachedNearbyUrls(Set<String> urls) {
        setStringSetInSharedPreferences(PREFS_NEARBY_URLS_KEY, urls);
    }

    private Set<String> getCachedResolvedUrls() {
        return getStringSetFromSharedPreferences(PREFS_RESOLVED_URLS_KEY);
    }

    private void putCachedResolvedUrls(Set<String> urls) {
        setStringSetInSharedPreferences(PREFS_RESOLVED_URLS_KEY, urls);
    }

    private PendingIntent createListUrlsIntent() {
        Intent intent = new Intent(mContext, ListUrlsActivity.class);
        intent.putExtra(ListUrlsActivity.REFERER_KEY, ListUrlsActivity.NOTIFICATION_REFERER);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);
        return pendingIntent;
    }

    private PendingIntent createOptInIntent() {
        Intent intent = new Intent(mContext, PhysicalWebOptInActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);
        return pendingIntent;
    }

    private void resolveUrl(final String url) {
        Set<String> urls = new HashSet<String>(Arrays.asList(url));
        final long timestamp = SystemClock.elapsedRealtime();
        mPwsClient.resolve(urls, new PwsClient.ResolveScanCallback() {
            @Override
            public void onPwsResults(Collection<PwsResult> pwsResults) {
                long duration = SystemClock.elapsedRealtime() - timestamp;
                PhysicalWebUma.onBackgroundPwsResolution(mContext, duration);

                for (PwsResult pwsResult : pwsResults) {
                    String requestUrl = pwsResult.requestUrl;
                    if (url.equalsIgnoreCase(requestUrl)) {
                        addResolvedUrl(url);
                        return;
                    }
                }
                removeResolvedUrl(url);
            }
        });
    }

    private void showNotification() {
        if (PhysicalWeb.isOnboarding(mContext)) {
            if (PhysicalWeb.getOptInNotifyCount(mContext) < PhysicalWeb.OPTIN_NOTIFY_MAX_TRIES) {
                // high priority notification
                createOptInNotification(true);
                PhysicalWeb.recordOptInNotification(mContext);
            } else {
                // min priority notification
                createOptInNotification(false);
            }
            return;
        }

        if (PhysicalWeb.isPhysicalWebPreferenceEnabled(mContext)) {
            createNotification();
        }
    }

    private void createNotification() {
        PendingIntent pendingIntent = createListUrlsIntent();

        // Get values to display.
        Resources resources = mContext.getResources();
        String title = resources.getString(R.string.physical_web_notification_title);
        Bitmap largeIcon = BitmapFactory.decodeResource(resources,
                R.drawable.physical_web_notification_large);

        // Create the notification.
        Notification notification = new NotificationCompat.Builder(mContext)
                .setLargeIcon(largeIcon)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentTitle(title)
                .setContentIntent(pendingIntent)
                .setPriority(NotificationCompat.PRIORITY_MIN)
                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                .build();
        mNotificationManager.notify(NotificationConstants.NOTIFICATION_ID_PHYSICAL_WEB,
                                    notification);
    }

    private void createOptInNotification(boolean highPriority) {
        PendingIntent pendingIntent = createOptInIntent();

        int priority = highPriority ? NotificationCompat.PRIORITY_HIGH
                : NotificationCompat.PRIORITY_MIN;

        // Get values to display.
        Resources resources = mContext.getResources();
        String title = resources.getString(R.string.physical_web_optin_notification_title);
        Bitmap largeIcon = BitmapFactory.decodeResource(resources,
                R.drawable.physical_web_notification_large);

        // Create the notification.
        Notification notification = new NotificationCompat.Builder(mContext)
                .setLargeIcon(largeIcon)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentTitle(title)
                .setContentIntent(pendingIntent)
                .setPriority(priority)
                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                .setAutoCancel(true)
                .build();
        mNotificationManager.notify(NotificationConstants.NOTIFICATION_ID_PHYSICAL_WEB,
                                    notification);
    }

    private void clearNotification() {
        mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_PHYSICAL_WEB);
    }
}
