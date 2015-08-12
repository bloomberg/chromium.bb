// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.IBinder;
import android.provider.Browser;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.widget.RemoteViews;

import org.chromium.base.ApiCompatibilityUtils;
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
        private static final String ACTION_STOP_SELF =
                "NotificationMediaPlaybackControls.ListenerService.STOP_SELF";

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

            // This would only happen if we have been recreated by the OS after Chrome has died.
            // In this case, there can be no media playback happening so we don't have to show
            // the notification.
            if (sInstance == null) return;

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
                    || sInstance.mMediaNotificationInfo == null
                    || sInstance.mMediaNotificationInfo.listener == null) {
                stopSelf();
                return START_NOT_STICKY;
            }

            String action = intent.getAction();

            if (ACTION_STOP_SELF.equals(action)) {
                stopSelf();
                return START_NOT_STICKY;
            }

            if (ACTION_PLAY.equals(action)) {
                sInstance.mMediaNotificationInfo.listener.onPlay();
                sInstance.onPlaybackStateChanged(false);
            } else if (ACTION_PAUSE.equals(action)) {
                sInstance.mMediaNotificationInfo.listener.onPause();
                sInstance.onPlaybackStateChanged(true);
            }

            return START_NOT_STICKY;
        }
    }

    /**
     * Shows the notification with media controls with the specified media info. Replaces/updates
     * the current notification if already showing. Does nothing if |mediaNotificationInfo| hasn't
     * changed from the last one.
     *
     * @param applicationContext context to create the notification with
     * @param mediaNotificationInfo information to show in the notification
     */
    public static void show(Context applicationContext,
                            MediaNotificationInfo mediaNotificationInfo) {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new NotificationMediaPlaybackControls(applicationContext);
            }
        }
        sInstance.showNotification(mediaNotificationInfo);
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
        sInstance.mNotificationBuilder = null;
        sInstance.mService = null;
    }

    private final Context mContext;

    // ListenerService running for the notification. Only non-null when showing.
    private ListenerService mService;

    private final String mPlayDescription;

    private final String mPauseDescription;

    private NotificationCompat.Builder mNotificationBuilder;

    private Bitmap mNotificationIconBitmap;

    private MediaNotificationInfo mMediaNotificationInfo;

    private NotificationMediaPlaybackControls(Context context) {
        mContext = context;
        mPlayDescription = context.getResources().getString(R.string.accessibility_play);
        mPauseDescription = context.getResources().getString(R.string.accessibility_pause);
    }

    private void showNotification(MediaNotificationInfo mediaNotificationInfo) {
        mContext.startService(new Intent(mContext, ListenerService.class));

        assert mediaNotificationInfo != null;

        if (mediaNotificationInfo.equals(mMediaNotificationInfo)) return;

        mMediaNotificationInfo = new MediaNotificationInfo(
                sanitizeMediaTitle(mediaNotificationInfo.title),
                mediaNotificationInfo.isPaused,
                mediaNotificationInfo.origin,
                mediaNotificationInfo.tabId,
                mediaNotificationInfo.isPrivate,
                mediaNotificationInfo.listener);
        updateNotification();
    }

    private void clearNotification() {
        NotificationManagerCompat manager = NotificationManagerCompat.from(mContext);
        manager.cancel(R.id.media_playback_notification);

        mMediaNotificationInfo = null;
        mContext.stopService(new Intent(mContext, ListenerService.class));
    }

    private void hideNotification(int tabId) {
        if (mMediaNotificationInfo == null || tabId != mMediaNotificationInfo.tabId) return;
        clearNotification();
    }

    private void onPlaybackStateChanged(boolean isPaused) {
        assert mMediaNotificationInfo != null;
        mMediaNotificationInfo = new MediaNotificationInfo(
                mMediaNotificationInfo.title,
                isPaused,
                mMediaNotificationInfo.origin,
                mMediaNotificationInfo.tabId,
                mMediaNotificationInfo.isPrivate,
                mMediaNotificationInfo.listener);
        updateNotification();
    }

    private RemoteViews createContentView() {
        RemoteViews contentView =
                new RemoteViews(mContext.getPackageName(), R.layout.playback_notification_bar);
        return contentView;
    }

    private String sanitizeMediaTitle(String title) {
        // Improve the visibility of the title by removing all the leading/trailing white spaces
        // and the quite common unicode play icon.
        title = title.trim();
        return title.startsWith("\u25B6") ? title.substring(1).trim() : title;
    }

    private String getStatus() {
        if (mMediaNotificationInfo.origin != null) {
            return mContext.getString(R.string.media_notification_link_text,
                                      mMediaNotificationInfo.origin);
        }
        return mContext.getString(R.string.media_notification_text_no_link);
    }

    private PendingIntent createContentIntent() {
        int tabId = mMediaNotificationInfo.tabId;
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        intent.putExtra(TabOpenType.BRING_TAB_TO_FRONT.name(), tabId);
        intent.setPackage(mContext.getPackageName());
        return PendingIntent.getActivity(mContext, tabId, intent, 0);
    }

    private void updateNotification() {
        if (mService == null) return;

        if (mMediaNotificationInfo == null) {
            // Notification was hidden before we could update it.
            assert mNotificationBuilder == null;
            return;
        }

        // Android doesn't badge the icons for RemoteViews automatically when
        // running the app under the Work profile.
        if (mNotificationIconBitmap == null) {
            Drawable notificationIconDrawable = ApiCompatibilityUtils.getUserBadgedIcon(
                    mContext, R.drawable.audio_playing);
            mNotificationIconBitmap = drawableToBitmap(notificationIconDrawable);
        }

        if (mNotificationBuilder == null) {
            mNotificationBuilder = new NotificationCompat.Builder(mContext)
                .setSmallIcon(R.drawable.audio_playing)
                .setAutoCancel(false)
                .setDeleteIntent(mService.getPendingIntent(ListenerService.ACTION_STOP_SELF));
        }
        mNotificationBuilder.setOngoing(!mMediaNotificationInfo.isPaused);
        mNotificationBuilder.setContentIntent(createContentIntent());

        RemoteViews contentView = createContentView();

        contentView.setTextViewText(R.id.title, mMediaNotificationInfo.title);
        contentView.setTextViewText(R.id.status, getStatus());
        if (mNotificationIconBitmap != null) {
            contentView.setImageViewBitmap(R.id.icon, mNotificationIconBitmap);
        } else {
            contentView.setImageViewResource(R.id.icon, R.drawable.audio_playing);
        }

        if (mMediaNotificationInfo.isPaused) {
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

        mNotificationBuilder.setContent(contentView);
        mNotificationBuilder.setVisibility(
                mMediaNotificationInfo.isPrivate ? NotificationCompat.VISIBILITY_PRIVATE
                                                 : NotificationCompat.VISIBILITY_PUBLIC);

        Notification notification = mNotificationBuilder.build();

        // We keep the service as a foreground service while the media is playing. When it is not,
        // the service isn't stopped but is no longer in foreground, thus at a lower priority.
        // While the service is in foreground, the associated notification can't be swipped away.
        // Moving it back to background allows the user to remove the notification.
        if (mMediaNotificationInfo.isPaused) {
            mService.stopForeground(false /* removeNotification */);

            NotificationManagerCompat manager = NotificationManagerCompat.from(mContext);
            manager.notify(R.id.media_playback_notification, notification);
        } else {
            mService.startForeground(R.id.media_playback_notification, notification);
        }
    }

    private Bitmap drawableToBitmap(Drawable drawable) {
        if (!(drawable instanceof BitmapDrawable)) return null;

        BitmapDrawable bitmapDrawable = (BitmapDrawable) drawable;
        return bitmapDrawable.getBitmap();
    }
}
