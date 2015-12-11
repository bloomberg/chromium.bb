// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.content.Context;
import android.media.MediaPlayer;
import android.os.Handler;
import android.os.SystemClock;
import android.support.v7.media.MediaItemStatus;
import android.support.v7.media.MediaRouter.RouteInfo;
import android.util.Log;

import java.io.IOException;

/**
 * Handles playback of a single media item using MediaPlayer.
 */
public class DummyPlayer implements MediaPlayer.OnPreparedListener,
        MediaPlayer.OnCompletionListener, MediaPlayer.OnErrorListener,
        MediaPlayer.OnSeekCompleteListener {
    private static final String TAG = "CastDummyPlayer";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private static final int STATE_IDLE = 0;
    private static final int STATE_PLAY_PENDING = 1;
    private static final int STATE_READY = 2;
    private static final int STATE_PLAYING = 3;
    private static final int STATE_PAUSED = 4;

    private final Context mContext;
    private final Handler mHandler = new Handler();
    private MediaPlayer mMediaPlayer;
    private int mState = STATE_IDLE;
    private int mSeekToPos;
    private Callback mCallback;

    /**
     * Callback interface for the session manager
     */
    public static interface Callback {
        void onError();
        void onCompletion();
        void onSeekComplete();
        void onPrepared();
    }

    public DummyPlayer(Context context) {
        // reset media player
        reset();
        mContext = context;
    }

    public void connect(RouteInfo route) {
        if (DEBUG) Log.d(TAG, "connecting to: " + route);
    }

    public void release() {
        if (DEBUG) Log.d(TAG, "releasing");
        // release media player
        if (mMediaPlayer != null) {
            mMediaPlayer.stop();
            mMediaPlayer.release();
            mMediaPlayer = null;
        }
    }

    // Player
    public void play(final MediaItem item) {
        if (DEBUG) Log.d(TAG, "play: item=" + item);
        reset();
        mSeekToPos = (int) item.getPosition();
        try {
            mMediaPlayer.setDataSource(mContext, item.getUri());
            mMediaPlayer.prepare();
        } catch (IllegalStateException e) {
            Log.e(TAG, "MediaPlayer throws IllegalStateException, uri=" + item.getUri());
        } catch (IOException e) {
            Log.e(TAG, "MediaPlayer throws IOException, uri=" + item.getUri());
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "MediaPlayer throws IllegalArgumentException, uri=" + item.getUri());
        } catch (SecurityException e) {
            Log.e(TAG, "MediaPlayer throws SecurityException, uri=" + item.getUri());
        }
        if (item.getState() == MediaItemStatus.PLAYBACK_STATE_PLAYING) {
            resume();
        } else {
            pause();
        }
    }

    public void seek(final MediaItem item) {
        if (DEBUG) Log.d(TAG, "seek: item=" + item);
        int pos = (int) item.getPosition();
        if (mState == STATE_PLAYING || mState == STATE_PAUSED) {
            mMediaPlayer.seekTo(pos);
            mSeekToPos = pos;
        } else if (mState == STATE_IDLE || mState == STATE_PLAY_PENDING) {
            // Seek before onPrepared() arrives,
            // need to performed delayed seek in onPrepared()
            mSeekToPos = pos;
        }
    }

    public void getStatus(final MediaItem item, final boolean update) {
        if (mState == STATE_PLAYING || mState == STATE_PAUSED) {
            // use mSeekToPos if we're currently seeking (mSeekToPos is reset
            // when seeking is completed)
            item.setDuration(mMediaPlayer.getDuration());
            item.setPosition(mSeekToPos > 0 ? mSeekToPos : mMediaPlayer.getCurrentPosition());
            item.setTimestamp(SystemClock.uptimeMillis());
        }
    }

    public void pause() {
        if (DEBUG) Log.d(TAG, "pause");
        if (mState == STATE_PLAYING) {
            mMediaPlayer.pause();
            mState = STATE_PAUSED;
        }
    }

    public void resume() {
        if (DEBUG) Log.d(TAG, "resume");
        if (mState == STATE_READY || mState == STATE_PAUSED) {
            mMediaPlayer.start();
            mState = STATE_PLAYING;
        } else if (mState == STATE_IDLE) {
            mState = STATE_PLAY_PENDING;
        }
    }

    public void stop() {
        if (DEBUG) Log.d(TAG, "stop");
        if (mState == STATE_PLAYING || mState == STATE_PAUSED) {
            mMediaPlayer.stop();
            mState = STATE_IDLE;
        }
    }

    // MediaPlayer Listeners
    @Override
    public void onPrepared(MediaPlayer mp) {
        if (DEBUG) Log.d(TAG, "onPrepared");
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mState == STATE_IDLE) {
                    mState = STATE_READY;
                } else if (mState == STATE_PLAY_PENDING) {
                    mState = STATE_PLAYING;
                    if (mSeekToPos > 0) {
                        if (DEBUG) Log.d(TAG, "seek to initial pos: " + mSeekToPos);
                        mMediaPlayer.seekTo(mSeekToPos);
                    }
                    mMediaPlayer.start();
                }
                if (mCallback != null) {
                    mCallback.onPrepared();
                }
            }
        });
    }

    @Override
    public void onCompletion(MediaPlayer mp) {
        if (DEBUG) Log.d(TAG, "onCompletion");
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mCallback != null) {
                    mCallback.onCompletion();
                }
            }
        });
    }

    @Override
    public boolean onError(MediaPlayer mp, int what, int extra) {
        if (DEBUG) Log.d(TAG, "onError");
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mCallback != null) {
                    mCallback.onError();
                }
            }
        });
        // return true so that onCompletion is not called
        return true;
    }

    @Override
    public void onSeekComplete(MediaPlayer mp) {
        if (DEBUG) Log.d(TAG, "onSeekComplete");
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mSeekToPos = 0;
                if (mCallback != null) {
                    mCallback.onSeekComplete();
                }
            }
        });
    }

    protected Context getContext() {
        return mContext;
    }

    private void reset() {
        if (mMediaPlayer != null) {
            mMediaPlayer.stop();
            mMediaPlayer.release();
            mMediaPlayer = null;
        }
        mMediaPlayer = new MediaPlayer();
        mMediaPlayer.setOnPreparedListener(this);
        mMediaPlayer.setOnCompletionListener(this);
        mMediaPlayer.setOnErrorListener(this);
        mMediaPlayer.setOnSeekCompleteListener(this);
        mState = STATE_IDLE;
        mSeekToPos = 0;
    }

    public void setCallback(Callback callback) {
        mCallback = callback;
    }

    public static DummyPlayer create(Context context, RouteInfo route) {
        DummyPlayer player = new DummyPlayer(context);
        player.connect(route);
        return player;
    }
}
