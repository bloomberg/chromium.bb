// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.MediaController;
import android.widget.MediaController.MediaPlayerControl;

import java.lang.ref.WeakReference;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.app.AppResource;
import org.chromium.content.common.ISandboxedProcessService;

@JNINamespace("content")
public class ContentVideoView extends FrameLayout implements MediaPlayerControl,
        SurfaceHolder.Callback, View.OnTouchListener, View.OnKeyListener {

    private static final String TAG = "ContentVideoView";

    /* Do not change these values without updating their counterparts
     * in include/media/mediaplayer.h!
     */
    private static final int MEDIA_NOP = 0; // interface test message
    private static final int MEDIA_PREPARED = 1;
    private static final int MEDIA_PLAYBACK_COMPLETE = 2;
    private static final int MEDIA_BUFFERING_UPDATE = 3;
    private static final int MEDIA_SEEK_COMPLETE = 4;
    private static final int MEDIA_SET_VIDEO_SIZE = 5;
    private static final int MEDIA_ERROR = 100;
    private static final int MEDIA_INFO = 200;

    // Type needs to be kept in sync with surface_texture_peer.h.
    private static final int SET_VIDEO_SURFACE_TEXTURE = 1;

    /** Unspecified media player error.
     * @see android.media.MediaPlayer.OnErrorListener
     */
    public static final int MEDIA_ERROR_UNKNOWN = 0;

    /** Media server died. In this case, the application must release the
     * MediaPlayer object and instantiate a new one.
     */
    public static final int MEDIA_ERROR_SERVER_DIED = 1;

    /** The video is streamed and its container is not valid for progressive
     * playback i.e the video's index (e.g moov atom) is not at the start of the
     * file.
     */
    public static final int MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 2;

    // all possible internal states
    private static final int STATE_ERROR              = -1;
    private static final int STATE_IDLE               = 0;
    private static final int STATE_PREPARING          = 1;
    private static final int STATE_PREPARED           = 2;
    private static final int STATE_PLAYING            = 3;
    private static final int STATE_PAUSED             = 4;
    private static final int STATE_PLAYBACK_COMPLETED = 5;

    private SurfaceHolder mSurfaceHolder = null;
    private int mVideoWidth;
    private int mVideoHeight;
    private int mCurrentBufferPercentage;
    private int mDuration;
    private MediaController mMediaController = null;
    private boolean mCanPause;
    private boolean mCanSeekBack;
    private boolean mCanSeekForward;
    private boolean mHasMediaMetadata = false;

    // Native pointer to C++ ContentVideoView object.
    private int mNativeContentVideoView = 0;

    // webkit should have prepared the media
    private int mCurrentState = STATE_IDLE;

    // Strings for displaying media player errors
    static String mPlaybackErrorText;
    static String mUnknownErrorText;
    static String mErrorButton;
    static String mErrorTitle;

    // This view will contain the video.
    private static VideoSurfaceView sVideoSurfaceView;

    private Surface mSurface = null;

    private static Activity sChromeActivity;
    private static FrameLayout sRootLayout;
    private static ViewGroup sContentContainer;
    private static ViewGroup sControlContainer;

    // There are can be at most 1 fullscreen video
    // TODO(qinmin): will change this once  we move the creation of this class
    // to the host application
    private static ContentVideoView sContentVideoView = null;

    private class VideoSurfaceView extends SurfaceView {

        public VideoSurfaceView(Context context) {
            super(context);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int width = getDefaultSize(mVideoWidth, widthMeasureSpec);
            int height = getDefaultSize(mVideoHeight, heightMeasureSpec);
            if (mVideoWidth > 0 && mVideoHeight > 0) {
                if ( mVideoWidth * height  > width * mVideoHeight ) {
                    height = width * mVideoHeight / mVideoWidth;
                } else if ( mVideoWidth * height  < width * mVideoHeight ) {
                    width = height * mVideoWidth / mVideoHeight;
                }
            }
            setMeasuredDimension(width, height);
        }
    }

    public ContentVideoView(Context context) {
        this(context, 0);
    }

    public ContentVideoView(Context context, int nativeContentVideoView) {
        super(context);
        initResources(context);

        if (nativeContentVideoView == 0) return;
        mNativeContentVideoView = nativeContentVideoView;

        mCurrentBufferPercentage = 0;
        sVideoSurfaceView = new VideoSurfaceView(context);
        mCurrentState = isPlaying() ? STATE_PLAYING : STATE_PAUSED;
    }

    private static void initResources(Context context) {
        if (mPlaybackErrorText != null) return;

        assert AppResource.STRING_MEDIA_PLAYER_MESSAGE_PLAYBACK_ERROR != 0;
        assert AppResource.STRING_MEDIA_PLAYER_MESSAGE_UNKNOWN_ERROR != 0;
        assert AppResource.STRING_MEDIA_PLAYER_ERROR_BUTTON != 0;
        assert AppResource.STRING_MEDIA_PLAYER_ERROR_TITLE != 0;

        mPlaybackErrorText = context.getString(
                AppResource.STRING_MEDIA_PLAYER_MESSAGE_PLAYBACK_ERROR);
        mUnknownErrorText = context.getString(
                AppResource.STRING_MEDIA_PLAYER_MESSAGE_UNKNOWN_ERROR);
        mErrorButton = context.getString(AppResource.STRING_MEDIA_PLAYER_ERROR_BUTTON);
        mErrorTitle = context.getString(AppResource.STRING_MEDIA_PLAYER_ERROR_TITLE);
    }

    void showContentVideoView() {
        this.addView(sVideoSurfaceView,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER));
        sVideoSurfaceView.setOnKeyListener(this);
        sVideoSurfaceView.setOnTouchListener(this);
        sVideoSurfaceView.getHolder().addCallback(this);
        sVideoSurfaceView.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        sVideoSurfaceView.setFocusable(true);
        sVideoSurfaceView.setFocusableInTouchMode(true);
        sVideoSurfaceView.requestFocus();
    }

    @CalledByNative
    public void onMediaPlayerError(int errorType) {
        Log.d(TAG, "OnMediaPlayerError: " + errorType);
        if (mCurrentState == STATE_ERROR || mCurrentState == STATE_PLAYBACK_COMPLETED) {
            return;
        }

        mCurrentState = STATE_ERROR;
        if (mMediaController != null) {
            mMediaController.hide();
        }

        /* Pop up an error dialog so the user knows that
         * something bad has happened. Only try and pop up the dialog
         * if we're attached to a window. When we're going away and no
         * longer have a window, don't bother showing the user an error.
         *
         * TODO(qinmin): We need to review whether this Dialog is OK with
         * the rest of the browser UI elements.
         */
        if (getWindowToken() != null) {
            String message;

            if (errorType == MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK) {
                message = mPlaybackErrorText;
            } else {
                message = mUnknownErrorText;
            }

            new AlertDialog.Builder(getContext())
                .setTitle(mErrorTitle)
                .setMessage(message)
                .setPositiveButton(mErrorButton,
                        new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        /* Inform that the video is over.
                         */
                        onCompletion();
                    }
                })
                .setCancelable(false)
                .show();
        }
    }

    @CalledByNative
    public void onVideoSizeChanged(int width, int height) {
        mVideoWidth = width;
        mVideoHeight = height;
        if (mVideoWidth != 0 && mVideoHeight != 0) {
            sVideoSurfaceView.getHolder().setFixedSize(mVideoWidth, mVideoHeight);
        }
    }

    @CalledByNative
    public void onBufferingUpdate(int percent) {
        mCurrentBufferPercentage = percent;
    }

    @CalledByNative
    public void onPlaybackComplete() {
        onCompletion();
    }

    @CalledByNative
    public void updateMediaMetadata(
            int videoWidth,
            int videoHeight,
            int duration,
            boolean canPause,
            boolean canSeekBack,
            boolean canSeekForward) {
        mDuration = duration;
        mCanPause = canPause;
        mCanSeekBack = canSeekBack;
        mCanSeekForward = canSeekForward;
        mHasMediaMetadata = true;

        if (mMediaController != null) {
            mMediaController.setEnabled(true);
            // If paused , should show the controller for ever.
            if (isPlaying())
                mMediaController.show();
            else
                mMediaController.show(0);
        }

        onVideoSizeChanged(videoWidth, videoHeight);
    }

    public void destroyNativeView() {
        if (mNativeContentVideoView != 0) {
            mNativeContentVideoView = 0;
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        sVideoSurfaceView.setFocusable(true);
        sVideoSurfaceView.setFocusableInTouchMode(true);
        if (isInPlaybackState() && mMediaController != null) {
            if (mMediaController.isShowing()) {
                // ensure the controller will get repositioned later
                mMediaController.hide();
            }
            mMediaController.show();
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mSurfaceHolder = holder;
        openVideo();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mSurfaceHolder = null;
        if (mNativeContentVideoView != 0) {
            nativeDestroyContentVideoView(mNativeContentVideoView, true);
            destroyNativeView();
        }
        if (mMediaController != null) {
            mMediaController.hide();
            mMediaController = null;
        }
    }

    public void setMediaController(MediaController controller) {
        if (mMediaController != null) {
            mMediaController.hide();
        }
        mMediaController = controller;
        attachMediaController();
    }

    private void attachMediaController() {
        if (mMediaController != null) {
            mMediaController.setMediaPlayer(this);
            mMediaController.setAnchorView(sVideoSurfaceView);
            mMediaController.setEnabled(mHasMediaMetadata);
        }
    }

    @CalledByNative
    public void openVideo() {
        if (mSurfaceHolder != null) {
            if (mNativeContentVideoView != 0) {
                nativeUpdateMediaMetadata(mNativeContentVideoView);
            }
            mCurrentBufferPercentage = 0;
            if (mNativeContentVideoView != 0) {
                int renderHandle = nativeGetRenderHandle(mNativeContentVideoView);
                if (renderHandle == 0) {
                    nativeSetSurface(mNativeContentVideoView,
                            mSurfaceHolder.getSurface(),
                            nativeGetRouteId(mNativeContentVideoView),
                            nativeGetPlayerId(mNativeContentVideoView));
                    return;
                }
                ISandboxedProcessService service =
                    SandboxedProcessLauncher.getSandboxedService(renderHandle);
                if (service == null) {
                    Log.e(TAG, "Unable to get SandboxedProcessService from pid.");
                    return;
                }
                try {
                    service.setSurface(
                            SET_VIDEO_SURFACE_TEXTURE,
                            mSurfaceHolder.getSurface(),
                            nativeGetRouteId(mNativeContentVideoView),
                            nativeGetPlayerId(mNativeContentVideoView));
                } catch (RemoteException e) {
                    Log.e(TAG, "Unable to call setSurfaceTexture: " + e);
                    return;
                }
            }
            requestLayout();
            invalidate();
            setMediaController(new MediaController(getChromeActivity()));

            if (mMediaController != null) {
                mMediaController.show();
            }
        }
    }

    private void onCompletion() {
        mCurrentState = STATE_PLAYBACK_COMPLETED;
        if (mMediaController != null) {
            mMediaController.hide();
        }
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        if (isInPlaybackState() && mMediaController != null &&
                event.getAction() == MotionEvent.ACTION_DOWN) {
            toggleMediaControlsVisiblity();
        }
        return true;
    }

    @Override
    public boolean onTrackballEvent(MotionEvent ev) {
        if (isInPlaybackState() && mMediaController != null) {
            toggleMediaControlsVisiblity();
        }
        return false;
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        boolean isKeyCodeSupported = keyCode != KeyEvent.KEYCODE_BACK &&
                                     keyCode != KeyEvent.KEYCODE_VOLUME_UP &&
                                     keyCode != KeyEvent.KEYCODE_VOLUME_DOWN &&
                                     keyCode != KeyEvent.KEYCODE_VOLUME_MUTE &&
                                     keyCode != KeyEvent.KEYCODE_CALL &&
                                     keyCode != KeyEvent.KEYCODE_MENU &&
                                     keyCode != KeyEvent.KEYCODE_SEARCH &&
                                     keyCode != KeyEvent.KEYCODE_ENDCALL;
        if (isInPlaybackState() && isKeyCodeSupported && mMediaController != null) {
            if (keyCode == KeyEvent.KEYCODE_HEADSETHOOK ||
                    keyCode == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE) {
                if (isPlaying()) {
                    pause();
                    mMediaController.show();
                } else {
                    start();
                    mMediaController.hide();
                }
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_MEDIA_PLAY) {
                if (!isPlaying()) {
                    start();
                    mMediaController.hide();
                }
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_MEDIA_STOP
                    || keyCode == KeyEvent.KEYCODE_MEDIA_PAUSE) {
                if (isPlaying()) {
                    pause();
                    mMediaController.show();
                }
                return true;
            } else {
                toggleMediaControlsVisiblity();
            }
        } else if (keyCode == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
            if (mNativeContentVideoView != 0) {
                nativeDestroyContentVideoView(mNativeContentVideoView, false);
                destroyNativeView();
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_SEARCH) {
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    private void toggleMediaControlsVisiblity() {
        if (mMediaController.isShowing()) {
            mMediaController.hide();
        } else {
            mMediaController.show();
        }
    }

    private boolean isInPlaybackState() {
        return (mCurrentState != STATE_ERROR &&
                mCurrentState != STATE_IDLE &&
                mCurrentState != STATE_PREPARING);
    }

    public void start() {
        if (isInPlaybackState()) {
            if (mNativeContentVideoView != 0) {
                nativePlay(mNativeContentVideoView);
            }
            mCurrentState = STATE_PLAYING;
        }
    }

    public void pause() {
        if (isInPlaybackState()) {
            if (isPlaying()) {
                if (mNativeContentVideoView != 0) {
                    nativePause(mNativeContentVideoView);
                }
                mCurrentState = STATE_PAUSED;
            }
        }
    }

    // cache duration as mDuration for faster access
    public int getDuration() {
        if (isInPlaybackState()) {
            if (mDuration > 0) {
                return mDuration;
            }
            if (mNativeContentVideoView != 0) {
                mDuration = nativeGetDurationInMilliSeconds(mNativeContentVideoView);
            } else {
                mDuration = 0;
            }
            return mDuration;
        }
        mDuration = -1;
        return mDuration;
    }

    public int getCurrentPosition() {
        if (isInPlaybackState() && mNativeContentVideoView != 0) {
            return nativeGetCurrentPosition(mNativeContentVideoView);
        }
        return 0;
    }

    public void seekTo(int msec) {
        if (mNativeContentVideoView != 0) {
            nativeSeekTo(mNativeContentVideoView, msec);
        }
    }

    public boolean isPlaying() {
        return mNativeContentVideoView != 0 && nativeIsPlaying(mNativeContentVideoView);
    }

    public int getBufferPercentage() {
        return mCurrentBufferPercentage;
    }
    public boolean canPause() {
        return mCanPause;
    }

    public boolean canSeekBackward() {
        return mCanSeekBack;
    }

    public boolean canSeekForward() {
        return mCanSeekForward;
    }

    @CalledByNative
    public static ContentVideoView createContentVideoView(int nativeContentVideoView) {
        if (getChromeActivity() != null) {
            ContentVideoView videoView = new ContentVideoView(getChromeActivity(),
                    nativeContentVideoView);
            if (sContentVideoView != null) {
                return videoView;
            }

            sContentVideoView = videoView;

            sContentContainer.setVisibility(View.GONE);
            sControlContainer.setVisibility(View.GONE);

            sChromeActivity.getWindow().setFlags(
                    WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    WindowManager.LayoutParams.FLAG_FULLSCREEN);

            sChromeActivity.getWindow().addContentView(videoView,
                    new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            Gravity.CENTER));

            videoView.setBackgroundColor(Color.BLACK);
            sContentVideoView.showContentVideoView();
            videoView.setVisibility(View.VISIBLE);
            return videoView;
        }
        return null;
    }

    public static Activity getChromeActivity() {
        return sChromeActivity;
    }

    public static void showFullScreen(ContentVideoView fullscreenView) {

    }

    @CalledByNative
    public static void destroyContentVideoView() {
        sChromeActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        if (sContentVideoView != null) {
            sContentVideoView.removeView(sVideoSurfaceView);
            sVideoSurfaceView = null;
            sContentVideoView.setVisibility(View.GONE);
            sRootLayout.removeView(sContentVideoView);
        }

        sContentContainer.setVisibility(View.VISIBLE);
        sControlContainer.setVisibility(View.VISIBLE);

        sContentVideoView = null;
    }

    public static ContentVideoView getContentVideoView() {
        return sContentVideoView;
    }

    public static void registerChromeActivity(Activity activity, FrameLayout rootLayout,
            ViewGroup controlContainer, ViewGroup contentContainer) {
        sChromeActivity = activity;
        sRootLayout = rootLayout;
        sControlContainer = controlContainer;
        sContentContainer = contentContainer;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
            destroyContentVideoView();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    private native void nativeDestroyContentVideoView(int nativeContentVideoView,
            boolean relaseMediaPlayer);
    private native int nativeGetCurrentPosition(int nativeContentVideoView);
    private native int nativeGetDurationInMilliSeconds(int nativeContentVideoView);
    private native void nativeUpdateMediaMetadata(int nativeContentVideoView);
    private native int nativeGetVideoWidth(int nativeContentVideoView);
    private native int nativeGetVideoHeight(int nativeContentVideoView);
    private native int nativeGetPlayerId(int nativeContentVideoView);
    private native int nativeGetRouteId(int nativeContentVideoView);
    private native int nativeGetRenderHandle(int nativeContentVideoView);
    private native boolean nativeIsPlaying(int nativeContentVideoView);
    private native void nativePause(int nativeContentVideoView);
    private native void nativePlay(int nativeContentVideoView);
    private native void nativeSeekTo(int nativeContentVideoView, int msec);
    private native void nativeSetSurface(int nativeContentVideoView,
            Surface surface, int routeId, int playerId);
}
