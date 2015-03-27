// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Handler;
import android.os.IBinder;
import android.support.v4.app.NotificationCompat;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.RemoteViews;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.media.remote.RemoteVideoInfo.PlayerState;

import java.util.Set;

import javax.annotation.Nullable;

/**
 * A class for notifications that provide information and optional transport controls for a given
 * remote control. Internally implements a Service for transforming notification Intents into
 * {@link TransportControl.Listener} calls for all registered listeners.
 *
 */
public class NotificationTransportControl
        extends TransportControl implements MediaRouteController.UiListener {
    /**
     * Service used to transform intent requests triggered from the notification into
     * {@code Listener} callbacks. Ideally this class should be protected, but public is required to
     * create as a service.
     */
    public static class ListenerService extends Service {
        private static final String ACTION_PREFIX = ListenerService.class.getName() + ".";

        // Constants used by intent actions
        public static final int ACTION_ID_PLAY = 0;
        public static final int ACTION_ID_PAUSE = 1;
        public static final int ACTION_ID_SEEK = 2;
        public static final int ACTION_ID_STOP = 3;
        public static final int ACTION_ID_SELECT = 4;

        // Intent parameters
        public static final String SEEK_POSITION = "SEEK_POSITION";

        // Must be kept in sync with the ACTION_ID_XXX constants above
        private static final String[] ACTION_VERBS = {"PLAY", "PAUSE", "SEEK", "STOP", "SELECT" };

        private PendingIntent[] mPendingIntents;

        @VisibleForTesting
        PendingIntent getPendingIntent(int id) {
            return mPendingIntents[id];
        }

        @Override
        public IBinder onBind(Intent intent) {
            return null;
        }

        @Override
        public void onCreate() {
            super.onCreate();

            // Create all the PendingIntents
            int actionCount = ACTION_VERBS.length;
            mPendingIntents = new PendingIntent[actionCount];
            for (int i = 0; i < actionCount; ++i) {
                Intent intent = new Intent(this, ListenerService.class);
                intent.setAction(ACTION_PREFIX + ACTION_VERBS[i]);
                mPendingIntents[i] = PendingIntent.getService(this, 0, intent,
                        PendingIntent.FLAG_CANCEL_CURRENT);
            }
            if (sInstance == null) {
                // This can only happen if we have been recreated by the OS after Chrome has died.
                // In this case we need to create a MediaRouteController so that we can reconnect
                // to the Chromecast.
                RemoteMediaPlayerController playerController =
                        RemoteMediaPlayerController.instance();
                playerController.createMediaRouteControllers(this);
                for (MediaRouteController routeController :
                        playerController.getMediaRouteControllers()) {
                    routeController.initialize();
                    if (routeController.reconnectAnyExistingRoute()) {
                        playerController.setCurrentMediaRouteController(routeController);
                        routeController.prepareMediaRoute();
                        NotificationTransportControl.getOrCreate(this, routeController);
                        sInstance.addListener(routeController);
                        break;
                    }
                }
                if (sInstance == null) {
                    // No controller wants to reconnect, so we haven't created a notification.
                    return;
                }
            }
            onServiceStarted(this);
        }

        @Override
        public void onDestroy() {
            onServiceDestroyed();
        }

        @Override
        public int onStartCommand(Intent intent, int flags, int startId) {
            if (sInstance == null) {
                // This can only happen after a restart where none of the controllers
                // wanted to reconnect.
                stopSelf();
                return START_NOT_STICKY;
            }
            if (intent != null) {
                String action = intent.getAction();
                if (action != null && action.startsWith(ACTION_PREFIX)) {
                    Set<Listener> listeners = sInstance.getListeners();

                    // Strip the prefix for matching the verb
                    action = action.substring(ACTION_PREFIX.length());
                    if (ACTION_VERBS[ACTION_ID_PLAY].equals(action)) {
                        for (Listener listener : listeners) {
                            listener.onPlay();
                        }
                    } else if (ACTION_VERBS[ACTION_ID_PAUSE].equals(action)) {
                        for (Listener listener : listeners) {
                            listener.onPause();
                        }
                    } else if (ACTION_VERBS[ACTION_ID_SEEK].equals(action)) {
                        int seekPosition = intent.getIntExtra(SEEK_POSITION, 0);
                        for (Listener listener : listeners) {
                            listener.onSeek(seekPosition);
                        }
                    } else if (ACTION_VERBS[ACTION_ID_STOP].equals(action)) {
                        for (Listener listener : listeners) {
                            listener.onStop();
                            stopSelf();
                        }
                    } else if (ACTION_VERBS[ACTION_ID_SELECT].equals(action)) {
                        for (Listener listener : listeners) {
                            ExpandedControllerActivity.startActivity(this);
                        }
                    }
                }
            }

            return START_STICKY;
        }
    }

    private static NotificationTransportControl sInstance = null;
    private static final Object LOCK = new Object();
    private static final int MSG_UPDATE_NOTIFICATION = 100;

    private static final int MINIMUM_PROGRESS_UPDATE_INTERVAL_MS = 1000;

    /**
     * Returns the singleton NotificationTransportControl object.
     *
     * @param context The Context that the notification service needs to be created in.
     * @param mrc The MediaRouteController object to use.
     * @return A {@code NotificationTransportControl} object that uses the given
     *         MediaRouteController object.
     */
    public static NotificationTransportControl getOrCreate(Context context,
            @Nullable MediaRouteController mrc) {
        synchronized (LOCK) {
            if (sInstance == null) {
                sInstance = new NotificationTransportControl(context);
                sInstance.setVideoInfo(
                        new RemoteVideoInfo(null, 0, RemoteVideoInfo.PlayerState.STOPPED, 0, null));
            }

            sInstance.setMediaRouteController(mrc);
            return sInstance;
        }
    }

    @VisibleForTesting
    static NotificationTransportControl getIfExists() {
        return sInstance;
    }

    /**
     * Ensures the truth of an expression involving the state of the calling instance, but not
     * involving any parameters to the calling method.
     *
     * @param expression a boolean expression
     * @throws IllegalStateException if {@code expression} is false
     */
    private static void checkState(boolean expression) {
        if (!expression) {
            throw new IllegalStateException();
        }
    }

    private static void onServiceDestroyed() {
        if (sInstance == null) return;
        checkState(sInstance.mService != null);
        sInstance.destroyNotification();
        sInstance.mService = null;
    }

    private static void onServiceStarted(ListenerService service) {
        checkState(sInstance != null);
        checkState(sInstance.mService == null);
        sInstance.mService = service;
        sInstance.createNotification();
    }

    /**
     * Scale the specified bitmap to the desired with and height while preserving aspect ratio.
     */
    private static Bitmap scaleBitmap(Bitmap bitmap, int maxWidth, int maxHeight) {
        if (bitmap == null) {
            return null;
        }

        float scaleX = 1.0f;
        float scaleY = 1.0f;
        if (bitmap.getWidth() > maxWidth) {
            scaleX = maxWidth / (float) bitmap.getWidth();
        }
        if (bitmap.getHeight() > maxHeight) {
            scaleY = maxHeight / (float) bitmap.getHeight();
        }
        float scale = Math.min(scaleX, scaleY);
        int width = (int) (bitmap.getWidth() * scale);
        int height = (int) (bitmap.getHeight() * scale);
        return Bitmap.createScaledBitmap(bitmap, width, height, false);
    }

    private final Context mContext;
    private MediaRouteController mMediaRouteController;

    // ListenerService running for the notification. Only non-null when showing.
    private ListenerService mService;

    private final String mPlayDescription;

    private final String mPauseDescription;

    private Notification mNotification;

    private Bitmap mIcon;

    private Handler mHandler;

    private int mProgressUpdateInterval = MINIMUM_PROGRESS_UPDATE_INTERVAL_MS;

    private NotificationTransportControl(Context context) {
        this.mContext = context;
        mHandler = new Handler(context.getMainLooper()) {
            @Override
            public void handleMessage(android.os.Message msg) {
                if (msg.what == MSG_UPDATE_NOTIFICATION) {
                    mHandler.removeMessages(MSG_UPDATE_NOTIFICATION); // Only one update is needed.
                    updateNotificationInternal();
                }
            }
        };

        mPlayDescription = context.getResources().getString(R.string.accessibility_play);
        mPauseDescription = context.getResources().getString(R.string.accessibility_pause);
    }

    @Override
    public void hide() {
        mContext.stopService(new Intent(mContext, ListenerService.class));
    }

    /**
     * @return true if the notification is currently visible to the user.
     */
    public boolean isShowing() {
        return mService != null;
    }

    @Override
    public void onDurationUpdated(int durationMillis) {
        RemoteVideoInfo videoInfo = new RemoteVideoInfo(getVideoInfo());
        videoInfo.durationMillis = durationMillis;
        setVideoInfo(videoInfo);

        // Set the progress update interval based on the screen height/width, since there's no point
        // in updating the progress bar more frequently than what the user can see.
        // getDisplayMetrics() is dependent on the current orientation, so we need to get the max
        // of both height and width so that both portrait and landscape notifications are covered.
        DisplayMetrics metrics = mContext.getResources().getDisplayMetrics();
        float density = metrics.density;
        float dpHeight = metrics.heightPixels / density;
        float dpWidth = metrics.widthPixels / density;

        float maxDimen = Math.max(dpHeight, dpWidth);

        mProgressUpdateInterval = Math.max(MINIMUM_PROGRESS_UPDATE_INTERVAL_MS,
                Math.round(durationMillis / maxDimen));
    }

    @Override
    public void onError(int error, String errorMessage) {
        // Stop the session for all errors
        hide();
    }

    @Override
    public void onPlaybackStateChanged(PlayerState oldState, PlayerState newState) {
        RemoteVideoInfo videoInfo = new RemoteVideoInfo(getVideoInfo());
        videoInfo.state = newState;
        setVideoInfo(videoInfo);

        if (newState == oldState) return;

        if (newState == PlayerState.PLAYING || newState == PlayerState.LOADING
                || newState == PlayerState.PAUSED) {
            show(newState);
            if (newState == PlayerState.PLAYING) {
                // If we transitioned from not playing to playing, start monitoring the playback.
                monitorProgress();
            }
        } else if (isShowing()
                && (newState == PlayerState.FINISHED || newState == PlayerState.INVALIDATED)) {
            // If we are switching to a finished state, stop the notifications.
            hide();
        }
    }

    @Override
    public void onPositionChanged(int positionMillis) {
        RemoteVideoInfo videoInfo = new RemoteVideoInfo(getVideoInfo());
        videoInfo.currentTimeMillis = positionMillis;
        setVideoInfo(videoInfo);
    }

    @Override
    public void onPrepared(MediaRouteController mediaRouteController) {
        show(PlayerState.PLAYING);
    }

    @Override
    public void onRouteSelected(String name, MediaRouteController mediaRouteController) {
        setScreenName(name);
    }

    @Override
    public void onRouteUnselected(MediaRouteController mediaRouteController) {
        hide();
    }

    @Override
    public void onTitleChanged(String title) {
        RemoteVideoInfo videoInfo = new RemoteVideoInfo(getVideoInfo());
        videoInfo.title = title;
        setVideoInfo(videoInfo);
    }

    @Override
    public void setRouteController(MediaRouteController controller) {
        setMediaRouteController(controller);
    }

    @Override
    public void show(PlayerState initialState) {
        mMediaRouteController.addUiListener(this);
        RemoteVideoInfo newVideoInfo = new RemoteVideoInfo(mVideoInfo);
        newVideoInfo.state = initialState;
        setVideoInfo(newVideoInfo);
        mContext.startService(new Intent(mContext, ListenerService.class));

        if (initialState == PlayerState.PLAYING) {
            monitorProgress();
        }
    }

    private void updateNotification() {
        checkState(mNotification != null);

        // Defer the call to updateNotificationInternal() so it can be cancelled in
        // destroyNotification(). This is done to avoid the OS bug b/8798662.
        mHandler.sendEmptyMessage(MSG_UPDATE_NOTIFICATION);
    }

    @VisibleForTesting
    Notification getNotification() {
        return mNotification;
    }

    @VisibleForTesting
    final ListenerService getService() {
        return mService;
    }

    @Override
    protected void onErrorChanged() {
        if (isShowing()) {
            updateNotification();
        }
    }

    @Override
    protected void onPosterBitmapChanged() {
        Bitmap posterBitmap = getPosterBitmap();
        mIcon = scaleBitmapForIcon(posterBitmap);
        super.onPosterBitmapChanged();
    }

    @Override
    protected void onScreenNameChanged() {
        if (isShowing()) {
            updateNotification();
        }
    }

    @Override
    protected void onVideoInfoChanged() {
        if (isShowing()) {
            updateNotification();
        }
    }

    private RemoteViews createContentView() {
        RemoteViews contentView =
                new RemoteViews(getContext().getPackageName(), R.layout.remote_notification_bar);
        contentView.setOnClickPendingIntent(R.id.stop,
                getService().getPendingIntent(ListenerService.ACTION_ID_STOP));

        return contentView;
    }

    private void createNotification() {
        checkState(mNotification == null);

        NotificationCompat.Builder notificationBuilder =
                new NotificationCompat.Builder(getContext())
                .setDefaults(0)
                .setSmallIcon(R.drawable.ic_notification_media_route)
                .setLocalOnly(true)
                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                .setOnlyAlertOnce(true)
                .setOngoing(true)
                .setContent(createContentView())
                .setContentIntent(getService().getPendingIntent(ListenerService.ACTION_ID_SELECT))
                .setDeleteIntent(getService().getPendingIntent(ListenerService.ACTION_ID_STOP));
        mNotification = notificationBuilder.build();
        updateNotification();
    }

    private void destroyNotification() {
        checkState(mNotification != null);

        // Cancel any pending updates - we're about to tear down the notification.
        mHandler.removeMessages(MSG_UPDATE_NOTIFICATION);

        NotificationManager manager =
                (NotificationManager) getContext().getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(R.id.remote_notification);
        mNotification = null;
    }

    private final Context getContext() {
        return mContext;
    }

    private String getStatus() {
        Context context = getContext();
        RemoteVideoInfo videoInfo = getVideoInfo();
        String videoTitle = videoInfo != null ? videoInfo.title : null;
        if (hasError()) {
            return getError();
        } else if (videoInfo != null) {
            switch (videoInfo.state) {
                case PLAYING:
                    return videoTitle != null ? context.getString(
                            R.string.cast_notification_playing_for_video, videoTitle)
                            : context.getString(R.string.cast_notification_playing);
                case LOADING:
                    return videoTitle != null ? context.getString(
                            R.string.cast_notification_loading_for_video, videoTitle)
                            : context.getString(R.string.cast_notification_loading);
                case PAUSED:
                    return videoTitle != null ? context.getString(
                            R.string.cast_notification_paused_for_video, videoTitle)
                            : context.getString(R.string.cast_notification_paused);
                case STOPPED:
                    return context.getString(R.string.cast_notification_stopped);
                case FINISHED:
                case INVALIDATED:
                    return videoTitle != null ? context.getString(
                            R.string.cast_notification_finished_for_video, videoTitle)
                            : context.getString(R.string.cast_notification_finished);
                case ERROR:
                default:
                    return videoInfo.errorMessage;
            }
        } else {
            return ""; // TODO(bclayton): Is there something better to display here?
        }
    }

    private String getTitle() {
        return getScreenName();
    }

    private void monitorProgress() {
        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                onPositionChanged(mMediaRouteController.getPosition());
                if (mMediaRouteController.isPlaying()) {
                    mHandler.postDelayed(this, mProgressUpdateInterval);
                }
            }
        }, mProgressUpdateInterval);

    }

    /**
     * Scale the specified bitmap to properly fit as a notification icon. If the argument is null
     * the function returns null.
     */
    private Bitmap scaleBitmapForIcon(Bitmap bitmap) {
        Resources res = getContext().getResources();
        float maxWidth = res.getDimension(R.dimen.remote_notification_logo_max_width);
        float maxHeight = res.getDimension(R.dimen.remote_notification_logo_max_height);
        return scaleBitmap(bitmap, (int) maxWidth, (int) maxHeight);
    }

    /**
     * Sets the MediaRouteController the notification should be using to get the data from.
     *
     * @param mrc the MediaRouteController object to use. If null, the previous MediaRouteController
     *        object will not be overwritten.
     */
    private void setMediaRouteController(@Nullable MediaRouteController mrc) {
        if (mrc == null || mMediaRouteController == mrc) return;

        if (mMediaRouteController != null) {
            mMediaRouteController.removeUiListener(this);
        }

        mMediaRouteController = mrc;
        mMediaRouteController.addUiListener(this);
    }

    private void updateNotificationInternal() {
        checkState(mNotification != null);

        RemoteViews contentView = createContentView();

        contentView.setTextViewText(R.id.title, getTitle());
        contentView.setTextViewText(R.id.status, getStatus());
        if (mIcon != null) {
            contentView.setImageViewBitmap(R.id.icon, mIcon);
        } else {
            contentView.setImageViewResource(R.id.icon, R.drawable.ic_notification_media_route);
        }

        RemoteVideoInfo videoInfo = getVideoInfo();
        if (videoInfo != null) {
            boolean showPlayPause = false;
            boolean showProgress = false;
            switch (videoInfo.state) {
                case PLAYING:
                    showProgress = true;
                    showPlayPause = true;
                    contentView.setProgressBar(R.id.progress, videoInfo.durationMillis,
                            videoInfo.currentTimeMillis, false);
                    contentView.setImageViewResource(R.id.playpause,
                            R.drawable.ic_vidcontrol_pause);
                    ApiCompatibilityUtils.setContentDescriptionForRemoteView(contentView,
                            R.id.playpause, mPauseDescription);
                    contentView.setOnClickPendingIntent(R.id.playpause,
                            getService().getPendingIntent(ListenerService.ACTION_ID_PAUSE));
                    break;
                case PAUSED:
                    showProgress = true;
                    showPlayPause = true;
                    contentView.setProgressBar(R.id.progress, videoInfo.durationMillis,
                            videoInfo.currentTimeMillis, false);
                    contentView.setImageViewResource(R.id.playpause, R.drawable.ic_vidcontrol_play);
                    ApiCompatibilityUtils.setContentDescriptionForRemoteView(contentView,
                            R.id.playpause, mPlayDescription);
                    contentView.setOnClickPendingIntent(R.id.playpause,
                            getService().getPendingIntent(ListenerService.ACTION_ID_PLAY));
                    break;
                case LOADING:
                    showProgress = true;
                    contentView.setProgressBar(R.id.progress, 0, 0, true);
                    break;
                case ERROR:
                    showProgress = true;
                    break;
                default:
                    break;
            }

            contentView.setViewVisibility(R.id.playpause,
                    showPlayPause ? View.VISIBLE : View.INVISIBLE);
            // We use GONE instead of INVISIBLE for this because the notification looks funny with
            // a large gap in the middle if we have no duration. Setting it to GONE forces the
            // layout to squeeze tighter to the middle.
            contentView.setViewVisibility(R.id.progress,
                    (showProgress && videoInfo.durationMillis > 0) ? View.VISIBLE : View.GONE);
            contentView.setViewVisibility(R.id.stop, showPlayPause ? View.VISIBLE : View.INVISIBLE);

            mNotification.contentView = contentView;

            NotificationManager manager = (NotificationManager) getContext().getSystemService(
                    Context.NOTIFICATION_SERVICE);
            manager.notify(R.id.remote_notification, mNotification);

            if (videoInfo.state == PlayerState.STOPPED || videoInfo.state == PlayerState.FINISHED) {
                getService().stopSelf();
            } else {
                getService().startForeground(R.id.remote_notification, mNotification);
            }
        }
    }
}
