// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.cma.backend.android;

import android.annotation.TargetApi;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.os.Build;
import android.os.SystemClock;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Implements an audio sink object using Android's AudioTrack module to
 * playback audio samples.
 * It assumes the following fixed configuration parameters:
 *   - 2-channel audio
 *   - PCM audio format (i.e., no encoded data like mp3)
 *   - samples are 4-byte floats, interleaved channels ("LRLRLRLRLR").
 * The only configurable audio parameter is the sample rate (typically 44.1 or
 * 48 KHz).
 *
 * PCM data is shared through the JNI using memory-mapped ByteBuffer objects.
 * The AudioTrack.write() function is called in BLOCKING mode. That means when
 * in PLAYING state the call will block until all data has been accepted
 * (queued) by the Audio server. The native side feeding data in through the
 * JNI is assumed to be running in a dedicated thread to avoid hanging other
 * parts of the application.
 *
 * No locking of instance data is done as it is assumed to be called from a
 * single thread in native code.
 *
 */
@JNINamespace("chromecast::media")
@TargetApi(Build.VERSION_CODES.N)
class AudioSinkAudioTrackImpl {
    private static final String TAG = "AudiotrackImpl";
    private static final int DEBUG_LEVEL = 0;

    // hardcoded AudioTrack config parameters
    private static final int STREAM_TYPE = AudioManager.STREAM_MUSIC;
    private static final int CHANNEL_CONFIG = AudioFormat.CHANNEL_OUT_STEREO;
    private static final int AUDIO_FORMAT = AudioFormat.ENCODING_PCM_FLOAT;
    private static final int AUDIO_MODE = AudioTrack.MODE_STREAM;
    private static final int BYTES_PER_FRAME = 2 * 4; // 2 channels, float (4-bytes)

    private static final long NO_TIMESTAMP = Long.MIN_VALUE;

    private static final long SEC_IN_NSEC = 1000000000L;
    private static final long TIMESTAMP_UPDATE_PERIOD = 3 * SEC_IN_NSEC;
    private static final long UNDERRUN_LOG_THROTTLE_PERIOD = SEC_IN_NSEC;

    private final long mNativeAudioSinkAudioTrackImpl;

    private boolean mIsInitialized;

    // Dynamic AudioTrack config parameter.
    private int mSampleRateInHz;

    private AudioTrack mAudioTrack;

    // Timestamping logic for RenderingDelay calculations.
    private AudioTimestamp mLastPlayoutTStamp;
    private long mLastTimestampUpdateNsec; // Last time we updated the timestamp.
    private boolean mTriggerTimestampUpdateNow; // Set to true to trigger an early update.

    private int mLastUnderrunCount;
    private long mLastUnderrunLogNsec;

    // Statistics
    private long mTotalFramesWritten;

    // Sample Rate calculator
    private long mSRWindowStartTimeNsec;
    private long mSRWindowFramesWritten;

    // Buffers shared between native and java space to move data across the JNI.
    // We use a direct buffers so that the native class can have access to
    // the underlying memory address. This avoids the need to copy from a
    // jbyteArray to native memory. More discussion of this here:
    // http://developer.android.com/training/articles/perf-jni.html
    private ByteBuffer mPcmBuffer; // PCM audio data (native->java)
    private ByteBuffer mRenderingDelayBuffer; // RenderingDelay return value
                                              // (java->native)

    /** Construction */
    @CalledByNative
    private static AudioSinkAudioTrackImpl createAudioSinkAudioTrackImpl(
            long nativeAudioSinkAudioTrackImpl) {
        return new AudioSinkAudioTrackImpl(nativeAudioSinkAudioTrackImpl);
    }

    private AudioSinkAudioTrackImpl(long nativeAudioSinkAudioTrackImpl) {
        mNativeAudioSinkAudioTrackImpl = nativeAudioSinkAudioTrackImpl;
        mIsInitialized = false;
        mLastTimestampUpdateNsec = NO_TIMESTAMP;
        mTriggerTimestampUpdateNow = false;
        mLastUnderrunCount = 0;
        mLastUnderrunLogNsec = NO_TIMESTAMP;
        mTotalFramesWritten = 0;
    }

