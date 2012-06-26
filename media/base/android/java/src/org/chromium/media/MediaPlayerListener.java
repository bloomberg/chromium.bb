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
                                     MediaPlayer.OnErrorListener,
                                     MediaPlayer.OnInfoListener {
    // These values are mirrored as enums in media/base/android/media_player_bridge.h.
    // Please ensure they stay in sync.
    private static final int MEDIA_ERROR_UNKNOWN = 0;
    private static final int MEDIA_ERROR_SERVER_DIED = 1;
    private static final int MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 2;
    private static final int MEDIA_ERROR_INVALID_CODE = 3;

    private static final int MEDIA_INFO_UNKNOWN = 0;
    private static final int MEDIA_INFO_VIDEO_TRACK_LAGGING = 1;
    private static final int MEDIA_INFO_BUFFERING_START = 2;
    private static final int MEDIA_INFO_BUFFERING_END = 3;
    private static final int MEDIA_INFO_BAD_INTERLEAVING = 4;
    private static final int MEDIA_INFO_NOT_SEEKABLE = 5;
    private static final int MEDIA_INFO_METADATA_UPDATE = 6;

    // Used to determine the class instance to dispatch the native call to.
    private int mNativeMediaPlayerBridge = 0;

    private MediaPlayerListener(int nativeMediaPlayerBridge) {
        mNativeMediaPlayerBridge = nativeMediaPlayerBridge;
    }

    @Override
    public boolean onInfo(MediaPlayer mp, int what, int extra) {
        int infoType;
        switch (what) {
            case MediaPlayer.MEDIA_INFO_UNKNOWN:
                infoType = MEDIA_INFO_UNKNOWN;
                break;
            case MediaPlayer.MEDIA_INFO_VIDEO_TRACK_LAGGING:
                infoType = MEDIA_INFO_VIDEO_TRACK_LAGGING;
                break;
            case MediaPlayer.MEDIA_INFO_BUFFERING_START:
                infoType = MEDIA_INFO_BUFFERING_START;
                break;
            case MediaPlayer.MEDIA_INFO_BUFFERING_END:
                infoType = MEDIA_INFO_BUFFERING_END;
                break;
            case MediaPlayer.MEDIA_INFO_BAD_INTERLEAVING:
                infoType = MEDIA_INFO_BAD_INTERLEAVING;
                break;
            case MediaPlayer.MEDIA_INFO_NOT_SEEKABLE:
                infoType = MEDIA_INFO_NOT_SEEKABLE;
                break;
            case MediaPlayer.MEDIA_INFO_METADATA_UPDATE:
                infoType = MEDIA_INFO_METADATA_UPDATE;
                break;
            default:
                infoType = MEDIA_INFO_UNKNOWN;
                break;
        }
        nativeOnMediaInfo(mNativeMediaPlayerBridge, infoType);
        return true;
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
        nativeOnMediaError(mNativeMediaPlayerBridge, errorType);
        return true;
    }

    @Override
    public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
        nativeOnVideoSizeChanged(mNativeMediaPlayerBridge, width, height);
    }

    @Override
    public void onSeekComplete(MediaPlayer mp) {
        nativeOnSeekComplete(mNativeMediaPlayerBridge);
    }

    @Override
    public void onBufferingUpdate(MediaPlayer mp, int percent) {
        nativeOnBufferingUpdate(mNativeMediaPlayerBridge, percent);
    }

    @Override
    public void onCompletion(MediaPlayer mp) {
        nativeOnPlaybackComplete(mNativeMediaPlayerBridge);
    }

    @Override
    public void onPrepared(MediaPlayer mp) {
        nativeOnMediaPrepared(mNativeMediaPlayerBridge);
    }

    @CalledByNative
    private static MediaPlayerListener create(int nativeMediaPlayerBridge) {
        return new MediaPlayerListener(nativeMediaPlayerBridge);
    }

    /**
     * See media/base/android/media_player_bridge.cc for all the following functions.
     */
    private native void nativeOnMediaError(
            int nativeMediaPlayerBridge,
            int errorType);

    private native void nativeOnMediaInfo(
            int nativeMediaPlayerBridge,
            int infoType);

    private native void nativeOnVideoSizeChanged(
            int nativeMediaPlayerBridge,
            int width, int height);

    private native void nativeOnBufferingUpdate(
            int nativeMediaPlayerBridge,
            int percent);

    private native void nativeOnMediaPrepared(int nativeMediaPlayerBridge);

    private native void nativeOnPlaybackComplete(int nativeMediaPlayerBridge);

    private native void nativeOnSeekComplete(int nativeMediaPlayerBridge);
}
