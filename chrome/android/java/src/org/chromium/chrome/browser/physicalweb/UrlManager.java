// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;

import java.util.ArrayList;
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
    private static final long STALE_NOTIFICATION_TIMEOUT_MILLIS = 30 * 60 * 1000;
    private static UrlManager sInstance = null;
    private final Context mContext;
    private NotificationManagerProxy mNotificationManager;
    private PwsClient mPwsClient;
    private final ObserverList<Listener> mObservers;

    /**
     * Interface for observers that should be notified when the nearby URL list changes.
     */
    public interface Listener {
        /**
         * Callback called when one or more URLs are added to the URL list.
         * @param urls A set of strings containing nearby URLs resolvable with our resolution
         * service.
         */
        void onDisplayableUrlsAdded(Collection<String> urls);
    }

    /**
     * Construct the UrlManager.
     * @param context An instance of android.content.Context
     */
    public UrlManager(Context context) {
        mContext = context;
        mNotificationManager = new NotificationManagerProxyImpl(
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE));
        mPwsClient = new PwsClientImpl();
        mObservers = new ObserverList<Listener>();
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
     * Add an observer to be notified on changes to the nearby URL list.
     * @param observer The observer to add.
     */
    public void addObserver(Listener observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer from the observer list.
     * @param observer The observer to remove.
     */
    public void removeObserver(Listener observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Add a URL to the store of URLs.
     * This method additionally updates the Physical Web notification.
     * @param url The URL to add.
     */
    @VisibleForTesting
    public void addUrl(String url) {
        Log.d(TAG, "URL found: %s", url);
        boolean isOnboarding = PhysicalWeb.isOnboarding(mContext);
        Set<String> nearbyUrls = getCachedNearbyUrls();

        // A URL is displayable if it is both nearby and resolved through our resolution service.
        // When new displayable URLs are found we tell our observers. In onboarding mode we do not
        // use our resolution service so the displayable list should always be empty. However, we
        // still want to track the nearby URL count so we can show an opt-in notification.
        // In normal operation, both the notification and observers are updated for changes to the
        // displayable list.

        int displayableUrlsBefore;
        int notificationUrlsBefore;
        if (isOnboarding) {
            displayableUrlsBefore = 0;
            notificationUrlsBefore = nearbyUrls.size();
        } else {
            displayableUrlsBefore = notificationUrlsBefore = getUrls().size();
        }

        nearbyUrls.add(url);
        putCachedNearbyUrls(nearbyUrls);

        if (!isOnboarding) {
            resolveUrl(url);
        }

        int displayableUrlsAfter;
        int notificationUrlsAfter;
        if (isOnboarding) {
            displayableUrlsAfter = 0;
            notificationUrlsAfter = nearbyUrls.size();
        } else {
            displayableUrlsAfter = notificationUrlsAfter = getUrls().size();
        }

        updateNotification(notificationUrlsBefore == 0, notificationUrlsAfter == 0);
        notifyDisplayableUrlsChanged(displayableUrlsBefore, displayableUrlsAfter, url);
    }

    /**
     * Remove a URL to the store of URLs.
     * This method additionally updates the Physical Web notification.
     * @param url The URL to remove.
     */
    @VisibleForTesting
    public void removeUrl(String url) {
        Log.d(TAG, "URL lost: %s", url);
        boolean isOnboarding = PhysicalWeb.isOnboarding(mContext);
        Set<String> nearbyUrls = getCachedNearbyUrls();
        nearbyUrls.remove(url);
        putCachedNearbyUrls(nearbyUrls);

        int notificationUrlsAfter = isOnboarding ? nearbyUrls.size() : getUrls().size();
        updateNotification(false, notificationUrlsAfter == 0);
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
        Log.d(TAG, "Get URLs With: %d nearby, %d resolved, and %d in intersection.",
                nearbyUrls.size(), resolvedUrls.size(), intersection.size());

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
        cancelClearNotificationAlarm();
    }

    /**
     * Clear the URLManager's notification.
     * Typically, this should not be called except when we want to clear the notification without
     * modifying the list of URLs, as is the case when we want to remove stale notifications.
     */
    public void clearNotification() {
        mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_PHYSICAL_WEB);
    }

    private void addResolvedUrl(String url) {
        Log.d(TAG, "PWS resolved: %s", url);
        Set<String> resolvedUrls = getCachedResolvedUrls();
        int displayableUrlsBefore = getUrls().size();

        resolvedUrls.add(url);
        putCachedResolvedUrls(resolvedUrls);

        int displayableUrlsAfter = getUrls().size();
        updateNotification(displayableUrlsBefore == 0, displayableUrlsAfter == 0);
        notifyDisplayableUrlsChanged(displayableUrlsBefore, displayableUrlsAfter, url);
    }

    private void removeResolvedUrl(String url) {
        Log.d(TAG, "PWS unresolved: %s", url);
        Set<String> resolvedUrls = getCachedResolvedUrls();
        resolvedUrls.remove(url);
        putCachedResolvedUrls(resolvedUrls);
        updateNotification(false, getUrls().isEmpty());
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
            public void onPwsResults(final Collection<PwsResult> pwsResults) {
                long duration = SystemClock.elapsedRealtime() - timestamp;
                PhysicalWebUma.onBackgroundPwsResolution(mContext, duration);
                new Handler(Looper.getMainLooper()).post(new Runnable() {
                    @Override
                    public void run() {
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
        });
    }

    private void updateNotification(boolean isUrlListEmptyBefore, boolean isUrlListEmptyAfter) {
        if (isUrlListEmptyAfter) {
            clearNotification();
            cancelClearNotificationAlarm();
            return;
        }

        // We only call showNotification if the list was empty before because we need to be able to
        // count the number of times we show the OptIn notification.
        if (isUrlListEmptyBefore) {
            showNotification();
        }
        scheduleClearNotificationAlarm();
    }

    private void showNotification() {
        if (PhysicalWeb.isOnboarding(mContext)) {
            if (PhysicalWeb.getOptInNotifyCount(mContext) < PhysicalWeb.OPTIN_NOTIFY_MAX_TRIES) {
                // high priority notification
                createOptInNotification(true);
                PhysicalWeb.recordOptInNotification(mContext);
                PhysicalWebUma.onOptInHighPriorityNotificationShown(mContext);
            } else {
                // min priority notification
                createOptInNotification(false);
                PhysicalWebUma.onOptInMinPriorityNotificationShown(mContext);
            }
        } else if (PhysicalWeb.isPhysicalWebPreferenceEnabled(mContext)) {
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
        String text = resources.getString(R.string.physical_web_optin_notification_text);
        Bitmap largeIcon = BitmapFactory.decodeResource(resources, R.mipmap.app_icon);

        // Create the notification.
        Notification notification = new NotificationCompat.Builder(mContext)
                .setLargeIcon(largeIcon)
                .setSmallIcon(R.drawable.ic_physical_web_notification)
                .setContentTitle(title)
                .setContentText(text)
                .setContentIntent(pendingIntent)
                .setPriority(priority)
                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                .setAutoCancel(true)
                .build();
        mNotificationManager.notify(NotificationConstants.NOTIFICATION_ID_PHYSICAL_WEB,
                                    notification);
    }

    private PendingIntent createClearNotificationAlarmIntent() {
        Intent intent = new Intent(mContext, ClearNotificationAlarmReceiver.class);
        return PendingIntent.getBroadcast(mContext, 0, intent, PendingIntent.FLAG_CANCEL_CURRENT);
    }

    private void scheduleClearNotificationAlarm() {
        PendingIntent pendingIntent = createClearNotificationAlarmIntent();
        AlarmManager alarmManager = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
        long time = SystemClock.elapsedRealtime() + STALE_NOTIFICATION_TIMEOUT_MILLIS;
        alarmManager.set(AlarmManager.ELAPSED_REALTIME, time, pendingIntent);
    }

    private void cancelClearNotificationAlarm() {
        PendingIntent pendingIntent = createClearNotificationAlarmIntent();
        AlarmManager alarmManager = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
        alarmManager.cancel(pendingIntent);
    }

    private void notifyDisplayableUrlsChanged(int displayCountBefore, int displayCountAfter,
            String url) {
        if (displayCountAfter > displayCountBefore) {
            Collection<String> urls = new ArrayList<String>();
            urls.add(url);
            Collection<String> wrappedUrls = Collections.unmodifiableCollection(urls);
            for (Listener observer : mObservers) {
                observer.onDisplayableUrlsAdded(wrappedUrls);
            }
        }
    }

    @VisibleForTesting
    void overridePwsClientForTesting(PwsClient pwsClient) {
        mPwsClient = pwsClient;
    }

    @VisibleForTesting
    void overrideNotificationManagerForTesting(
            NotificationManagerProxy notificationManager) {
        mNotificationManager = notificationManager;
    }
}
