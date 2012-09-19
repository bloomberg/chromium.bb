// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaPlayer;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

// This class implements all the listener interface for android mediaplayer.
// Callbacks will be sent to the native class for processing.
@JNINamespace("media")
class MediaPlayerListener implements MediaPlayer.OnPreparedListener,
                                     MediaPlayer.OnCompletionListener,
                                     MediaPlayer.OnBufferingUpdateListener,
                                     MediaPlayer.OnSeekCompleteListener,
                                     MediaPlayer.OnVideoSizeChangedListener,
                                     MediaPlayer.OnErrorListener {
    // These values are mirrored as enums in media/base/android/media_player_bridge.h.
    // Please ensure they stay in sync.
    private static final int MEDIA_ERROR_UNKNOWN = 0;
    private static final int MEDIA_ERROR_SERVER_DIED = 1;
    private static final int MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 2;
    private static final int MEDIA_ERROR_INVALID_CODE = 3;

    // Used to determine the class instance to dispatch the native call to.
    private int mNativeMediaPlayerListener = 0;

    private MediaPlayerListener(int nativeMediaPlayerListener) {
        mNativeMediaPlayerListener = nativeMediaPlayerListener;
    }

    @Override
    public boolean onError(MediaPlayer mp, int what, int extra) {
        int errorType;
        switch (what) {
            case MediaPlayer.MEDIA_ERROR_UNKNOWN:
                errorType = MEDIA_ERROR_UNKNOWN;
                break;
            case MediaPlayer.MEDIA_ERROR_SERVER_DIED:
                errorType = MEDIA_ERROR_SERVER_DIED;
                break;
            case MediaPlayer.MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK:
                errorType = MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK;
                break;
            default:
                // There are some undocumented error codes for android media player.
                // For example, when surfaceTexture got deleted before we setVideoSuface
                // to NULL, mediaplayer will report error -38. These errors should be ignored
                // and not be treated as an error to webkit.
                errorType = MEDIA_ERROR_INVALID_CODE;
                break;
        }
        nativeOnMediaError(mNativeMediaPlayerListener, errorType);
        return true;
    }

    @Override
    public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
        nativeOnVideoSizeChanged(mNativeMediaPlayerListener, width, height);
    }

    @Override
    public void onSeekComplete(MediaPlayer mp) {
        nativeOnSeekComplete(mNativeMediaPlayerListener);
    }

    @Override
    public void onBufferingUpdate(MediaPlayer mp, int percent) {
        nativeOnBufferingUpdate(mNativeMediaPlayerListener, percent);
    }

    @Override
    public void onCompletion(MediaPlayer mp) {
        nativeOnPlaybackComplete(mNativeMediaPlayerListener);
    }

    @Override
    public void onPrepared(MediaPlayer mp) {
        nativeOnMediaPrepared(mNativeMediaPlayerListener);
    }

    @CalledByNative
    private static MediaPlayerListener create(int nativeMediaPlayerListener) {
        return new MediaPlayerListener(nativeMediaPlayerListener);
    }

    /**
     * See media/base/android/media_player_listener.cc for all the following functions.
     */
    private native void nativeOnMediaError(
            int nativeMediaPlayerListener,
            int errorType);

    private native void nativeOnVideoSizeChanged(
            int nativeMediaPlayerListener,
            int width, int height);

    private native void nativeOnBufferingUpdate(
            int nativeMediaPlayerListener,
            int percent);

    private native void nativeOnMediaPrepared(int nativeMediaPlayerListener);

    private native void nativeOnPlaybackComplete(int nativeMediaPlayerListener);

    private native void nativeOnSeekComplete(int nativeMediaPlayerListener);
}
