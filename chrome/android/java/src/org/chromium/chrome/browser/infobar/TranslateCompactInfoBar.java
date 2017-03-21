// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.chrome.browser.infobar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;

/**
 * Java version of the compcat translate infobar
 */
class TranslateCompactInfoBar extends InfoBar {
    private long mNativeTranslateInfoBarPtr;

    @CalledByNative
    private static InfoBar create() {
        return new TranslateCompactInfoBar();
    }

    TranslateCompactInfoBar() {
        super(R.drawable.infobar_translate, null, null);
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        // TODO(googleo): Add custom view later.
        layout.setMessage("Compact Translate Infobar Testing...");
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {
        if (mNativeTranslateInfoBarPtr == 0) return;

        // TODO(googleo): Handle the button click on this infobar.
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        mNativeTranslateInfoBarPtr = nativePtr;
    }

    @Override
    protected void onNativeDestroyed() {
        mNativeTranslateInfoBarPtr = 0;
        super.onNativeDestroyed();
    }
}
