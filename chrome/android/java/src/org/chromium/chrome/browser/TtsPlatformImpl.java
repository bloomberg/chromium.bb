// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;

/**
 * This class is the Java counterpart to the C++ TtsPlatformImplAndroid class.
 * It implements the Android-native text-to-speech code to support the web
 * speech synthesis API.
 *
 * Threading model note: all calls from C++ must happen on the UI thread.
 * Callbacks from Android may happen on a different thread, so we always
 * use ThreadUtils.runOnUiThread when calling back to C++.
 */
class TtsPlatformImpl {
    private static class TtsVoice {
        private TtsVoice(String name, String language) {
            mName = name;
            mLanguage = language;
        }
        private final String mName;
        private final String mLanguage;
    }

    private static class PendingUtterance {
        private PendingUtterance(TtsPlatformImpl impl, int utteranceId, String text,
                String lang, float rate, float pitch, float volume) {
            mImpl = impl;
            mUtteranceId = utteranceId;
            mText = text;
            mLang = lang;
            mRate = rate;
            mPitch = pitch;
            mVolume = volume;
        }

        private void speak() {
            mImpl.speak(mUtteranceId, mText, mLang, mRate, mPitch, mVolume);
        }

        TtsPlatformImpl mImpl;
        int mUtteranceId;
        String mText;
        String mLang;
        float mRate;
        float mPitch;
        float mVolume;
    }

    private long mNativeTtsPlatformImplAndroid;
    private final TextToSpeech mTextToSpeech;
    private boolean mInitialized;
    private ArrayList<TtsVoice> mVoices;
    private String mCurrentLanguage;
    private PendingUtterance mPendingUtterance;

