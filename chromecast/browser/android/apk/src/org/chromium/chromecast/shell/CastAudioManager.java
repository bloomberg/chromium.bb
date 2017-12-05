// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.media.AudioManager;

/**
 * Wrapper for Cast code to use a single AudioManager instance.
 * Muting and unmuting streams must be invoke on the same AudioManager instance.
 */
public class CastAudioManager {
    // TODO(sanfin): This class should encapsulate SDK-dependent implementation details of
    // android.media.AudioManager.
    private static CastAudioManager sInstance = null;

    public static CastAudioManager getAudioManager(Context context) {
        if (sInstance == null) {
            sInstance = new CastAudioManager(
                    (AudioManager) context.getApplicationContext().getSystemService(
                            Context.AUDIO_SERVICE));
        }
        return sInstance;
    }

    private final AudioManager mAudioManager;

    private CastAudioManager(AudioManager audioManager) {
        mAudioManager = audioManager;
    }

    // TODO(sanfin): Use the AudioFocusRequest version on O and above.
    @SuppressWarnings("deprecation")
    public int requestAudioFocus(
            AudioManager.OnAudioFocusChangeListener l, int streamType, int durationHint) {
        return mAudioManager.requestAudioFocus(l, streamType, durationHint);
    }

    // TODO(sanfin): Use the AudioFocusRequest version on O and above.
    @SuppressWarnings("deprecation")
    public int abandonAudioFocus(AudioManager.OnAudioFocusChangeListener l) {
        return mAudioManager.abandonAudioFocus(l);
    }

    // TODO(sanfin): Do not expose this. All needed AudioManager methods can be adapted with
    // CastAudioManager.
    public AudioManager getInternal() {
        return mAudioManager;
    }
}
