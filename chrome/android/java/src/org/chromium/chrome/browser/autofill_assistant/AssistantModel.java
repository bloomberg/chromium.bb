// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;

/**
 * State for the Autofill Assistant UI.
 */
@JNINamespace("autofill_assistant")
class AssistantModel {
    private final AssistantHeaderModel mHeaderModel = new AssistantHeaderModel();

    @CalledByNative
    public AssistantHeaderModel getHeaderModel() {
        return mHeaderModel;
    }
}
