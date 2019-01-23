// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("autofill_assistant")
class AssistantHeaderDelegate {
    private long mNativeAssistantHeaderDelegate;

    @CalledByNative
    private static AssistantHeaderDelegate create(long nativeAssistantHeaderDelegate) {
        return new AssistantHeaderDelegate(nativeAssistantHeaderDelegate);
    }

    private AssistantHeaderDelegate(long nativeAssistantHeaderDelegate) {
        mNativeAssistantHeaderDelegate = nativeAssistantHeaderDelegate;
    }

    void onFeedbackButtonClicked() {
        if (mNativeAssistantHeaderDelegate != 0) {
            nativeOnFeedbackButtonClicked(mNativeAssistantHeaderDelegate);
        }
    }

    void onCloseButtonClicked() {
        if (mNativeAssistantHeaderDelegate != 0) {
            nativeOnCloseButtonClicked(mNativeAssistantHeaderDelegate);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantHeaderDelegate = 0;
    }

    private native void nativeOnFeedbackButtonClicked(long nativeAssistantHeaderDelegate);
    private native void nativeOnCloseButtonClicked(long nativeAssistantHeaderDelegate);
}
