// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * A collection of MediaCodec utility functions.
 */
@JNINamespace("media")
class MediaCodecUtil {
    private static final String TAG = "MediaCodecUtil";

    // Codec direction.  Keep this in sync with media_codec_bridge.h.
    static final int MEDIA_CODEC_DECODER = 0;
    static final int MEDIA_CODEC_ENCODER = 1;

    /**
     * This class represents supported android codec information.
     */
    private static class CodecInfo {
        private final String mCodecType; // e.g. "video/x-vnd.on2.vp8".
        private final String mCodecName; // e.g. "OMX.google.vp8.decoder".
        private final int mDirection;

        private CodecInfo(String codecType, String codecName, int direction) {
            mCodecType = codecType;
            mCodecName = codecName;
            mDirection = direction;
        }

        @CalledByNative("CodecInfo")
        private String codecType() {
            return mCodecType;
        }

        @CalledByNative("CodecInfo")
        private String codecName() {
            return mCodecName;
        }

        @CalledByNative("CodecInfo")
        private int direction() {
            return mDirection;
        }
    }

    /**
     * @return a list of supported android codec information.
     */
    @SuppressWarnings("deprecation")
    @CalledByNative
    private static CodecInfo[] getCodecsInfo() {
        // Return the first (highest-priority) codec for each MIME type.
        Map<String, CodecInfo> encoderInfoMap = new HashMap<String, CodecInfo>();
        Map<String, CodecInfo> decoderInfoMap = new HashMap<String, CodecInfo>();
        int count = MediaCodecList.getCodecCount();
        for (int i = 0; i < count; ++i) {
            MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
            int direction = info.isEncoder() ? MEDIA_CODEC_ENCODER : MEDIA_CODEC_DECODER;
            String codecString = info.getName();
            String[] supportedTypes = info.getSupportedTypes();
            for (int j = 0; j < supportedTypes.length; ++j) {
                Map<String, CodecInfo> map = info.isEncoder() ? encoderInfoMap : decoderInfoMap;
                if (!map.containsKey(supportedTypes[j])) {
                    map.put(supportedTypes[j],
                            new CodecInfo(supportedTypes[j], codecString, direction));
                }
            }
        }
        ArrayList<CodecInfo> codecInfos =
                new ArrayList<CodecInfo>(decoderInfoMap.size() + encoderInfoMap.size());
        codecInfos.addAll(encoderInfoMap.values());
        codecInfos.addAll(decoderInfoMap.values());
        return codecInfos.toArray(new CodecInfo[codecInfos.size()]);
    }

    /**
     * Get a name of default android codec.
     * @param mime MIME type of the media.
     * @param direction Whether this is encoder or decoder.
     * @return name of the codec.
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @SuppressWarnings("deprecation")
    @CalledByNative
    private static String getDefaultCodecName(String mime, int direction) {
        String codecName = "";
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            try {
                MediaCodec mediaCodec = null;
                if (direction == MEDIA_CODEC_ENCODER) {
                    mediaCodec = MediaCodec.createEncoderByType(mime);
                } else {
                    mediaCodec = MediaCodec.createDecoderByType(mime);
                }
                codecName = mediaCodec.getName();
                mediaCodec.release();
            } catch (Exception e) {
                Log.w(TAG, "getDefaultCodecName: Failed to create MediaCodec: %s, direction: %d",
                        mime, direction, e);
            }
        }
        return codecName;
    }

    /**
     * Get a list of encoder supported color formats for specified MIME type.
     * @param mime MIME type of the media format.
     * @return a list of encoder supported color formats.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @SuppressWarnings("deprecation")
    @CalledByNative
    private static int[] getEncoderColorFormatsForMime(String mime) {
        MediaCodecInfo[] codecs = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            MediaCodecList mediaCodecList = new MediaCodecList(MediaCodecList.ALL_CODECS);
            codecs = mediaCodecList.getCodecInfos();
        } else {
            int count = MediaCodecList.getCodecCount();
            if (count <= 0) {
                return null;
            }
            codecs = new MediaCodecInfo[count];
            for (int i = 0; i < count; ++i) {
                MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
                codecs[i] = info;
            }
        }

        for (int i = 0; i < codecs.length; i++) {
            if (!codecs[i].isEncoder()) {
                continue;
            }

            String[] supportedTypes = codecs[i].getSupportedTypes();
            for (int j = 0; j < supportedTypes.length; ++j) {
                if (!supportedTypes[j].equalsIgnoreCase(mime)) {
                    continue;
                }

                MediaCodecInfo.CodecCapabilities capabilities =
                        codecs[i].getCapabilitiesForType(mime);
                return capabilities.colorFormats;
            }
        }
        return null;
    }

    /**
     * Get decoder name for the input MIME type.
     * @param mime MIME type of the media.
     * @return name of the decoder.
     */
    @SuppressWarnings("deprecation")
    protected static String getDecoderNameForMime(String mime) {
        int count = MediaCodecList.getCodecCount();
        for (int i = 0; i < count; ++i) {
            MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
            if (info.isEncoder()) {
                continue;
            }

            String[] supportedTypes = info.getSupportedTypes();
            for (int j = 0; j < supportedTypes.length; ++j) {
                if (supportedTypes[j].equalsIgnoreCase(mime)) {
                    return info.getName();
                }
            }
        }
        return null;
    }

    /**
     * Check if a given MIME type can be decoded.
     * @param mime MIME type of the media.
     * @param secure Whether secure decoder is required.
     * @return true if system is able to decode, or false otherwise.
     */
    @CalledByNative
    private static boolean canDecode(String mime, boolean isSecure) {
        // Creation of ".secure" codecs sometimes crash instead of throwing exceptions
        // on pre-JBMR2 devices.
        if (isSecure && Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) {
            return false;
        }
        MediaCodec mediaCodec = null;
        try {
            // |isSecure| only applies to video decoders.
            if (mime.startsWith("video") && isSecure) {
                String decoderName = getDecoderNameForMime(mime);
                if (decoderName == null) return false;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                    // To work around an issue that we cannot get the codec info from the secure
                    // decoder, create an insecure decoder first so that we can query its codec
                    // info. http://b/15587335.
                    MediaCodec insecureCodec = MediaCodec.createByCodecName(decoderName);
                    insecureCodec.release();
                }
                mediaCodec = MediaCodec.createByCodecName(decoderName + ".secure");
            } else {
                mediaCodec = MediaCodec.createDecoderByType(mime);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec: %s, isSecure: %s", mime, isSecure, e);
        }

        if (mediaCodec == null) return false;
        try {
            mediaCodec.release();
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot release media codec", e);
        }
        return true;
    }
}
