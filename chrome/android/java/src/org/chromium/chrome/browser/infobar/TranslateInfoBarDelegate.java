// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import org.chromium.base.CalledByNative;

/**
 * Translate JNI methods
 */
public class TranslateInfoBarDelegate {
    private TranslateInfoBar mInfoBar;

    private TranslateInfoBarDelegate() {}

    @CalledByNative
    public static TranslateInfoBarDelegate create() {
        return new TranslateInfoBarDelegate();
    }

    @CalledByNative
    boolean changeTranslateInfoBarTypeAndPointer(
            long newNativeInfoBar, int translateBarType) {
        mInfoBar.changeInfoBarTypeAndNativePointer(
                translateBarType, newNativeInfoBar);
        return true;
    }

    @CalledByNative
    InfoBar showTranslateInfoBar(
            long nativeInfoBar, int translateBarType,
            int sourceLanguageIndex, int targetLanguageIndex, boolean autoTranslatePair,
            boolean showNeverInfobar, boolean triggeredFromMenu,
            String[] languages) {
        mInfoBar = new TranslateInfoBar(nativeInfoBar, this, translateBarType,
                sourceLanguageIndex, targetLanguageIndex, autoTranslatePair, showNeverInfobar,
                triggeredFromMenu, languages);
        return mInfoBar;
    }

    public void applyTranslateOptions(long nativeTranslateInfoBar,
            int sourceLanguageIndex, int targetLanguageIndex, boolean alwaysTranslate,
            boolean neverTranslateLanguage, boolean neverTranslateSite) {
        nativeApplyTranslateOptions(nativeTranslateInfoBar, sourceLanguageIndex,
                targetLanguageIndex, alwaysTranslate, neverTranslateLanguage, neverTranslateSite);
    }

    private native void nativeApplyTranslateOptions(long nativeTranslateInfoBar,
            int sourceLanguageIndex, int targetLanguageIndex, boolean alwaysTranslate,
            boolean neverTranslateLanguage, boolean neverTranslateSite);
}
