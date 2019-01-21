// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

/**
 * Mediator that updates the model for the Autofill Assistant header.
 */
class AssistantHeaderMediator {
    private final AssistantHeaderModel mModel;

    AssistantHeaderMediator(AssistantHeaderModel model) {
        mModel = model;
    }

    String getCurrentStatusMessage() {
        return mModel.get(AssistantHeaderModel.STATUS_MESSAGE);
    }

    void setStatusMessage(String message) {
        if (message != null && !message.isEmpty()) {
            mModel.set(AssistantHeaderModel.STATUS_MESSAGE, message);
        }
    }

    void setFeedbackButtonVisible(boolean visible) {
        mModel.set(AssistantHeaderModel.FEEDBACK_VISIBLE, visible);
    }

    void setCloseButtonVisible(boolean visible) {
        mModel.set(AssistantHeaderModel.CLOSE_VISIBLE, visible);
    }

    void enableProgressBarPulsing() {
        mModel.set(AssistantHeaderModel.PROGRESS_PULSING, true);
    }

    void disableProgressBarPulsing() {
        mModel.set(AssistantHeaderModel.PROGRESS_PULSING, false);
    }
}
