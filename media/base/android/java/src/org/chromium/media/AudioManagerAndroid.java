// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.media.AudioManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

@JNINamespace("media")
class AudioManagerAndroid {
    @CalledByNative
    public static void setMode(Context context, int mode) {
        AudioManager audioManager =
                (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
        if (null != audioManager) {
            audioManager.setMode(mode);
        }
    }
}
