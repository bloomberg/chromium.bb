// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.Nullable;

import org.chromium.base.Log;

/**
 * A background service that clears browsing data.
 *
 * Lifecycle: This is a Service, so it will be created by the Android Framework.
 * Thread safety: As an {@link IntentService}, {@link #onHandleIntent} is called on a background
 * thread.
 */
public class ClearDataService extends IntentService {
    private static final String TAG = "ClearDataService";

    /* package */ static final String ACTION_CLEAR_DATA =
            "org.chromium.chrome.browser.browserservices.ClearDataService.CLEAR_DATA";
    /* package */ static final String ACTION_DISMISS =
            "org.chromium.chrome.browser.browserservices.ClearDataService.DISMISS";

    /* package */ static final String EXTRA_NOTIFICATION_ID =
            "org.chromium.chrome.browser.browserservices.ClearDataService.NOTIFICATION_ID";
    /* package */ static final String EXTRA_URL =
            "org.chromium.chrome.browser.browserservices.ClearDataService.URL";

    private final ClearDataNotificationPublisher mNotificationManager;

    /** Constructor required by Android with default dependencies. */
    public ClearDataService() {
        this(new ClearDataNotificationPublisher());
    }

    /** Constructor with dependency injection for testing. */
    public ClearDataService(ClearDataNotificationPublisher notificationManager) {
        super(TAG);
        mNotificationManager = notificationManager;
    }

    @Override
    protected void onHandleIntent(@Nullable Intent intent) {
        if (intent == null) return;
        // ClearDataService is not exported, so as long as we don't let PendingIntents pointing to
        // this class leak to other Android applications, we can trust that this code can only be
        // called from Chrome (or a Notification that Chrome created).

        if (!intent.hasExtra(EXTRA_NOTIFICATION_ID)) {
            Log.w(TAG, "Got Intent without Notification Id");
            return;
        }
        int notificationId = intent.getIntExtra(EXTRA_NOTIFICATION_ID, 0);

        if (ACTION_CLEAR_DATA.equals(intent.getAction())) {
            String url = intent.getStringExtra(EXTRA_URL);
            if (url == null) {
                Log.w(TAG, "Got Clear Data Intent without URL.");
                return;
            }
            // TODO(peconn): Clear the data!
            Log.d(TAG, "Pretending to clear data for %s.", url);
            mNotificationManager.dismissClearDataNotification(this, notificationId);
        } else if (ACTION_DISMISS.equals(intent.getAction())) {
            mNotificationManager.dismissClearDataNotification(this, notificationId);
        }
    }

    /**
     * Creates a PendingIntent to clear data for the given |url| and cancel the notification with
     * the given |id|.
     */
    public static PendingIntent getClearDataIntent(Context context, String url, int id) {
        Intent intent = new Intent(context, ClearDataService.class);
        intent.setAction(ACTION_CLEAR_DATA);
        intent.putExtra(EXTRA_NOTIFICATION_ID, id);
        // TODO(peconn): Consider putting this in data instead.
        intent.putExtra(EXTRA_URL, url);
        // See similar code in {@link getDismissIntent}.
        return PendingIntent.getService(context, id, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /**
     * Creates a PendingIntent to dismiss the Notification with the given |id|.
     */
    public static PendingIntent getDismissIntent(Context context, int id) {
        Intent intent = new Intent(context, ClearDataService.class);
        intent.setAction(ACTION_DISMISS);
        // Store the notification ID in the intent itself so we can retrieve it later.
        intent.putExtra(EXTRA_NOTIFICATION_ID, id);
        // Also use the notification ID as the request code so we can have multiple pending intents
        // existing at the same time for different applications/urls.
        return PendingIntent.getService(context, id, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }
}
