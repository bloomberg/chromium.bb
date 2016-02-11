// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaCodec;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.view.Surface;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

import java.nio.ByteBuffer;

/**
 * A wrapper of the MediaCodec class to facilitate exception capturing and
 * audio rendering.
 */
@JNINamespace("media")
class MediaCodecBridge {
    private static final String TAG = "cr_media";

    // Error code for MediaCodecBridge. Keep this value in sync with
    // MediaCodecStatus in media_codec_bridge.h.
    private static final int MEDIA_CODEC_OK = 0;
    private static final int MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER = 1;
    private static final int MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER = 2;
    private static final int MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED = 3;
    private static final int MEDIA_CODEC_OUTPUT_FORMAT_CHANGED = 4;
    private static final int MEDIA_CODEC_INPUT_END_OF_STREAM = 5;
    private static final int MEDIA_CODEC_OUTPUT_END_OF_STREAM = 6;
    private static final int MEDIA_CODEC_NO_KEY = 7;
    private static final int MEDIA_CODEC_ABORT = 8;
    private static final int MEDIA_CODEC_ERROR = 9;

    // Max adaptive playback size to be supplied to the decoder.
    private static final int MAX_ADAPTIVE_PLAYBACK_WIDTH = 1920;
    private static final int MAX_ADAPTIVE_PLAYBACK_HEIGHT = 1080;

    // After a flush(), dequeueOutputBuffer() can often produce empty presentation timestamps
    // for several frames. As a result, the player may find that the time does not increase
    // after decoding a frame. To detect this, we check whether the presentation timestamp from
    // dequeueOutputBuffer() is larger than input_timestamp - MAX_PRESENTATION_TIMESTAMP_SHIFT_US
    // after a flush. And we set the presentation timestamp from dequeueOutputBuffer() to be
    // non-decreasing for the remaining frames.
    private static final long MAX_PRESENTATION_TIMESTAMP_SHIFT_US = 100000;

    // TODO(qinmin): Use MediaFormat constants when part of the public API.
    private static final String KEY_CROP_LEFT = "crop-left";
    private static final String KEY_CROP_RIGHT = "crop-right";
    private static final String KEY_CROP_BOTTOM = "crop-bottom";
    private static final String KEY_CROP_TOP = "crop-top";

    private ByteBuffer[] mInputBuffers;
    private ByteBuffer[] mOutputBuffers;

    private MediaCodec mMediaCodec;
    private AudioTrack mAudioTrack;
    private byte[] mPendingAudioBuffer;
    private boolean mFlushed;
    private long mLastPresentationTimeUs;
    private String mMime;
    private boolean mAdaptivePlaybackSupported;

    @MainDex
    private static class DequeueInputResult {
        private final int mStatus;
        private final int mIndex;

        private DequeueInputResult(int status, int index) {
            mStatus = status;
            mIndex = index;
        }

        @CalledByNative("DequeueInputResult")
        private int status() {
            return mStatus;
        }

        @CalledByNative("DequeueInputResult")
        private int index() {
            return mIndex;
        }
    }

    @MainDex
    private static class DequeueOutputResult {
        private final int mStatus;
        private final int mIndex;
        private final int mFlags;
        private final int mOffset;
        private final long mPresentationTimeMicroseconds;
        private final int mNumBytes;

        private DequeueOutputResult(int status, int index, int flags, int offset,
                long presentationTimeMicroseconds, int numBytes) {
            mStatus = status;
            mIndex = index;
            mFlags = flags;
            mOffset = offset;
            mPresentationTimeMicroseconds = presentationTimeMicroseconds;
            mNumBytes = numBytes;
        }

        @CalledByNative("DequeueOutputResult")
        private int status() {
            return mStatus;
        }

        @CalledByNative("DequeueOutputResult")
        private int index() {
            return mIndex;
        }

        @CalledByNative("DequeueOutputResult")
        private int flags() {
            return mFlags;
        }

        @CalledByNative("DequeueOutputResult")
        private int offset() {
            return mOffset;
        }

        @CalledByNative("DequeueOutputResult")
        private long presentationTimeMicroseconds() {
            return mPresentationTimeMicroseconds;
        }

        @CalledByNative("DequeueOutputResult")
        private int numBytes() {
            return mNumBytes;
        }
    }

