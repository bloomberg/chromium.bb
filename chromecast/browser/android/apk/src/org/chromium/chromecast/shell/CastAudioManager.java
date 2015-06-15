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

    private static AudioManager sAudioManager = null;

    public static AudioManager getAudioManager(Context context) {
        if (sAudioManager == null) {
            sAudioManager = (AudioManager) context.getApplicationContext()
                .getSystemService(Context.AUDIO_SERVICE);
        }
        return sAudioManager;
    }
}