    private TtsPlatformImpl(long nativeTtsPlatformImplAndroid, Context context) {
        mInitialized = false;
        mNativeTtsPlatformImplAndroid = nativeTtsPlatformImplAndroid;
        mTextToSpeech = new TextToSpeech(context, new TextToSpeech.OnInitListener() {
                @Override
                public void onInit(int status) {
                    if (status == TextToSpeech.SUCCESS) {
                        ThreadUtils.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                initialize();
                            }
                        });
                    }
                }
            });
        mTextToSpeech.setOnUtteranceProgressListener(new UtteranceProgressListener() {
                @Override
                public void onDone(final String utteranceId) {
                    ThreadUtils.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (mNativeTtsPlatformImplAndroid != 0) {
                                nativeOnEndEvent(mNativeTtsPlatformImplAndroid,
                                                 Integer.parseInt(utteranceId));
                            }
                        }
                    });
                }

                @Override
                public void onError(final String utteranceId) {
                    ThreadUtils.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (mNativeTtsPlatformImplAndroid != 0) {
                                nativeOnErrorEvent(mNativeTtsPlatformImplAndroid,
                                                   Integer.parseInt(utteranceId));
                            }
                        }
                    });
                }

                @Override
                public void onStart(final String utteranceId) {
                    ThreadUtils.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (mNativeTtsPlatformImplAndroid != 0) {
                                nativeOnStartEvent(mNativeTtsPlatformImplAndroid,
                                                   Integer.parseInt(utteranceId));
                            }
                        }
                    });
                }
            });
    }

    /**
     * Create a TtsPlatformImpl object, which is owned by TtsPlatformImplAndroid
     * on the C++ side.
     *
     * @param nativeTtsPlatformImplAndroid The C++ object that owns us.
     * @param context The app context.
     */
    @CalledByNative
    private static TtsPlatformImpl create(long nativeTtsPlatformImplAndroid,
                                          Context context) {
        return new TtsPlatformImpl(nativeTtsPlatformImplAndroid, context);
    }

    /**
     * Called when our C++ counterpoint is deleted. Clear the handle to our
     * native C++ object, ensuring it's never called.
     */
    @CalledByNative
    private void destroy() {
        mNativeTtsPlatformImplAndroid = 0;
    }

    /**
     * @return true if our TextToSpeech object is initialized and we've
     * finished scanning the list of voices.
     */
    @CalledByNative
    private boolean isInitialized() {
        return mInitialized;
    }

    /**
     * @return the number of voices.
     */
    @CalledByNative
    private int getVoiceCount() {
        assert mInitialized == true;
        return mVoices.size();
    }

    /**
     * @return the name of the voice at a given index.
     */
    @CalledByNative
    private String getVoiceName(int voiceIndex) {
        assert mInitialized == true;
        return mVoices.get(voiceIndex).mName;
    }

    /**
     * @return the language of the voice at a given index.
     */
    @CalledByNative
    private String getVoiceLanguage(int voiceIndex) {
        assert mInitialized == true;
        return mVoices.get(voiceIndex).mLanguage;
    }

    /**
     * Attempt to start speaking an utterance. If it returns true, will call back on
     * start and end.
     *
     * @param utteranceId A unique id for this utterance so that callbacks can be tied
     *     to a particular utterance.
     * @param text The text to speak.
     * @param lang The language code for the text (e.g., "en-US").
     * @param rate The speech rate, in the units expected by Android TextToSpeech.
     * @param pitch The speech pitch, in the units expected by Android TextToSpeech.
     * @param volume The speech volume, in the units expected by Android TextToSpeech.
     * @return true on success.
     */
    @CalledByNative
    private boolean speak(int utteranceId, String text, String lang,
                          float rate, float pitch, float volume) {
        if (!mInitialized) {
            mPendingUtterance = new PendingUtterance(this, utteranceId, text, lang, rate,
                    pitch, volume);
            return true;
        }
        if (mPendingUtterance != null) mPendingUtterance = null;

        if (!lang.equals(mCurrentLanguage)) {
            mTextToSpeech.setLanguage(new Locale(lang));
            mCurrentLanguage = lang;
        }

        mTextToSpeech.setSpeechRate(rate);
        mTextToSpeech.setPitch(pitch);
        HashMap<String, String> params = new HashMap<String, String>();
        if (volume != 1.0) {
            params.put(TextToSpeech.Engine.KEY_PARAM_VOLUME, Double.toString(volume));
        }
        params.put(TextToSpeech.Engine.KEY_PARAM_UTTERANCE_ID, Integer.toString(utteranceId));
        int result = mTextToSpeech.speak(text, TextToSpeech.QUEUE_FLUSH, params);
        return (result == TextToSpeech.SUCCESS);
    }

    /**
     * Stop the current utterance.
     */
    @CalledByNative
    private void stop() {
        if (mInitialized) mTextToSpeech.stop();
        if (mPendingUtterance != null) mPendingUtterance = null;
    }

    /**
     * Note: we enforce that this method is called on the UI thread, so
     * we can call nativeVoicesChanged directly.
     */
    private void initialize() {
        assert mNativeTtsPlatformImplAndroid != 0;

        // Note: Android supports multiple speech engines, but querying the
        // metadata about all of them is expensive. So we deliberately only
        // support the default speech engine, and expose the different
        // supported languages for the default engine as different voices.
        String defaultEngineName = mTextToSpeech.getDefaultEngine();
        String engineLabel = defaultEngineName;
        for (TextToSpeech.EngineInfo info : mTextToSpeech.getEngines()) {
            if (info.name.equals(defaultEngineName)) engineLabel = info.label;
        }
        Locale[] locales = Locale.getAvailableLocales();
        mVoices = new ArrayList<TtsVoice>();
        for (int i = 0; i < locales.length; ++i) {
            if (!locales[i].getVariant().isEmpty()) continue;
            if (mTextToSpeech.isLanguageAvailable(locales[i]) > 0) {
                String name = locales[i].getDisplayLanguage();
                if (!locales[i].getCountry().isEmpty()) {
                    name += " " + locales[i].getDisplayCountry();
                }
                TtsVoice voice = new TtsVoice(name, locales[i].toString());
                mVoices.add(voice);
            }
        }

        mInitialized = true;
        nativeVoicesChanged(mNativeTtsPlatformImplAndroid);

        if (mPendingUtterance != null) mPendingUtterance.speak();
    }

    private native void nativeVoicesChanged(long nativeTtsPlatformImplAndroid);
    private native void nativeOnEndEvent(long nativeTtsPlatformImplAndroid, int utteranceId);
    private native void nativeOnStartEvent(long nativeTtsPlatformImplAndroid, int utteranceId);
    private native void nativeOnErrorEvent(long nativeTtsPlatformImplAndroid, int utteranceId);
}