    private MediaCodecBridge(
            MediaCodec mediaCodec, String mime, boolean adaptivePlaybackSupported) {
        assert mediaCodec != null;
        mMediaCodec = mediaCodec;
        mPendingAudioBuffer = null;
        mMime = mime;
        mLastPresentationTimeUs = 0;
        mFlushed = true;
        mAdaptivePlaybackSupported = adaptivePlaybackSupported;
    }

    @CalledByNative
    private static MediaCodecBridge create(String mime, boolean isSecure, int direction) {
        MediaCodecUtil.CodecCreationInfo info = new MediaCodecUtil.CodecCreationInfo();
        try {
            if (direction == MediaCodecUtil.MEDIA_CODEC_ENCODER) {
                info.mediaCodec = MediaCodec.createEncoderByType(mime);
                info.supportsAdaptivePlayback = false;
            } else {
                // |isSecure| only applies to video decoders.
                info = MediaCodecUtil.createDecoder(mime, isSecure);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec: %s, isSecure: %s, direction: %d",
                    mime, isSecure, direction, e);
        }

        if (info.mediaCodec == null) return null;

        return new MediaCodecBridge(info.mediaCodec, mime, info.supportsAdaptivePlayback);
    }

    @CalledByNative
    private void release() {
        try {
            Log.w(TAG, "calling MediaCodec.release()");
            mMediaCodec.release();
        } catch (IllegalStateException e) {
            // The MediaCodec is stuck in a wrong state, possibly due to losing
            // the surface.
            Log.e(TAG, "Cannot release media codec", e);
        }
        mMediaCodec = null;
        if (mAudioTrack != null) {
            mAudioTrack.release();
        }
        mPendingAudioBuffer = null;
    }

