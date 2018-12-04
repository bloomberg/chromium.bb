// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;

import org.chromium.base.annotations.JNINamespace;

/**
 * Subclass of TtsPlatformImpl for Lollipop to make use of newer APIs.
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
class LollipopTtsPlatformImpl extends TtsPlatformImpl {
    protected LollipopTtsPlatformImpl(long nativeTtsPlatformImplAndroid) {
        super(nativeTtsPlatformImplAndroid);
    }

    /**
     * Overrides TtsPlatformImpl because the API changed in Lollipop.
     */
    @Override
    protected void addOnUtteranceProgressListener() {
        mTextToSpeech.setOnUtteranceProgressListener(new UtteranceProgressListener() {
            @Override
            public void onDone(final String utteranceId) {
                sendEndEventOnUiThread(utteranceId);
            }

            @Override
            public void onError(final String utteranceId, int errorCode) {
                sendErrorEventOnUiThread(utteranceId);
            }

            @Override
            @Deprecated
            public void onError(final String utteranceId) {}

            @Override
            public void onStart(final String utteranceId) {
                sendStartEventOnUiThread(utteranceId);
            }
        });
    }

    /**
     * Overrides TtsPlatformImpl because the API changed in Lollipop.
     */
    @Override
    protected int callSpeak(String text, float volume, int utteranceId) {
        Bundle params = new Bundle();
        if (volume != 1.0) {
            params.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, volume);
        }
        return mTextToSpeech.speak(
                text, TextToSpeech.QUEUE_FLUSH, params, Integer.toString(utteranceId));
    }
}
