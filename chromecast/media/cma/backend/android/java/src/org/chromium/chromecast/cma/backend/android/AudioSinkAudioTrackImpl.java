// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.cma.backend.android;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTimestamp;
import android.media.AudioTrack;
import android.os.Build;
import android.os.SystemClock;
import android.util.SparseIntArray;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chromecast.media.AudioContentType;

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

    // Mapping from Android's stream_type to Cast's AudioContentType (used for callback).
    private static final SparseIntArray CAST_TYPE_TO_ANDROID_USAGE_TYPE_MAP = new SparseIntArray(
            3) {
        {
            append(AudioContentType.MEDIA, AudioAttributes.USAGE_MEDIA);
            append(AudioContentType.ALARM, AudioAttributes.USAGE_ALARM);
            append(AudioContentType.COMMUNICATION, AudioAttributes.USAGE_ASSISTANCE_SONIFICATION);
        }
    };

    private static final SparseIntArray CAST_TYPE_TO_ANDROID_CONTENT_TYPE_MAP = new SparseIntArray(
            3) {
        {
            append(AudioContentType.MEDIA, AudioAttributes.CONTENT_TYPE_MUSIC);
            // Note: ALARM uses the same as COMMUNICATON.
            append(AudioContentType.ALARM, AudioAttributes.CONTENT_TYPE_SONIFICATION);
            append(AudioContentType.COMMUNICATION, AudioAttributes.CONTENT_TYPE_SONIFICATION);
        }
    };

    // Hardcoded AudioTrack config parameters.
    private static final int CHANNEL_CONFIG = AudioFormat.CHANNEL_OUT_STEREO;
    private static final int AUDIO_FORMAT = AudioFormat.ENCODING_PCM_FLOAT;
    private static final int AUDIO_MODE = AudioTrack.MODE_STREAM;
    private static final int BYTES_PER_FRAME = 2 * 4; // 2 channels, float (4-bytes)

    // Parameter to determine the proper internal buffer size of the AudioTrack instance. In order
    // to minimize latency we want a buffer as small as possible. However, to avoid underruns we
    // need a size several times the size returned by AudioTrack.getMinBufferSize() (see
    // the Android documentation for details).
    private static final int MIN_BUFFER_SIZE_MULTIPLIER = 3;

    private static final long NO_TIMESTAMP = Long.MIN_VALUE;

    private static final long SEC_IN_NSEC = 1000000000L;
    private static final long SEC_IN_USEC = 1000000L;
    private static final long MSEC_IN_NSEC = 1000000L;
    private static final long TIMESTAMP_UPDATE_PERIOD = 3 * SEC_IN_NSEC;
    private static final long UNDERRUN_LOG_THROTTLE_PERIOD = SEC_IN_NSEC;

    // Maximum amount a timestamp may deviate from the previous one to be considered stable.
    private static final long MAX_TIMESTAMP_DEVIATION_NSEC = 1 * MSEC_IN_NSEC;
    // Number of consecutive stable timestamps needed to make it a valid reference point.
    private static final int MIN_TIMESTAMP_STABILITY_CNT = 3;

    // Additional padding for minimum buffer time, determined experimentally.
    private static final long MIN_BUFFERED_TIME_PADDING_US = 20000;

    private static AudioManager sAudioManager = null;

    private static int sSessionIdMedia = AudioManager.ERROR;
    private static int sSessionIdCommunication = AudioManager.ERROR;

    private final long mNativeAudioSinkAudioTrackImpl;

    private boolean mIsInitialized;

    // Dynamic AudioTrack config parameter.
    private int mSampleRateInHz;

    private AudioTrack mAudioTrack;

    // Timestamping logic for RenderingDelay calculations.
    private AudioTimestamp mRefPointTStamp;
    private AudioTimestamp mLastTStampCandidate;
    private long mLastTimestampUpdateNsec; // Last time we updated the timestamp.
    private boolean mTriggerTimestampUpdateNow; // Set to true to trigger an early update.
    private long mTimestampStabilityCounter; // Counts consecutive stable timestamps at startup.
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

    private static AudioManager getAudioManager() {
        if (sAudioManager == null) {
            sAudioManager = (AudioManager) ContextUtils.getApplicationContext().getSystemService(
                    Context.AUDIO_SERVICE);
        }
        return sAudioManager;
    }

    @CalledByNative
    public static long getMinimumBufferedTime(int sampleRateInHz) {
        int sizeBytes = AudioTrack.getMinBufferSize(sampleRateInHz, CHANNEL_CONFIG, AUDIO_FORMAT);
        long sizeUs = SEC_IN_USEC * (long) sizeBytes / (BYTES_PER_FRAME * (long) sampleRateInHz);
        return sizeUs + MIN_BUFFERED_TIME_PADDING_US;
    }

    @CalledByNative
    public static int getSessionIdMedia() {
        if (sSessionIdMedia == AudioManager.ERROR) {
            sSessionIdMedia = getAudioManager().generateAudioSessionId();
            if (sSessionIdMedia == AudioManager.ERROR) {
                Log.e(TAG, "Cannot generate session-id for media tracks!");
            } else {
                Log.i(TAG, "Session-id for media tracks is " + sSessionIdMedia);
            }
        }
        return sSessionIdMedia;
    }

    @CalledByNative
    public static int getSessionIdCommunication() {
        if (sSessionIdCommunication == AudioManager.ERROR) {
            sSessionIdCommunication = getAudioManager().generateAudioSessionId();
            if (sSessionIdCommunication == AudioManager.ERROR) {
                Log.e(TAG, "Cannot generate session-id for communication tracks!");
            } else {
                Log.i(TAG, "Session-id for communication tracks is " + sSessionIdCommunication);
            }
        }
        return sSessionIdCommunication;
    }

    /** Construction */
    @CalledByNative
    private static AudioSinkAudioTrackImpl createAudioSinkAudioTrackImpl(
            long nativeAudioSinkAudioTrackImpl) {
        return new AudioSinkAudioTrackImpl(nativeAudioSinkAudioTrackImpl);
    }

    private AudioSinkAudioTrackImpl(long nativeAudioSinkAudioTrackImpl) {
        mNativeAudioSinkAudioTrackImpl = nativeAudioSinkAudioTrackImpl;
        mRefPointTStamp = new AudioTimestamp();
        mLastTStampCandidate = new AudioTimestamp();
        reset();
    }

    private void reset() {
        mIsInitialized = false;
        mLastTimestampUpdateNsec = NO_TIMESTAMP;
        mLastUnderrunLogNsec = NO_TIMESTAMP;
        mTriggerTimestampUpdateNow = false;
        mTimestampStabilityCounter = 0;
        mLastUnderrunCount = 0;
        mTotalFramesWritten = 0;
    }

    private boolean haveValidRefPoint() {
        return mLastTimestampUpdateNsec != NO_TIMESTAMP;
    }

    /**
     * Initializes the instance by creating the AudioTrack object and allocating
     * the shared memory buffers.
     */
    @CalledByNative
    private void init(
            @AudioContentType int castContentType, int sampleRateInHz, int bytesPerBuffer) {
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

        int usageType = CAST_TYPE_TO_ANDROID_USAGE_TYPE_MAP.get(castContentType);
        int contentType = CAST_TYPE_TO_ANDROID_CONTENT_TYPE_MAP.get(castContentType);

        int sessionId = AudioManager.ERROR;
        if (castContentType == AudioContentType.MEDIA) {
            sessionId = getSessionIdMedia();
        } else if (castContentType == AudioContentType.COMMUNICATION) {
            sessionId = getSessionIdCommunication();
        }
        // AudioContentType.ALARM doesn't get a sessionId.

        int bufferSizeInBytes = MIN_BUFFER_SIZE_MULTIPLIER
                * AudioTrack.getMinBufferSize(mSampleRateInHz, CHANNEL_CONFIG, AUDIO_FORMAT);
        int bufferSizeInMs = 1000 * bufferSizeInBytes / (BYTES_PER_FRAME * mSampleRateInHz);
        Log.i(TAG,
                "Init: create an AudioTrack of size=" + bufferSizeInBytes + " (" + bufferSizeInMs
                        + "ms) usageType=" + usageType + " contentType=" + contentType
                        + " with session-id=" + sessionId);

        AudioTrack.Builder builder = new AudioTrack.Builder();
        builder.setBufferSizeInBytes(bufferSizeInBytes)
                .setTransferMode(AUDIO_MODE)
                .setAudioAttributes(new AudioAttributes.Builder()
                                            .setUsage(usageType)
                                            .setContentType(contentType)
                                            .build())
                .setAudioFormat(new AudioFormat.Builder()
                                        .setEncoding(AUDIO_FORMAT)
                                        .setSampleRate(mSampleRateInHz)
                                        .setChannelMask(CHANNEL_CONFIG)
                                        .build());
        if (sessionId != AudioManager.ERROR) builder.setSessionId(sessionId);

        mAudioTrack = builder.build();

        // Allocated shared buffers.
        mPcmBuffer = ByteBuffer.allocateDirect(bytesPerBuffer);
        mPcmBuffer.order(ByteOrder.nativeOrder());

        mRenderingDelayBuffer = ByteBuffer.allocateDirect(2 * 8); // 2 long
        mRenderingDelayBuffer.order(ByteOrder.nativeOrder());

        nativeCacheDirectBufferAddress(
                mNativeAudioSinkAudioTrackImpl, mPcmBuffer, mRenderingDelayBuffer);

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

    private boolean isStopped() {
        return mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_STOPPED;
    }

    private boolean isPlaying() {
        return mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING;
    }

    private boolean isPaused() {
        return mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PAUSED;
    }

    /** Stops the AudioTrack and returns an estimate of the time it takes for the remaining data
     * left in the internal queue to be played out (in usecs). */
    @CalledByNative
    private long prepareForShutdown() {
        long playtimeLeftNsecs;

        // Stop the AudioTrack. This will put it into STOPPED mode and audio will stop playing after
        // the last buffer that was written has been played.
        mAudioTrack.stop();

        // Estimate how much playing time is left based on the most recent reference point.
        updateRefPointTimestamp();
        if (haveValidRefPoint()) {
            long lastPlayoutTimeNsecs =
                    getInterpolatedTStampNsecs(mRefPointTStamp, mTotalFramesWritten);
            long now = System.nanoTime();
            playtimeLeftNsecs = lastPlayoutTimeNsecs - now;
        } else {
            // We have no timestamp to estimate how much is left to play, so assume the worst case.
            long most_frames_left =
                    Math.min(mTotalFramesWritten, mAudioTrack.getBufferSizeInFrames());
            playtimeLeftNsecs = SEC_IN_NSEC * most_frames_left / mSampleRateInHz;
        }
        return (playtimeLeftNsecs < 0) ? 0 : playtimeLeftNsecs / 1000; // return usecs
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
        if (!isStopped()) mAudioTrack.stop();
        mAudioTrack.release();
        reset();
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

        if (bytesWritten < 0) {
            int error = bytesWritten;
            Log.e(TAG, "Couldn't write into AudioTrack (" + error + ")");
            return error;
        }

        if (isStopped()) {
            // Data was written, start playing now.
            play();

            // If not all data fit on the previous write() call (since we were not in PLAYING state
            // it didn't block), do a second (now blocking) call to write().
            int bytesLeft = sizeInBytes - bytesWritten;
            if (bytesLeft > 0) {
                mPcmBuffer.position(bytesWritten);
                int moreBytesWritten =
                        mAudioTrack.write(mPcmBuffer, bytesLeft, AudioTrack.WRITE_BLOCKING);
                if (moreBytesWritten < 0) {
                    int error = moreBytesWritten;
                    Log.e(TAG, "Couldn't write into AudioTrack (" + error + ")");
                    return error;
                }
                bytesWritten += moreBytesWritten;
            }
        }

        int framesWritten = bytesWritten / BYTES_PER_FRAME;
        mTotalFramesWritten += framesWritten;

        if (DEBUG_LEVEL >= 3) {
            Log.i(TAG,
                    "  wrote " + bytesWritten + "/" + sizeInBytes
                            + " total_bytes_written=" + (mTotalFramesWritten * BYTES_PER_FRAME)
                            + " took:" + (SystemClock.elapsedRealtime() - beforeMsecs) + "ms");
        }

        if (bytesWritten < sizeInBytes && isPaused()) {
            // We are in PAUSED state, in which case the write() is non-blocking. If not all data
            // was written, we will come back here once we transition back into PLAYING state.
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
        checkForUnderruns();
        updateRefPointTimestamp();
        if (!haveValidRefPoint()) {
            // No timestamp available yet, just put dummy values and return.
            mRenderingDelayBuffer.putLong(0, 0);
            mRenderingDelayBuffer.putLong(8, NO_TIMESTAMP);
            return;
        }

        // Interpolate to get proper Rendering delay.
        long playoutTimeNsecs = getInterpolatedTStampNsecs(mRefPointTStamp, mTotalFramesWritten);
        long nowNsecs = System.nanoTime();
        long delayNsecs = playoutTimeNsecs - nowNsecs;

        // Populate RenderingDelay return value for native land.
        mRenderingDelayBuffer.putLong(0, delayNsecs / 1000);
        mRenderingDelayBuffer.putLong(8, nowNsecs / 1000);

        if (DEBUG_LEVEL >= 3) {
            Log.i(TAG,
                    "RenderingDelay: "
                            + " delay=" + (delayNsecs / 1000) + " play=" + (nowNsecs / 1000));
        }
    }

    /**
     * Returns an interpolated timestamp based on the given reference point timestamp and frame
     * position.
     */
    private long getInterpolatedTStampNsecs(AudioTimestamp referencePoint, long framePosition) {
        long deltaFrames = framePosition - referencePoint.framePosition;
        long deltaNsecs = 1000000000L * deltaFrames / mSampleRateInHz;
        long interpolatedTimestampNsecs = referencePoint.nanoTime + deltaNsecs;
        return interpolatedTimestampNsecs;
    }

    /** Checks for underruns and if detected invalidates the reference point timestamp. */
    private void checkForUnderruns() {
        int underruns = getUnderrunCount();
        if (underruns != mLastUnderrunCount) {
            logUnderruns(underruns);
            // Invalidate timestamp (resets RenderingDelay).
            mLastTimestampUpdateNsec = NO_TIMESTAMP;
            mLastUnderrunCount = underruns;
        }
    }

    private boolean isSameTimestamp(AudioTimestamp ts1, AudioTimestamp ts2) {
        return ts1.framePosition == ts2.framePosition && ts1.nanoTime == ts2.nanoTime;
    }

    private void copyTimestamp(AudioTimestamp dst, AudioTimestamp src) {
        dst.framePosition = src.framePosition;
        dst.nanoTime = src.nanoTime;
    }

    /**
     * Returns the deviation between the two given timestamps. Specifically, it uses ts1 to
     * interpolate the nanoTime expected for ts2 and returns the difference.
     */
    private long calculateTimestampDeviation(AudioTimestamp ts1, AudioTimestamp ts2) {
        long expectedTs2NanoTime = getInterpolatedTStampNsecs(ts1, ts2.framePosition);
        return expectedTs2NanoTime - ts2.nanoTime;
    }

    /**
     * Returns true if timestamps are not considered stable. They are not stable at the very
     * beginning of playback.
     */
    private boolean needToCheckTimestampStability() {
        return mTimestampStabilityCounter < MIN_TIMESTAMP_STABILITY_CNT;
    }

    /**
     * Returns true if the given timestamp is stable. A timestamp is considered stable if it and
     * its two predecessors do not deviate significantly from each other.
     */
    private boolean isTimestampStable(AudioTimestamp newTimestamp) {
        if (mTimestampStabilityCounter == 0) {
            copyTimestamp(mLastTStampCandidate, newTimestamp);
            mTimestampStabilityCounter = 1;
            return false;
        }

        if (isSameTimestamp(mLastTStampCandidate, newTimestamp)) {
            // Android can return the same timestamp on successive calls.
            return false;
        }

        long deviation = calculateTimestampDeviation(mLastTStampCandidate, newTimestamp);
        if (Math.abs(deviation) > MAX_TIMESTAMP_DEVIATION_NSEC) {
            // not stable
            Log.i(TAG,
                    "Timestamp [" + mTimestampStabilityCounter
                            + "] is not stable (deviation:" + deviation / 1000 + "us)");
            // Use this as the new starting point.
            copyTimestamp(mLastTStampCandidate, newTimestamp);
            mTimestampStabilityCounter = 1;
            return false;
        }

        if (++mTimestampStabilityCounter >= MIN_TIMESTAMP_STABILITY_CNT) {
            return true;
        }

        return false;
    }

    /**
     * Gets a new reference point timestamp from AudioTrack. For performance reasons we only
     * read a new timestamp in certain intervals.
     */
    private void updateRefPointTimestamp() {
        if (!mTriggerTimestampUpdateNow && haveValidRefPoint()
                && elapsedNsec(mLastTimestampUpdateNsec) <= TIMESTAMP_UPDATE_PERIOD) {
            // not time for an update yet
            return;
        }

        AudioTimestamp newTimestamp = new AudioTimestamp();
        if (!mAudioTrack.getTimestamp(newTimestamp)) {
            return; // no timestamp available
        }

        // We require several stable timestamps before setting a reference point.
        if (needToCheckTimestampStability() && !isTimestampStable(newTimestamp)) {
            return;
        }

        // Got a stable timestamp.
        copyTimestamp(mRefPointTStamp, newTimestamp);

        // Got a new value.
        if (DEBUG_LEVEL >= 1) {
            Log.i(TAG,
                    "New AudioTrack timestamp:"
                            + " pos=" + mRefPointTStamp.framePosition
                            + " ts=" + mRefPointTStamp.nanoTime / 1000 + "us");
        }

        mLastTimestampUpdateNsec = System.nanoTime();
        mTriggerTimestampUpdateNow = false;
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
