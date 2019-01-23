// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import android.content.Context;
import android.view.View;

import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the header of the Autofill Assistant.
 */
public class AssistantHeaderCoordinator {
    private final AssistantHeaderMediator mMediator;

    public AssistantHeaderCoordinator(Context context, View root, AssistantHeaderModel model) {
        // Bind view and mediator through the model.
        AssistantHeaderViewBinder.ViewHolder viewHolder =
                new AssistantHeaderViewBinder.ViewHolder(context, root);
        AssistantHeaderViewBinder viewBinder = new AssistantHeaderViewBinder();
        PropertyModelChangeProcessor.create(model, viewHolder, viewBinder);
        mMediator = new AssistantHeaderMediator(model);
    }

    // TODO(crbug.com/806868): Remove all methods here and delete mediator once UI is modified only
    // in native side.

    /**
     * Get the current status message.
     */
    public String getStatusMessage() {
        return mMediator.getCurrentStatusMessage();
    }

    /**
     * Set the status message.
     */
    public void setStatusMessage(String message) {
        mMediator.setStatusMessage(message);
    }

    /**
     * Show or hide the feedback button.
     */
    public void setFeedbackButtonVisible(boolean visible) {
        mMediator.setFeedbackButtonVisible(visible);
    }

    /**
     * Show or hide the close button.
     */
    public void setCloseButtonVisible(boolean visible) {
        mMediator.setCloseButtonVisible(visible);
    }
}