    /**
     * Initializes the instance by creating the AudioTrack object and allocating
     * the shared memory buffers.
     */
    @CalledByNative
    private void init(int sampleRateInHz, int bytesPerBuffer) {
        Log.i(TAG,
                "Init:"
                        + " sampleRateInHz=" + sampleRateInHz
                        + " bytesPerBuffer=" + bytesPerBuffer);

        if (mIsInitialized) {
            Log.w(TAG, "Init: already initialized.");
            return;
        }

        if (sampleRateInHz <= 0) {
            Log.e(TAG, "Invalid sampleRateInHz=" + sampleRateInHz + " given!");
            return;
        }
        mSampleRateInHz = sampleRateInHz;

        // TODO(ckuiper): ALSA code uses a 90ms buffer size, we should do something
        // similar.
        int bufferSizeInBytes =
                5 * AudioTrack.getMinBufferSize(mSampleRateInHz, CHANNEL_CONFIG, AUDIO_FORMAT);
        Log.i(TAG, "Init: using an AudioTrack buffer_size=" + bufferSizeInBytes);

        mAudioTrack = new AudioTrack(STREAM_TYPE, mSampleRateInHz, CHANNEL_CONFIG, AUDIO_FORMAT,
                bufferSizeInBytes, AUDIO_MODE);
        mLastPlayoutTStamp = new AudioTimestamp();

        // Allocated shared buffers.
        mPcmBuffer = ByteBuffer.allocateDirect(bytesPerBuffer);
        mPcmBuffer.order(ByteOrder.nativeOrder());

        mRenderingDelayBuffer = ByteBuffer.allocateDirect(2 * 8); // 2 long
        mRenderingDelayBuffer.order(ByteOrder.nativeOrder());

        nativeCacheDirectBufferAddress(
                mNativeAudioSinkAudioTrackImpl, mPcmBuffer, mRenderingDelayBuffer);

        // Put into PLAYING state so it starts playing right when data is fed in.
        play();

        mIsInitialized = true;
    }

    @CalledByNative
    private void play() {
        Log.i(TAG, "Start playback");
        mSRWindowFramesWritten = 0;
        mAudioTrack.play();
        mTriggerTimestampUpdateNow = true; // Get a fresh timestamp asap.
    }

    @CalledByNative
    private void pause() {
        Log.i(TAG, "Pausing playback");
        mAudioTrack.pause();
    }

    @CalledByNative
    private void setVolume(float volume) {
        Log.i(TAG, "Setting volume to " + volume);
        int ret = mAudioTrack.setVolume(volume);
        if (ret != AudioTrack.SUCCESS) {
            Log.e(TAG, "Cannot set volume: ret=" + ret);
        }
    }

    private boolean isPlaying() {
        return mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING;
    }

    private boolean isPaused() {
        return mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PAUSED;
    }

    @CalledByNative
    /** Closes the instance by stopping playback and releasing the AudioTrack
     * object. */
    private void close() {
        Log.i(TAG, "Close AudioSinkAudioTrackImpl!");
        if (!mIsInitialized) {
            Log.w(TAG, "Close: not initialized.");
            return;
        }
        mAudioTrack.stop();
        mAudioTrack.release();
        mIsInitialized = false;
    }

    private String getPlayStateString() {
        switch (mAudioTrack.getPlayState()) {
            case AudioTrack.PLAYSTATE_PAUSED:
                return "PAUSED";
            case AudioTrack.PLAYSTATE_STOPPED:
                return "STOPPED";
            case AudioTrack.PLAYSTATE_PLAYING:
                return "PLAYING";
            default:
                return "UNKNOWN";
        }
    }

    int getUnderrunCount() {
        return mAudioTrack.getUnderrunCount();
    }

    /** Writes the PCM data of the given size into the AudioTrack object. The
     * PCM data is provided through the memory-mapped ByteBuffer.
     *
     * Returns the number of bytes written into the AudioTrack object, -1 for
     * error.
     */
    @CalledByNative
    private int writePcm(int sizeInBytes) {
        if (DEBUG_LEVEL >= 3) {
            Log.i(TAG,
                    "Writing new PCM data:"
                            + " sizeInBytes=" + sizeInBytes + " state=" + getPlayStateString()
                            + " underruns=" + mLastUnderrunCount);
        }

        if (!mIsInitialized) {
            Log.e(TAG, "not initialized!");
            return -1;
        }

        // Setup the PCM ByteBuffer correctly.
        mPcmBuffer.limit(sizeInBytes);
        mPcmBuffer.position(0);

        // Feed into AudioTrack - blocking call.
        long beforeMsecs = SystemClock.elapsedRealtime();
        int bytesWritten = mAudioTrack.write(mPcmBuffer, sizeInBytes, AudioTrack.WRITE_BLOCKING);

        int framesWritten = bytesWritten / BYTES_PER_FRAME;
        mTotalFramesWritten += framesWritten;

        if (DEBUG_LEVEL >= 3) {
            Log.i(TAG,
                    "  wrote " + bytesWritten + "/" + sizeInBytes
                            + " total_bytes_written=" + (mTotalFramesWritten * BYTES_PER_FRAME)
                            + " took:" + (SystemClock.elapsedRealtime() - beforeMsecs) + "ms");
        }

        if (bytesWritten <= 0 || isPaused()) {
            // Either hit an error or we are in PAUSED state, in which case the
            // write() is non-blocking. If not all data was written, we will come
            // back here once we transition back into PLAYING state.
            return bytesWritten;
        }

        updateSampleRateMeasure(framesWritten);

        updateRenderingDelay();

        // TODO(ckuiper): Log key statistics (SR and underruns, e.g.) in regular intervals

        return bytesWritten;
    }

