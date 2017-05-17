// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.MediaController;
import android.widget.MediaController.MediaPlayerControl;
import android.widget.TextView;

import com.google.android.gms.cast.CastMediaControlIntent;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.media.remote.RemoteVideoInfo.PlayerState;
import org.chromium.chrome.browser.metrics.MediaNotificationUma;

/**
 * The activity that's opened by clicking the video flinging (casting) notification.
 *
 * TODO(aberent): Refactor to merge some common logic with {@link CastNotificationControl}.
 */
public class ExpandedControllerActivity
        extends FragmentActivity implements MediaRouteController.UiListener {
    private static final int PROGRESS_UPDATE_PERIOD_IN_MS = 1000;
    // The alpha value for the poster/placeholder image, an integer between 0 and 256 (opaque).
    private static final int POSTER_IMAGE_ALPHA = 200;

    // Subclass of {@link android.widget.MediaController} that never hides itself.
    class AlwaysShownMediaController extends MediaController {
        public AlwaysShownMediaController(Context context) {
            super(context);
        }

        @Override
        public void show(int timeout) {
            // Never auto-hide the controls.
            super.show(0);
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event) {
            int keyCode = event.getKeyCode();
            // MediaController hides the controls when back or menu are pressed.
            // Close the activity on back and ignore menu.
            if (keyCode == KeyEvent.KEYCODE_BACK) {
                finish();
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_MENU) {
                return true;
            }
            return super.dispatchKeyEvent(event);
        }

        @Override
        public void hide() {
            // Don't allow the controls to hide until explicitly asked to do so from the host
            // activity.
        }

        /**
         * Actually hides the controls which prevents some window leaks.
         */
        public void cleanup() {
            super.hide();
        }
    };

    private Handler mHandler;
    private AlwaysShownMediaController mMediaController;
    private FullscreenMediaRouteButton mMediaRouteButton;
    private MediaRouteController mMediaRouteController;
    private RemoteVideoInfo mVideoInfo;
    private String mScreenName;

    /**
     * Handle actions from on-screen media controls.
     */
    private MediaPlayerControl mMediaPlayerControl = new MediaPlayerControl() {
        @Override
        public boolean canPause() {
            return true;
        }

        @Override
        public boolean canSeekBackward() {
            return getDuration() > 0 && getCurrentPosition() > 0;
        }

        @Override
        public boolean canSeekForward() {
            return getDuration() > 0 && getCurrentPosition() < getDuration();
        }

        @Override
        public int getAudioSessionId() {
            // TODO(avayvod): not sure 0 is a valid value to return.
            return 0;
        }

        @Override
        public int getBufferPercentage() {
            int duration = getDuration();
            if (duration == 0) return 0;
            return (getCurrentPosition() * 100) / duration;
        }

        @Override
        public int getCurrentPosition() {
            if (mMediaRouteController == null) return 0;
            return (int) mMediaRouteController.getPosition();
        }

        @Override
        public int getDuration() {
            if (mMediaRouteController == null) return 0;
            return (int) mMediaRouteController.getDuration();
        }

        @Override
        public boolean isPlaying() {
            if (mMediaRouteController == null) return false;
            return mMediaRouteController.isPlaying();
        }

        @Override
        public void pause() {
            if (mMediaRouteController == null) return;
            mMediaRouteController.pause();
            RecordCastAction.recordFullscreenControlsAction(
                    RecordCastAction.FULLSCREEN_CONTROLS_PAUSE,
                    mMediaRouteController.getMediaStateListener() != null);
        }

        @Override
        public void start() {
            if (mMediaRouteController == null) return;
            mMediaRouteController.resume();
            RecordCastAction.recordFullscreenControlsAction(
                    RecordCastAction.FULLSCREEN_CONTROLS_RESUME,
                    mMediaRouteController.getMediaStateListener() != null);
        }

        @Override
        public void seekTo(int pos) {
            if (mMediaRouteController == null) return;
            mMediaRouteController.seekTo(pos);
            RecordCastAction.recordFullscreenControlsAction(
                    RecordCastAction.FULLSCREEN_CONTROLS_SEEK,
                    mMediaRouteController.getMediaStateListener() != null);
        }
    };

    private Runnable mControlsUpdater = new Runnable() {
        @Override
        public void run() {
            mMediaController.show();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        MediaNotificationUma.recordClickSource(getIntent());

        mMediaRouteController =
                RemoteMediaPlayerController.instance().getCurrentlyPlayingMediaRouteController();

        if (mMediaRouteController == null || mMediaRouteController.routeIsDefaultRoute()) {
            // We don't want to do anything for the default (local) route
            finish();
            return;
        }

        // Make the activity full screen.
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        // requestWindowFeature must be called before adding content.
        setContentView(R.layout.expanded_cast_controller);
        mHandler = new Handler();

        ViewGroup rootView = (ViewGroup) findViewById(android.R.id.content);
        rootView.setBackgroundColor(Color.BLACK);

        mMediaRouteController.addUiListener(this);

        // Create and initialize the media control UI.
        mMediaController = new AlwaysShownMediaController(this);
        mMediaController.setEnabled(true);
        mMediaController.setMediaPlayer(mMediaPlayerControl);
        mMediaController.setAnchorView(rootView);

        View button = getLayoutInflater().inflate(R.layout.cast_controller_media_route_button,
                rootView, false);

        if (button instanceof FullscreenMediaRouteButton) {
            mMediaRouteButton = (FullscreenMediaRouteButton) button;
            rootView.addView(mMediaRouteButton);
            mMediaRouteButton.bringToFront();
            mMediaRouteButton.initialize(mMediaRouteController);
        } else {
            mMediaRouteButton = null;
        }

        // Initialize the video info.
        mVideoInfo = new RemoteVideoInfo(null, 0, RemoteVideoInfo.PlayerState.STOPPED, 0, null);
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (mVideoInfo.state == PlayerState.FINISHED) finish();
        if (mMediaRouteController == null) return;

        // Lifetime of the media element is bound to that of the {@link MediaStateListener}
        // of the {@link MediaRouteController}.
        RecordCastAction.recordFullscreenControlsShown(
                mMediaRouteController.getMediaStateListener() != null);

        mMediaRouteController.prepareMediaRoute();

        ImageView iv = (ImageView) findViewById(R.id.cast_background_image);
        if (iv == null) return;
        Bitmap posterBitmap = mMediaRouteController.getPoster();
        if (posterBitmap != null) iv.setImageBitmap(posterBitmap);
        iv.setImageAlpha(POSTER_IMAGE_ALPHA);

        // Can't show the media controller until attached to window.
        scheduleControlsUpdate();
    }

    @Override
    protected void onDestroy() {
        cleanup();
        super.onDestroy();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        if ((keyCode != KeyEvent.KEYCODE_VOLUME_DOWN && keyCode != KeyEvent.KEYCODE_VOLUME_UP)
                || mVideoInfo.state == PlayerState.FINISHED) {
            return super.dispatchKeyEvent(event);
        }

        return handleVolumeKeyEvent(mMediaRouteController, event);
    }

    private void cleanup() {
        if (mHandler != null) mHandler.removeCallbacks(mControlsUpdater);
        if (mMediaRouteController != null) mMediaRouteController.removeUiListener(this);
        mMediaRouteController = null;
        mControlsUpdater = null;
        mMediaController.cleanup();
        mMediaController = null;
    }

    /**
     * Sets the remote's video information to display.
     */
    private final void setVideoInfo(RemoteVideoInfo videoInfo) {
        if ((mVideoInfo == null) ? (videoInfo == null) : mVideoInfo.equals(videoInfo)) return;

        mVideoInfo = videoInfo;
        updateUi();
    }

    private void scheduleControlsUpdate() {
        mHandler.removeCallbacks(mControlsUpdater);
        mHandler.post(mControlsUpdater);
    }

    /**
     * Sets the name to display for the device.
     */
    private void setScreenName(String screenName) {
        if (TextUtils.equals(mScreenName, screenName)) return;

        mScreenName = screenName;
        updateUi();
    }

    private void updateUi() {
        if (mMediaController == null || mMediaRouteController == null) return;

        String deviceName = mMediaRouteController.getRouteName();
        String castText = "";
        if (deviceName != null) {
            castText = getResources().getString(R.string.cast_casting_video, deviceName);
        }
        TextView castTextView = (TextView) findViewById(R.id.cast_screen_title);
        castTextView.setText(castText);

        scheduleControlsUpdate();
    }

    @Override
    public void onRouteSelected(String name, MediaRouteController mediaRouteController) {
        setScreenName(name);
    }

    @Override
    public void onRouteUnselected(MediaRouteController mediaRouteController) {
        finish();
    }

    @Override
    public void onPrepared(MediaRouteController mediaRouteController) {
        // No implementation.
    }

    @Override
    public void onError(int error, String message) {
        if (error == CastMediaControlIntent.ERROR_CODE_SESSION_START_FAILED) finish();
    }

    @Override
    public void onPlaybackStateChanged(PlayerState newState) {
        RemoteVideoInfo videoInfo = new RemoteVideoInfo(mVideoInfo);
        videoInfo.state = newState;
        setVideoInfo(videoInfo);

        scheduleControlsUpdate();

        if (newState == PlayerState.FINISHED || newState == PlayerState.INVALIDATED) {
            // If we are switching to a finished state, stop the notifications.
            finish();
        }
    }

    @Override
    public void onDurationUpdated(long durationMillis) {
        RemoteVideoInfo videoInfo = new RemoteVideoInfo(mVideoInfo);
        videoInfo.durationMillis = durationMillis;
        setVideoInfo(videoInfo);
    }

    @Override
    public void onPositionChanged(long positionMillis) {
        RemoteVideoInfo videoInfo = new RemoteVideoInfo(mVideoInfo);
        videoInfo.currentTimeMillis = positionMillis;
        setVideoInfo(videoInfo);
    }

    @Override
    public void onTitleChanged(String title) {
        RemoteVideoInfo videoInfo = new RemoteVideoInfo(mVideoInfo);
        videoInfo.title = title;
        setVideoInfo(videoInfo);
    }

    /**
     * Modify remote volume by handling volume keys.
     *
     * @param controller The remote controller through which the volume will be modified.
     * @param event The key event. Its keycode needs to be either {@code KEYCODE_VOLUME_DOWN} or
     *              {@code KEYCODE_VOLUME_UP} otherwise this method will return false.
     * @return True if the event is handled.
     */
    private boolean handleVolumeKeyEvent(MediaRouteController controller, KeyEvent event) {
        if (!controller.isBeingCast()) return false;

        int action = event.getAction();
        int keyCode = event.getKeyCode();
        // Intercept the volume keys to affect only remote volume.
        switch (keyCode) {
            case KeyEvent.KEYCODE_VOLUME_DOWN:
                if (action == KeyEvent.ACTION_DOWN) controller.setRemoteVolume(-1);
                return true;
            case KeyEvent.KEYCODE_VOLUME_UP:
                if (action == KeyEvent.ACTION_DOWN) controller.setRemoteVolume(1);
                return true;
            default:
                return false;
        }
    }

    /**
     * Launches the ExpandedControllerActivity as a new task.
     *
     * @param context the Context to start this activity within.
     */
    public static void startActivity(Context context) {
        if (context == null) return;

        Intent intent = new Intent(context, ExpandedControllerActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }
}
