// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodec;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.view.Surface;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.media.MediaCodecUtil.CodecCreationInfo;
import org.chromium.media.MediaCodecUtil.MimeTypes;

import java.nio.ByteBuffer;

@JNINamespace("media")
class MediaCodecBridgeBuilder {
    private static final String TAG = "cr_MediaCodecBridge";

    private static MediaFormat createVideoDecoderFormat(String mime, int width, int height) {
        return MediaFormat.createVideoFormat(mime, width, height);
    }

    private static void setCodecSpecificData(MediaFormat format, int index, byte[] bytes) {
        // Codec Specific Data is set in the MediaFormat as ByteBuffer entries with keys csd-0,
        // csd-1, and so on. See:
        // http://developer.android.com/reference/android/media/MediaCodec.html for details.
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
                return;
        }
        if (name != null && bytes.length > 0) {
            format.setByteBuffer(name, ByteBuffer.wrap(bytes));
        }
    }

    @CalledByNative
    static MediaCodecBridge createVideoDecoder(String mime, @CodecType int codecType,
            MediaCrypto mediaCrypto, int width, int height, Surface surface, byte[] csd0,
            byte[] csd1, HdrMetadata hdrMetadata, boolean allowAdaptivePlayback) {
        CodecCreationInfo info = new CodecCreationInfo();
        try {
            Log.i(TAG, "create MediaCodec video decoder, mime %s", mime);
            info = MediaCodecUtil.createDecoder(mime, codecType, mediaCrypto);
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec video decoder: %s, codecType: %d", mime,
                    codecType, e);
        }

        if (info.mediaCodec == null) return null;

        MediaCodecBridge bridge = new MediaCodecBridge(info.mediaCodec, info.bitrateAdjuster);

        MediaFormat format = createVideoDecoderFormat(mime, width, height);
        if (format == null) return null;

        if (csd0.length > 0) {
            setCodecSpecificData(format, 0, csd0);
        }

        if (csd1.length > 0) {
            setCodecSpecificData(format, 1, csd1);
        }

        if (hdrMetadata != null) {
            hdrMetadata.addMetadataToFormat(format);
        }

        if (!bridge.configureVideo(format, surface, mediaCrypto, 0,
                    info.supportsAdaptivePlayback && allowAdaptivePlayback)) {
            return null;
        }

        if (!bridge.start()) {
            bridge.release();
            return null;
        }
        return bridge;
    }

    private static MediaFormat createVideoEncoderFormat(String mime, int width, int height,
            int bitRate, int frameRate, int iFrameInterval, int colorFormat) {
        MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, frameRate);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iFrameInterval);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat);
        Log.d(TAG, "video encoder format: " + format);
        return format;
    }

    @CalledByNative
    static MediaCodecBridge createVideoEncoder(String mime, int width, int height, int bitRate,
            int frameRate, int iFrameInterval, int colorFormat) {
        CodecCreationInfo info = new CodecCreationInfo();
        try {
            Log.i(TAG, "create MediaCodec video encoder, mime %s", mime);
            info = MediaCodecUtil.createEncoder(mime);
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec video encoder: %s", mime, e);
        }

        if (info.mediaCodec == null) return null;

        // Create MediaCodecEncoder for H264 to meet WebRTC requirements to IDR/keyframes.
        // See https://crbug.com/761336 for more details.
        MediaCodecBridge bridge = mime.equals(MimeTypes.VIDEO_H264)
                ? new MediaCodecEncoder(info.mediaCodec, info.bitrateAdjuster)
                : new MediaCodecBridge(info.mediaCodec, info.bitrateAdjuster);

        frameRate = info.bitrateAdjuster.getInitialFrameRate(frameRate);
        MediaFormat format = createVideoEncoderFormat(
                mime, width, height, bitRate, frameRate, iFrameInterval, colorFormat);

        if (!bridge.configureVideo(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE,
                    info.supportsAdaptivePlayback)) {
            return null;
        }

        if (!bridge.start()) {
            bridge.release();
            return null;
        }
        return bridge;
    }

    private static MediaFormat createAudioFormat(String mime, int sampleRate, int channelCount) {
        return MediaFormat.createAudioFormat(mime, sampleRate, channelCount);
    }

    @CalledByNative
    static MediaCodecBridge createAudioDecoder(String mime, MediaCrypto mediaCrypto, int sampleRate,
            int channelCount, byte[] csd0, byte[] csd1, byte[] csd2, boolean frameHasAdtsHeader) {
        CodecCreationInfo info = new CodecCreationInfo();
        try {
            Log.i(TAG, "create MediaCodec audio decoder, mime %s", mime);
            info = MediaCodecUtil.createDecoder(mime, CodecType.ANY, mediaCrypto);
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec audio decoder: %s", mime, e);
        }

        if (info.mediaCodec == null) return null;

        MediaCodecBridge bridge = new MediaCodecBridge(info.mediaCodec, info.bitrateAdjuster);

        MediaFormat format = createAudioFormat(mime, sampleRate, channelCount);

        if (csd0.length > 0) {
            setCodecSpecificData(format, 0, csd0);
        }

        if (csd1.length > 0) {
            setCodecSpecificData(format, 1, csd1);
        }

        if (csd2.length > 0) {
            setCodecSpecificData(format, 2, csd2);
        }

        if (frameHasAdtsHeader) {
            format.setInteger(MediaFormat.KEY_IS_ADTS, 1);
        }

        if (!bridge.configureAudio(format, mediaCrypto, 0)) return null;

        if (!bridge.start()) {
            bridge.release();
            return null;
        }
        return bridge;
    }
}
