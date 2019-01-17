// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * State for the header of the Autofill Assistant.
 */
class AssistantHeaderModel extends PropertyModel {
    static final WritableObjectPropertyKey<String> STATUS_MESSAGE =
            new WritableObjectPropertyKey<>();

    static final WritableBooleanPropertyKey FEEDBACK_VISIBLE = new WritableBooleanPropertyKey();

    static final WritableBooleanPropertyKey CLOSE_VISIBLE = new WritableBooleanPropertyKey();

    static final WritableIntPropertyKey PROGRESS = new WritableIntPropertyKey();

    static final WritableBooleanPropertyKey PROGRESS_PULSING = new WritableBooleanPropertyKey();

    AssistantHeaderModel() {
        super(STATUS_MESSAGE, FEEDBACK_VISIBLE, CLOSE_VISIBLE, PROGRESS, PROGRESS_PULSING);
    }
}
