// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.provider.Browser;
import android.support.v4.app.NotificationCompat;
import android.widget.RemoteViews;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;

/**
 * A class for notifications that provide information and optional media controls for a given media.
 * Internally implements a Service for transforming notification Intents into
 * {@link MediaPlaybackListener} calls for all registered listeners.
 */
public class NotificationMediaPlaybackControls {
    private static final Object LOCK = new Object();
    private static NotificationMediaPlaybackControls sInstance;

    /**
     * Service used to transform intent requests triggered from the notification into
     * {@code Listener} callbacks. Ideally this class should be private, but public is required to
     * create as a service.
     */
    public static class ListenerService extends Service {
        private static final String ACTION_PLAY =
                "NotificationMediaPlaybackControls.ListenerService.PLAY";
        private static final String ACTION_PAUSE =
                "NotificationMediaPlaybackControls.ListenerService.PAUSE";

        private PendingIntent getPendingIntent(String action) {
            Intent intent = new Intent(this, ListenerService.class);
            intent.setAction(action);
            return PendingIntent.getService(this, 0, intent, PendingIntent.FLAG_CANCEL_CURRENT);
        }

        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }

        @Override
        public void onCreate() {
            super.onCreate();
            onServiceStarted(this);
        }

        @Override
        public void onDestroy() {
            super.onDestroy();
            onServiceDestroyed();
        }

        @Override
        public int onStartCommand(Intent intent, int flags, int startId) {
            if (intent == null
                    || sInstance == null
                    || sInstance.mMediaInfo == null
                    || sInstance.mMediaInfo.listener == null) {
                stopSelf();
                return START_NOT_STICKY;
            }

            String action = intent.getAction();
            if (ACTION_PLAY.equals(action)) {
                sInstance.mMediaInfo.listener.onPlay();
                sInstance.onPlaybackStateChanged(false);
            } else if (ACTION_PAUSE.equals(action)) {
                sInstance.mMediaInfo.listener.onPause();
                sInstance.onPlaybackStateChanged(true);
            }

            return START_NOT_STICKY;
        }
    }

    /**
     * Shows the notification with media controls with the specified media info. Replaces/updates
     * the current notification if already showing. Does nothing if |mediaInfo| hasn't changed from
     * the last one.
     *
     * @param applicationContext context to create the notification with
     * @param mediaInfo information to show in the notification
     */
    public static void show(Context applicationContext, MediaInfo mediaInfo) {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new NotificationMediaPlaybackControls(applicationContext);
            }
        }
        sInstance.showNotification(mediaInfo);
    }

    /**
     * Hides the notification for the specified tabId.
     *
     * @param tabId the id of the tab that showed the notification or invalid tab id.
     */
    public static void hide(int tabId) {
        if (sInstance == null) return;
        sInstance.hideNotification(tabId);
    }

    /**
     * Hides any notification if shown by this service.
     */
    public static void clear() {
        if (sInstance == null) return;
        sInstance.clearNotification();
    }

    /**
     * Registers the started {@link Service} with the singleton and creates the notification.
     *
     * @param service the service that was started
     */
    private static void onServiceStarted(ListenerService service) {
        assert sInstance != null;
        assert sInstance.mService == null;
        sInstance.mService = service;
        sInstance.updateNotification();
    }

    /**
     * Handles the destruction
     */
    private static void onServiceDestroyed() {
        assert sInstance != null;
        assert sInstance.mService != null;
        sInstance.mNotification = null;
        sInstance.mService = null;
    }

    private final Context mContext;

    // ListenerService running for the notification. Only non-null when showing.
    private ListenerService mService;

    private final String mPlayDescription;

    private final String mPauseDescription;

    private Notification mNotification;

    private MediaInfo mMediaInfo;

    private NotificationMediaPlaybackControls(Context context) {
        mContext = context;
        mPlayDescription = context.getResources().getString(R.string.accessibility_play);
        mPauseDescription = context.getResources().getString(R.string.accessibility_pause);
    }

    private void showNotification(MediaInfo mediaInfo) {
        mContext.startService(new Intent(mContext, ListenerService.class));

        assert mediaInfo != null;

        if (mediaInfo.equals(mMediaInfo)) return;

        mMediaInfo = mediaInfo;
        updateNotification();
    }

    private void clearNotification() {
        mMediaInfo = null;
        mContext.stopService(new Intent(mContext, ListenerService.class));
    }

    private void hideNotification(int tabId) {
        if (mMediaInfo == null || tabId != mMediaInfo.tabId) return;
        clearNotification();
    }

    private void onPlaybackStateChanged(boolean isPaused) {
        assert mMediaInfo != null;
        mMediaInfo = new MediaInfo(
                mMediaInfo.title,
                isPaused,
                mMediaInfo.origin,
                mMediaInfo.tabId,
                mMediaInfo.listener);
        updateNotification();
    }

    private RemoteViews createContentView() {
        RemoteViews contentView =
                new RemoteViews(mContext.getPackageName(), R.layout.playback_notification_bar);
        return contentView;
    }

    private String getStatus() {
        if (mMediaInfo.origin != null) {
            return mContext.getString(R.string.media_notification_link_text, mMediaInfo.origin);
        }
        return mContext.getString(R.string.media_notification_text_no_link);
    }

    private String getTitle() {
        String mediaTitle = mMediaInfo.title;
        if (mMediaInfo.isPaused) {
            return mContext.getString(
                    R.string.media_playback_notification_paused_for_media, mediaTitle);
        }
        return mContext.getString(
                R.string.media_playback_notification_playing_for_media, mediaTitle);
    }

    private PendingIntent createContentIntent() {
        int tabId = mMediaInfo.tabId;
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        intent.putExtra(TabOpenType.BRING_TAB_TO_FRONT.name(), tabId);
        intent.setPackage(mContext.getPackageName());
        return PendingIntent.getActivity(mContext, tabId, intent, 0);
    }

    private void updateNotification() {
        if (mService == null) return;

        if (mMediaInfo == null) {
            // Notification was hidden before we could update it.
            assert mNotification == null;
            return;
        }

        if (mNotification == null) {
            NotificationCompat.Builder notificationBuilder =
                    new NotificationCompat.Builder(mContext)
                            .setSmallIcon(R.drawable.audio_playing)
                            .setAutoCancel(false)
                            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                            .setOngoing(true)
                            .setContent(createContentView())
                            .setContentIntent(createContentIntent());
            mNotification = notificationBuilder.build();
        }

        RemoteViews contentView = createContentView();

        contentView.setTextViewText(R.id.title, getTitle());
        contentView.setTextViewText(R.id.status, getStatus());
        contentView.setImageViewResource(R.id.icon, R.drawable.audio_playing);

        if (mMediaInfo.isPaused) {
            contentView.setImageViewResource(R.id.playpause, R.drawable.ic_vidcontrol_play);
            contentView.setContentDescription(R.id.playpause, mPlayDescription);
            contentView.setOnClickPendingIntent(R.id.playpause,
                    mService.getPendingIntent(ListenerService.ACTION_PLAY));
        } else {
            contentView.setImageViewResource(R.id.playpause, R.drawable.ic_vidcontrol_pause);
            contentView.setContentDescription(R.id.playpause, mPauseDescription);
            contentView.setOnClickPendingIntent(R.id.playpause,
                    mService.getPendingIntent(ListenerService.ACTION_PAUSE));
        }

        mNotification.contentView = contentView;

        mService.startForeground(R.id.media_playback_notification, mNotification);
    }
}
