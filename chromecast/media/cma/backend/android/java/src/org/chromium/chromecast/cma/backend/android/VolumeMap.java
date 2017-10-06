// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.cma.backend.android;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.util.SparseIntArray;

import com.google.android.things.audio.VolumeTableReader;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Implements the java-side of the volume control API that maps between volume levels ([0..100])
 * and dBFS values. It uses an Android Things specific system API.
 */
@JNINamespace("chromecast::media")
@TargetApi(Build.VERSION_CODES.N)
public final class VolumeMap {
    private static final String TAG = "VolumeMap";

    private static AudioManager sAudioManager = null;

    // Mapping from Android's stream_type to Cast's AudioContentType (used for callback).
    private static final SparseIntArray ANDROID_TYPE_TO_CAST_TYPE_MAP = new SparseIntArray(3) {
        {
            append(AudioManager.STREAM_MUSIC, AudioContentType.MEDIA);
            append(AudioManager.STREAM_ALARM, AudioContentType.ALARM);
            append(AudioManager.STREAM_SYSTEM, AudioContentType.COMMUNICATION);
        }
    };

    private static final SparseIntArray MAX_VOLUME_INDEX = new SparseIntArray(3) {
        {
            append(AudioManager.STREAM_MUSIC,
                    getAudioManager().getStreamMaxVolume(AudioManager.STREAM_MUSIC));
            append(AudioManager.STREAM_ALARM,
                    getAudioManager().getStreamMaxVolume(AudioManager.STREAM_ALARM));
            append(AudioManager.STREAM_SYSTEM,
                    getAudioManager().getStreamMaxVolume(AudioManager.STREAM_SYSTEM));
        }
    };

    private static AudioManager getAudioManager() {
        if (sAudioManager == null) {
            Context context = ContextUtils.getApplicationContext();
            sAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        }
        return sAudioManager;
    }

    private static int getStreamType(int castType) {
        int i = ANDROID_TYPE_TO_CAST_TYPE_MAP.indexOfValue(castType);
        return ANDROID_TYPE_TO_CAST_TYPE_MAP.keyAt(i);
    }

    /**
     * Logs the dB value at each discrete Android volume index for the given cast type.
     * Note that this is not identical to the volume table, which may contain a different number
     * of points and at different levels.
     */
    @CalledByNative
    static void dumpVolumeTables(int castType) {
        int streamType = getStreamType(castType);
        int maxIndex = MAX_VOLUME_INDEX.get(streamType);
        int deviceType = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;
        Log.i(TAG, "Volume points for stream " + streamType + " (maxIndex=" + maxIndex + "):");
        for (int idx = 0; idx <= maxIndex; idx++) {
            float db = VolumeTableReader.getStreamVolumeDB(streamType, idx, deviceType);
            float level = (float) idx / (float) maxIndex;
            Log.i(TAG, "    " + idx + "(" + level + ") -> " + db);
        }
    }

    /**
     * Returns the dB value for the given volume level using the volume table for the given type.
     */
    @CalledByNative
    static float volumeToDbFs(int castType, float level) {
        level = Math.min(1.0f, Math.max(0.0f, level));
        int streamType = getStreamType(castType);
        int volumeIndex = Math.round(level * (float) MAX_VOLUME_INDEX.get(streamType));
        int deviceType = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;
        return VolumeTableReader.getStreamVolumeDB(streamType, volumeIndex, deviceType);
    }

    @CalledByNative
    /**
     * Returns the volume level for the given dB value using the volume table for the given type.
     */
    static float dbFsToVolume(int castType, float db) {
        int streamType = getStreamType(castType);
        int maxIndex = MAX_VOLUME_INDEX.get(streamType);
        int deviceType = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;

        float dbMin = VolumeTableReader.getStreamVolumeDB(streamType, 0, deviceType);
        if (db <= dbMin) return 0.0f;
        float dbMax = VolumeTableReader.getStreamVolumeDB(streamType, maxIndex, deviceType);
        if (db >= dbMax) return 1.0f;

        // There are only a few volume index steps, so simply loop through them
        // and find the interval [dbLeft .. dbRight] that contains db, then
        // interpolate to estimate the volume level to return.
        float dbLeft = dbMin, dbRight = dbMin;
        int idx = 1;
        for (; idx <= maxIndex; idx++) {
            dbLeft = dbRight;
            dbRight = VolumeTableReader.getStreamVolumeDB(streamType, idx, deviceType);
            if (db <= dbRight) {
                break;
            }
        }
        float interpolatedIdx = (db - dbLeft) / (dbRight - dbLeft) + (idx - 1);
        float level = Math.min(1.0f, Math.max(0.0f, interpolatedIdx / (float) maxIndex));
        return level;
    }
}
