// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.os.Handler;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt.CardUnmaskPromptDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
* JNI call glue for CardUnmaskPrompt C++ and Java objects.
*/
@JNINamespace("autofill")
public class CardUnmaskBridge implements CardUnmaskPromptDelegate {
    private final long mNativeCardUnmaskPromptViewAndroid;
    private final CardUnmaskPrompt mCardUnmaskPrompt;

    public CardUnmaskBridge(long nativeCardUnmaskPromptViewAndroid, String title,
            String instructions, int iconId, boolean shouldRequestExpirationDate,
            boolean defaultToStoringLocally, WindowAndroid windowAndroid) {
        mNativeCardUnmaskPromptViewAndroid = nativeCardUnmaskPromptViewAndroid;
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            mCardUnmaskPrompt = null;
            // Clean up the native counterpart.  This is posted to allow the native counterpart
            // to fully finish the construction of this glue object before we attempt to delete it.
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    dismissed();
                }
            });
        } else {
            mCardUnmaskPrompt = new CardUnmaskPrompt(activity, this, title, instructions,
                    ResourceId.mapToDrawableId(iconId), shouldRequestExpirationDate,
                    defaultToStoringLocally);
        }
    }

    @CalledByNative
    private static CardUnmaskBridge create(long nativeUnmaskPrompt, String title,
            String instructions, int iconId, boolean shouldRequestExpirationDate,
            boolean defaultToStoringLocally, WindowAndroid windowAndroid) {
        return new CardUnmaskBridge(nativeUnmaskPrompt, title, instructions, iconId,
                shouldRequestExpirationDate, defaultToStoringLocally, windowAndroid);
    }

    @Override
    public void dismissed() {
        nativePromptDismissed(mNativeCardUnmaskPromptViewAndroid);
    }

    @Override
    public boolean checkUserInputValidity(String userResponse) {
        return nativeCheckUserInputValidity(mNativeCardUnmaskPromptViewAndroid, userResponse);
    }

    @Override
    public void onUserInput(String cvc, String month, String year, boolean shouldStoreLocally) {
        nativeOnUserInput(mNativeCardUnmaskPromptViewAndroid, cvc, month, year, shouldStoreLocally);
    }

    /**
     * Shows a prompt for unmasking a Wallet credit card.
     */
    @CalledByNative
    private void show() {
        if (mCardUnmaskPrompt != null) mCardUnmaskPrompt.show();
    }

    /**
     * Dismisses the prompt without returning any user response.
     */
    @CalledByNative
    private void dismiss() {
        if (mCardUnmaskPrompt != null) mCardUnmaskPrompt.dismiss();
    }

    /**
     * Disables input and visually indicates that verification is ongoing.
     */
    @CalledByNative
    private void disableAndWaitForVerification() {
        if (mCardUnmaskPrompt != null) mCardUnmaskPrompt.disableAndWaitForVerification();
    }

    /**
     * Indicate that verification failed, allow user to retry.
     * @param errorMessage The error to display, or null to signal success.
     * @param allowRetry If there was an error, indicates whether to allow another attempt.
     */
    @CalledByNative
    private void verificationFinished(String errorMessage, boolean allowRetry) {
        if (mCardUnmaskPrompt != null) {
            mCardUnmaskPrompt.verificationFinished(errorMessage, allowRetry);
        }
    }

    private native void nativePromptDismissed(long nativeCardUnmaskPromptViewAndroid);
    private native boolean nativeCheckUserInputValidity(
            long nativeCardUnmaskPromptViewAndroid, String userResponse);
    private native void nativeOnUserInput(
            long nativeCardUnmaskPromptViewAndroid, String cvc, String month, String year,
            boolean shouldStoreLocally);
}
