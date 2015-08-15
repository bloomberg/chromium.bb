// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.IBinder;
import android.provider.Browser;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.view.KeyEvent;
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
    private static final int PLAYBACK_STATE_PAUSED = 0;
    private static final int PLAYBACK_STATE_PLAYING = 1;

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
        private static final String ACTION_MEDIA_BUTTON =
                "NotificationMediaPlaybackControls.ListenerService.MEDIA_BUTTON";

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

            if (sInstance == null) return;

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

            // Before Android L, instead of using the MediaSession callback, the system will fire
            // ACTION_MEDIA_BUTTON intents which stores the information about the key event.
            if (ACTION_MEDIA_BUTTON.equals(action)) {
                assert Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP;

                KeyEvent event = (KeyEvent) intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
                if (event == null) return START_NOT_STICKY;
                if (event.getAction() != KeyEvent.ACTION_DOWN) return START_NOT_STICKY;

                switch (event.getKeyCode()) {
                    case KeyEvent.KEYCODE_MEDIA_PLAY:
                        if (!sInstance.mMediaNotificationInfo.isPaused) break;
                        sInstance.onPlaybackStateChanged(PLAYBACK_STATE_PLAYING);
                        break;
                    case KeyEvent.KEYCODE_MEDIA_PAUSE:
                        if (sInstance.mMediaNotificationInfo.isPaused) break;
                        sInstance.onPlaybackStateChanged(PLAYBACK_STATE_PAUSED);
                        break;
                    case KeyEvent.KEYCODE_HEADSETHOOK:
                    case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                        if (sInstance.mMediaNotificationInfo.isPaused) {
                            sInstance.onPlaybackStateChanged(PLAYBACK_STATE_PLAYING);
                        } else {
                            sInstance.onPlaybackStateChanged(PLAYBACK_STATE_PAUSED);
                        }
                        break;
                    default:
                        break;
                }
                return START_NOT_STICKY;
            }

            if (ACTION_STOP_SELF.equals(action)) {
                stopSelf();
                return START_NOT_STICKY;
            }

            if (ACTION_PLAY.equals(action)) {
                sInstance.onPlaybackStateChanged(PLAYBACK_STATE_PLAYING);
            } else if (ACTION_PAUSE.equals(action)) {
                sInstance.onPlaybackStateChanged(PLAYBACK_STATE_PAUSED);
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

        clear();
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

    private MediaSessionCompat mMediaSession;

    private final MediaSessionCompat.Callback mMediaSessionCallback =
            new MediaSessionCompat.Callback() {
                @Override
                public void onPlay() {
                    if (!sInstance.mMediaNotificationInfo.isPaused) return;
                    sInstance.onPlaybackStateChanged(PLAYBACK_STATE_PLAYING);
                }

                @Override
                public void onPause() {
                    if (sInstance.mMediaNotificationInfo.isPaused) return;
                    sInstance.onPlaybackStateChanged(PLAYBACK_STATE_PAUSED);
                }
    };

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

        if (mMediaSession != null) {
            mMediaSession.setActive(false);
            mMediaSession.release();
            mMediaSession = null;
        }
        mMediaNotificationInfo = null;
        mContext.stopService(new Intent(mContext, ListenerService.class));
    }

    private void hideNotification(int tabId) {
        if (mMediaNotificationInfo == null || tabId != mMediaNotificationInfo.tabId) return;
        clearNotification();
    }

    private void onPlaybackStateChanged(int playbackState) {
        assert mMediaNotificationInfo != null;
        assert playbackState == PLAYBACK_STATE_PLAYING || playbackState == PLAYBACK_STATE_PAUSED;

        mMediaNotificationInfo = new MediaNotificationInfo(
                mMediaNotificationInfo.title,
                playbackState == PLAYBACK_STATE_PAUSED,
                mMediaNotificationInfo.origin,
                mMediaNotificationInfo.tabId,
                mMediaNotificationInfo.isPrivate,
                mMediaNotificationInfo.listener);
        updateNotification();

        if (playbackState == PLAYBACK_STATE_PAUSED) {
            mMediaNotificationInfo.listener.onPause();
        } else {
            mMediaNotificationInfo.listener.onPlay();
        }
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

    private MediaMetadataCompat createMetadata() {
        MediaMetadataCompat.Builder metadataBuilder = new MediaMetadataCompat.Builder();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_TITLE,
                    mMediaNotificationInfo.title);
            metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_SUBTITLE,
                    mMediaNotificationInfo.origin);
            // TODO(mlamouri): this is temporary until we figure out which icon we want to show.
            metadataBuilder.putBitmap(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON,
                    BitmapFactory.decodeResource(mContext.getResources(), R.mipmap.app_icon));
        } else {
            metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_TITLE,
                    mMediaNotificationInfo.title);
            metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ARTIST,
                    mMediaNotificationInfo.origin);
            // TODO(mlamouri): we don't show any kind of placeholder art for pre-L because it
            // would be shown on the lockscreen. Having a big application icon in there is not
            // what the user would expect. We will be able to show something when the page will
            // be able to provide art via the MediaSession API.
        }

        return metadataBuilder.build();
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
                .setLocalOnly(true)
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

        if (mMediaSession == null) {
            mMediaSession = new MediaSessionCompat(mContext, mContext.getString(R.string.app_name),
                    new ComponentName(mService.getPackageName(), ListenerService.class.getName()),
                    mService.getPendingIntent(ListenerService.ACTION_MEDIA_BUTTON));
            mMediaSession.setFlags(MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS
                    | MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
            mMediaSession.setCallback(mMediaSessionCallback);
            mMediaSession.setActive(true);
        }

        MediaMetadataCompat.Builder metadataBuilder = new MediaMetadataCompat.Builder();
        metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_TITLE,
                mMediaNotificationInfo.title);
        metadataBuilder.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_SUBTITLE,
                mMediaNotificationInfo.origin);
        metadataBuilder.putBitmap(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON,
                BitmapFactory.decodeResource(mContext.getResources(), R.mipmap.app_icon));
        mMediaSession.setMetadata(createMetadata());

        PlaybackStateCompat.Builder playbackStateBuilder = new PlaybackStateCompat.Builder()
                .setActions(PlaybackStateCompat.ACTION_PLAY | PlaybackStateCompat.ACTION_PAUSE);
        if (mMediaNotificationInfo.isPaused) {
            playbackStateBuilder.setState(PlaybackStateCompat.STATE_PAUSED,
                    PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, 1.0f);
        } else {
            playbackStateBuilder.setState(PlaybackStateCompat.STATE_PLAYING,
                    PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN, 1.0f);
        }
        mMediaSession.setPlaybackState(playbackStateBuilder.build());

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
