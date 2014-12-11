// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.os.Handler;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.ui.autofill.CardUnmaskPrompt;
import org.chromium.ui.autofill.CardUnmaskPrompt.CardUnmaskPromptDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
* JNI call glue for CardUnmaskPrompt C++ and Java objects.
*/
@JNINamespace("autofill")
public class CardUnmaskBridge implements CardUnmaskPromptDelegate {
    private final long mNativeCardUnmaskPromptViewAndroid;
    private final CardUnmaskPrompt mCardUnmaskPrompt;

    public CardUnmaskBridge(long nativeCardUnmaskPromptViewAndroid, WindowAndroid windowAndroid) {
        mNativeCardUnmaskPromptViewAndroid = nativeCardUnmaskPromptViewAndroid;
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            mCardUnmaskPrompt = null;
            // Clean up the native counterpart.  This is posted to allow the native counterpart
            // to fully finish the construction of this glue object before we attempt to delete it.
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    dismissed("");
                }
            });
        } else {
            mCardUnmaskPrompt = new CardUnmaskPrompt(activity, this);
        }
    }

    @CalledByNative
    private static CardUnmaskBridge create(long nativeUnmaskPrompt,
            WindowAndroid windowAndroid) {
        return new CardUnmaskBridge(nativeUnmaskPrompt, windowAndroid);
    }

    @Override
    public void dismissed(String userResponse) {
        nativePromptDismissed(mNativeCardUnmaskPromptViewAndroid, userResponse);
    }

    /**
     * Shows a prompt for unmasking a Wallet credit card.
     * @param suggestions Autofill suggestions to be displayed.
     */
    @CalledByNative
    private void show() {
        if (mCardUnmaskPrompt != null) mCardUnmaskPrompt.show();
    }

    private native void nativePromptDismissed(long nativeCardUnmaskPromptViewAndroid,
            String userResponse);
}