    @SuppressWarnings("deprecation")
    @CalledByNative
    private boolean start() {
        try {
            mMediaCodec.start();
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.KITKAT) {
                mInputBuffers = mMediaCodec.getInputBuffers();
                mOutputBuffers = mMediaCodec.getOutputBuffers();
            }
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot start the media codec", e);
            return false;
        }
        return true;
    }

    @CalledByNative
    private DequeueInputResult dequeueInputBuffer(long timeoutUs) {
        int status = MEDIA_CODEC_ERROR;
        int index = -1;
        try {
            int indexOrStatus = mMediaCodec.dequeueInputBuffer(timeoutUs);
            if (indexOrStatus >= 0) { // index!
                status = MEDIA_CODEC_OK;
                index = indexOrStatus;
            } else if (indexOrStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                status = MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER;
            } else {
                Log.e(TAG, "Unexpected index_or_status: " + indexOrStatus);
                assert false;
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to dequeue input buffer", e);
        }
        return new DequeueInputResult(status, index);
    }

    @CalledByNative
    private int flush() {
        try {
            mFlushed = true;
            if (mAudioTrack != null) {
                // Need to call pause() here, or otherwise flush() is a no-op.
                mAudioTrack.pause();
                mAudioTrack.flush();
                mPendingAudioBuffer = null;
            }
            mMediaCodec.flush();
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to flush MediaCodec", e);
            return MEDIA_CODEC_ERROR;
        }
        return MEDIA_CODEC_OK;
    }

    @CalledByNative
    private void stop() {
        mMediaCodec.stop();
        if (mAudioTrack != null) {
            mAudioTrack.pause();
        }
    }

    private boolean outputFormatHasCropValues(MediaFormat format) {
        return format.containsKey(KEY_CROP_RIGHT) && format.containsKey(KEY_CROP_LEFT)
                && format.containsKey(KEY_CROP_BOTTOM) && format.containsKey(KEY_CROP_TOP);
    }

    @CalledByNative
    private int getOutputHeight() {
        MediaFormat format = mMediaCodec.getOutputFormat();
        return outputFormatHasCropValues(format)
                ? format.getInteger(KEY_CROP_BOTTOM) - format.getInteger(KEY_CROP_TOP) + 1
                : format.getInteger(MediaFormat.KEY_HEIGHT);
    }

    @CalledByNative
    private int getOutputWidth() {
        MediaFormat format = mMediaCodec.getOutputFormat();
        return outputFormatHasCropValues(format)
                ? format.getInteger(KEY_CROP_RIGHT) - format.getInteger(KEY_CROP_LEFT) + 1
                : format.getInteger(MediaFormat.KEY_WIDTH);
    }

    @CalledByNative
    private int getOutputSamplingRate() {
        MediaFormat format = mMediaCodec.getOutputFormat();
        return format.getInteger(MediaFormat.KEY_SAMPLE_RATE);
    }

    @CalledByNative
    private ByteBuffer getInputBuffer(int index) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) {
            return mMediaCodec.getInputBuffer(index);
        }
        return mInputBuffers[index];
    }

    @CalledByNative
    private ByteBuffer getOutputBuffer(int index) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) {
            return mMediaCodec.getOutputBuffer(index);
        }
        return mOutputBuffers[index];
    }

    @CalledByNative
    private int queueInputBuffer(
            int index, int offset, int size, long presentationTimeUs, int flags) {
        resetLastPresentationTimeIfNeeded(presentationTimeUs);
        try {
            mMediaCodec.queueInputBuffer(index, offset, size, presentationTimeUs, flags);
        } catch (Exception e) {
            Log.e(TAG, "Failed to queue input buffer", e);
            return MEDIA_CODEC_ERROR;
        }
        return MEDIA_CODEC_OK;
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    @CalledByNative
    private void setVideoBitrate(int bps) {
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, bps);
        mMediaCodec.setParameters(b);
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    @CalledByNative
    private void requestKeyFrameSoon() {
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
        mMediaCodec.setParameters(b);
    }

    @CalledByNative
    private int queueSecureInputBuffer(
            int index, int offset, byte[] iv, byte[] keyId, int[] numBytesOfClearData,
            int[] numBytesOfEncryptedData, int numSubSamples, long presentationTimeUs) {
        resetLastPresentationTimeIfNeeded(presentationTimeUs);
        try {
            MediaCodec.CryptoInfo cryptoInfo = new MediaCodec.CryptoInfo();
            cryptoInfo.set(numSubSamples, numBytesOfClearData, numBytesOfEncryptedData,
                    keyId, iv, MediaCodec.CRYPTO_MODE_AES_CTR);
            mMediaCodec.queueSecureInputBuffer(index, offset, cryptoInfo, presentationTimeUs, 0);
        } catch (MediaCodec.CryptoException e) {
            if (e.getErrorCode() == MediaCodec.CryptoException.ERROR_NO_KEY) {
                Log.d(TAG, "Failed to queue secure input buffer: CryptoException.ERROR_NO_KEY");
                return MEDIA_CODEC_NO_KEY;
            }
            Log.e(TAG, "Failed to queue secure input buffer, CryptoException with error code "
                            + e.getErrorCode());
            return MEDIA_CODEC_ERROR;
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to queue secure input buffer, IllegalStateException " + e);
            return MEDIA_CODEC_ERROR;
        }
        return MEDIA_CODEC_OK;
    }

    @CalledByNative
    private void releaseOutputBuffer(int index, boolean render) {
        try {
            mMediaCodec.releaseOutputBuffer(index, render);
        } catch (IllegalStateException e) {
            // TODO(qinmin): May need to report the error to the caller. crbug.com/356498.
            Log.e(TAG, "Failed to release output buffer", e);
        }
    }

    @SuppressWarnings("deprecation")
    @CalledByNative
    private DequeueOutputResult dequeueOutputBuffer(long timeoutUs) {
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        int status = MEDIA_CODEC_ERROR;
        int index = -1;
        try {
            int indexOrStatus = mMediaCodec.dequeueOutputBuffer(info, timeoutUs);
            if (info.presentationTimeUs < mLastPresentationTimeUs) {
                // TODO(qinmin): return a special code through DequeueOutputResult
                // to notify the native code the the frame has a wrong presentation
                // timestamp and should be skipped.
                info.presentationTimeUs = mLastPresentationTimeUs;
            }
            mLastPresentationTimeUs = info.presentationTimeUs;

            if (indexOrStatus >= 0) { // index!
                status = MEDIA_CODEC_OK;
                index = indexOrStatus;
            } else if (indexOrStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                assert Build.VERSION.SDK_INT <= Build.VERSION_CODES.KITKAT;
                mOutputBuffers = mMediaCodec.getOutputBuffers();
                status = MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED;
            } else if (indexOrStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                status = MEDIA_CODEC_OUTPUT_FORMAT_CHANGED;
                MediaFormat newFormat = mMediaCodec.getOutputFormat();
                if (mAudioTrack != null && newFormat.containsKey(MediaFormat.KEY_SAMPLE_RATE)) {
                    int newSampleRate = newFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                    if (mAudioTrack.setPlaybackRate(newSampleRate) != AudioTrack.SUCCESS) {
                        status = MEDIA_CODEC_ERROR;
                    }
                }
            } else if (indexOrStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                status = MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER;
            } else {
                Log.e(TAG, "Unexpected index_or_status: " + indexOrStatus);
                assert false;
            }
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to dequeue output buffer", e);
        }

        return new DequeueOutputResult(
                status, index, info.flags, info.offset, info.presentationTimeUs, info.size);
    }

    @CalledByNative
    private boolean configureVideo(MediaFormat format, Surface surface, MediaCrypto crypto,
            int flags) {
        try {
            if (mAdaptivePlaybackSupported) {
                format.setInteger(MediaFormat.KEY_MAX_WIDTH, MAX_ADAPTIVE_PLAYBACK_WIDTH);
                format.setInteger(MediaFormat.KEY_MAX_HEIGHT, MAX_ADAPTIVE_PLAYBACK_HEIGHT);
            }
            mMediaCodec.configure(format, surface, crypto, flags);
            return true;
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot configure the video codec, wrong format or surface", e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot configure the video codec", e);
        } catch (MediaCodec.CryptoException e) {
            Log.e(TAG, "Cannot configure the video codec: DRM error", e);
        } catch (Exception e) {
            Log.e(TAG, "Cannot configure the video codec", e);
        }
        return false;
    }

    @CalledByNative
    private static MediaFormat createAudioFormat(String mime, int sampleRate, int channelCount) {
        return MediaFormat.createAudioFormat(mime, sampleRate, channelCount);
    }

    @CalledByNative
    private static MediaFormat createVideoDecoderFormat(String mime, int width, int height) {
        return MediaFormat.createVideoFormat(mime, width, height);
    }

    @CalledByNative
    private static MediaFormat createVideoEncoderFormat(String mime, int width, int height,
            int bitRate, int frameRate, int iFrameInterval, int colorFormat) {
        MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, frameRate);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iFrameInterval);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat);
        return format;
    }

    @CalledByNative
    private boolean isAdaptivePlaybackSupported(int width, int height) {
        if (!mAdaptivePlaybackSupported) return false;
        return width <= MAX_ADAPTIVE_PLAYBACK_WIDTH && height <= MAX_ADAPTIVE_PLAYBACK_HEIGHT;
    }

    @CalledByNative
    private static void setCodecSpecificData(MediaFormat format, int index, byte[] bytes) {
        // Codec Specific Data is set in the MediaFormat as ByteBuffer entries with keys csd-0,
        // csd-1, and so on. See: http://developer.android.com/reference/android/media/MediaCodec.html
        // for details.
        String name;
        switch (index) {
            case 0:
                name = "csd-0";
                break;
            case 1:
                name = "csd-1";
                break;
            case 2:
                name = "csd-2";
                break;
            default:
                name = null;
                break;
        }
        if (name != null) {
            format.setByteBuffer(name, ByteBuffer.wrap(bytes));
        }
    }

    @CalledByNative
    private static void setFrameHasADTSHeader(MediaFormat format) {
        format.setInteger(MediaFormat.KEY_IS_ADTS, 1);
    }

    @CalledByNative
    private boolean configureAudio(MediaFormat format, MediaCrypto crypto, int flags,
            boolean playAudio) {
        try {
            mMediaCodec.configure(format, null, crypto, flags);
            if (playAudio) {
                int sampleRate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                int channelCount = format.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
                int channelConfig = getAudioFormat(channelCount);
                // Using 16bit PCM for output. Keep this value in sync with
                // kBytesPerAudioOutputSample in media_codec_bridge.cc.
                int minBufferSize = AudioTrack.getMinBufferSize(sampleRate, channelConfig,
                        AudioFormat.ENCODING_PCM_16BIT);
                mAudioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate, channelConfig,
                        AudioFormat.ENCODING_PCM_16BIT, minBufferSize, AudioTrack.MODE_STREAM);
                if (mAudioTrack.getState() == AudioTrack.STATE_UNINITIALIZED) {
                    Log.e(TAG, "Cannot create AudioTrack");
                    mAudioTrack = null;
                    return false;
                }
            }
            return true;
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot configure the audio codec", e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot configure the audio codec", e);
        } catch (MediaCodec.CryptoException e) {
            Log.e(TAG, "Cannot configure the audio codec: DRM error", e);
        } catch (Exception e) {
            Log.e(TAG, "Cannot configure the audio codec", e);
        }
        return false;
    }

    /**
     *  Play the audio buffer that is passed in.
     *
     *  @param buf Audio buffer to be rendered.
     *  @param postpone If true, save audio buffer for playback with the next
     *  audio buffer. Must be followed by playOutputBuffer() without postpone,
     *  flush() or release().
     *  @return The number of frames that have already been consumed by the
     *  hardware. This number resets to 0 after each flush call.
     */
    @CalledByNative
    private long playOutputBuffer(byte[] buf, boolean postpone) {
        if (mAudioTrack == null) {
            return 0;
        }

        if (postpone) {
            assert mPendingAudioBuffer == null;
            mPendingAudioBuffer = buf;
            return 0;
        }

        if (AudioTrack.PLAYSTATE_PLAYING != mAudioTrack.getPlayState()) {
            mAudioTrack.play();
        }

        int size = 0;
        if (mPendingAudioBuffer != null) {
            size = mAudioTrack.write(mPendingAudioBuffer, 0, mPendingAudioBuffer.length);
            if (mPendingAudioBuffer.length != size) {
                Log.i(TAG, "Failed to send all data to audio output, expected size: "
                                + mPendingAudioBuffer.length + ", actual size: " + size);
            }
            mPendingAudioBuffer = null;
        }

        size = mAudioTrack.write(buf, 0, buf.length);
        if (buf.length != size) {
            Log.i(TAG, "Failed to send all data to audio output, expected size: "
                    + buf.length + ", actual size: " + size);
        }
        // TODO(qinmin): Returning the head position allows us to estimate
        // the current presentation time in native code. However, it is
        // better to use AudioTrack.getCurrentTimestamp() to get the last
        // known time when a frame is played. However, we will need to
        // convert the java nano time to C++ timestamp.
        // If the stream runs too long, getPlaybackHeadPosition() could
        // overflow. AudioTimestampHelper in MediaSourcePlayer has the same
        // issue. See http://crbug.com/358801.

        // The method AudioTrack.getPlaybackHeadPosition() returns int that should be
        // interpreted as unsigned 32 bit value. Convert the return value of
        // getPlaybackHeadPosition() into unsigned int using the long mask.
        return 0xFFFFFFFFL & mAudioTrack.getPlaybackHeadPosition();
    }

    @SuppressWarnings("deprecation")
    @CalledByNative
    private void setVolume(double volume) {
        if (mAudioTrack != null) {
            mAudioTrack.setStereoVolume((float) volume, (float) volume);
        }
    }

    private void resetLastPresentationTimeIfNeeded(long presentationTimeUs) {
        if (mFlushed) {
            mLastPresentationTimeUs =
                    Math.max(presentationTimeUs - MAX_PRESENTATION_TIMESTAMP_SHIFT_US, 0);
            mFlushed = false;
        }
    }

    @SuppressWarnings("deprecation")
    private int getAudioFormat(int channelCount) {
        switch (channelCount) {
            case 1:
                return AudioFormat.CHANNEL_OUT_MONO;
            case 2:
                return AudioFormat.CHANNEL_OUT_STEREO;
            case 4:
                return AudioFormat.CHANNEL_OUT_QUAD;
            case 6:
                return AudioFormat.CHANNEL_OUT_5POINT1;
            case 8:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    return AudioFormat.CHANNEL_OUT_7POINT1_SURROUND;
                } else {
                    return AudioFormat.CHANNEL_OUT_7POINT1;
                }
            default:
                return AudioFormat.CHANNEL_OUT_DEFAULT;
        }
    }
}