    /** Returns the elapsed time from the given start_time until now, in nsec. */
    private long elapsedNsec(long startTimeNsec) {
        return System.nanoTime() - startTimeNsec;
    }

    private void updateSampleRateMeasure(long framesWritten) {
        if (mSRWindowFramesWritten == 0) {
            // Start new window.
            mSRWindowStartTimeNsec = System.nanoTime();
            mSRWindowFramesWritten = framesWritten;
            return;
        }
        mSRWindowFramesWritten += framesWritten;
        long periodNsec = elapsedNsec(mSRWindowStartTimeNsec);
        float sampleRate = 1e9f * mSRWindowFramesWritten / periodNsec;
        if (DEBUG_LEVEL >= 3) {
            Log.i(TAG,
                    "SR=" + mSRWindowFramesWritten + "/" + (periodNsec / 1000)
                            + "us = " + sampleRate);
        }
    }

    private void updateRenderingDelay() {
        updateTimestamp();
        if (mLastTimestampUpdateNsec == NO_TIMESTAMP) {
            // No timestamp available yet, just put dummy values and return.
            mRenderingDelayBuffer.putLong(0, 0);
            mRenderingDelayBuffer.putLong(8, NO_TIMESTAMP);
            return;
        }

        // Interpolate to get proper Rendering delay.
        long delta_frames = mTotalFramesWritten - mLastPlayoutTStamp.framePosition;
        long delta_nsecs = 1000000000 * delta_frames / mSampleRateInHz;
        long playout_time_nsecs = mLastPlayoutTStamp.nanoTime + delta_nsecs;
        long now_nsecs = System.nanoTime();
        long delay_nsecs = playout_time_nsecs - now_nsecs;

        // Populate RenderingDelay return value for native land.
        mRenderingDelayBuffer.putLong(0, delay_nsecs / 1000);
        mRenderingDelayBuffer.putLong(8, now_nsecs / 1000);

        if (DEBUG_LEVEL >= 3) {
            Log.i(TAG,
                    "RenderingDelay: "
                            + " df=" + delta_frames + " dt=" + (delta_nsecs / 1000)
                            + " delay=" + (delay_nsecs / 1000) + " play=" + (now_nsecs / 1000));
        }
    }

    /** Gets a new timestamp from AudioTrack. For performance reasons we only
     * read a new timestamp in certain intervals. */
    private void updateTimestamp() {
        int underruns = getUnderrunCount();
        if (underruns != mLastUnderrunCount) {
            logUnderruns(underruns);
            mLastTimestampUpdateNsec = NO_TIMESTAMP;
            mLastUnderrunCount = underruns;
        }
        if (!mTriggerTimestampUpdateNow && mLastTimestampUpdateNsec != NO_TIMESTAMP
                && elapsedNsec(mLastTimestampUpdateNsec) <= TIMESTAMP_UPDATE_PERIOD) {
            // not time for an update yet
            return;
        }

        if (mAudioTrack.getTimestamp(mLastPlayoutTStamp)) {
            // Got a new value.
            if (DEBUG_LEVEL >= 1) {
                Log.i(TAG,
                        "New AudioTrack timestamp:"
                                + " pos=" + mLastPlayoutTStamp.framePosition
                                + " ts=" + mLastPlayoutTStamp.nanoTime / 1000 + "us");
            }
            mLastTimestampUpdateNsec = System.nanoTime();
            mTriggerTimestampUpdateNow = false;
        }
    }

    /** Logs underruns in a throttled manner. */
    private void logUnderruns(int newUnderruns) {
        if (DEBUG_LEVEL >= 1
                || (mLastUnderrunLogNsec == NO_TIMESTAMP
                           || elapsedNsec(mLastUnderrunLogNsec) > UNDERRUN_LOG_THROTTLE_PERIOD)) {
            Log.i(TAG,
                    "Underrun detected (" + mLastUnderrunCount + "->" + newUnderruns
                            + ")! Resetting rendering delay logic.");
            mLastUnderrunLogNsec = System.nanoTime();
        }
    }

    //
    // JNI functions in native land.
    //
    private native void nativeCacheDirectBufferAddress(long nativeAudioSinkAndroidAudioTrackImpl,
            ByteBuffer mPcmBuffer, ByteBuffer mRenderingDelayBuffer);
}
