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
    /**
     * A client for this coordinator.
     */
    public interface Delegate {
        /**
         * Called when the feedback button is clicked.
         */
        void onFeedbackClicked();

        /**
         * Called when the close button is clicked.
         */
        void onCloseClicked();
    }

    private final Delegate mDelegate;
    private final AssistantHeaderMediator mMediator;

    public AssistantHeaderCoordinator(Context context, View root, Delegate client) {
        mDelegate = client;

        // Bind view and mediator through the model.
        AssistantHeaderModel model = new AssistantHeaderModel();
        AssistantHeaderViewBinder.ViewHolder viewHolder =
                new AssistantHeaderViewBinder.ViewHolder(context, root);
        AssistantHeaderViewBinder viewBinder = new AssistantHeaderViewBinder();
        PropertyModelChangeProcessor.create(model, viewHolder, viewBinder);
        mMediator = new AssistantHeaderMediator(model);

        // Notify the client when header buttons are clicked.
        viewHolder.mFeedbackButton.setOnClickListener(unusedView -> mDelegate.onFeedbackClicked());
        viewHolder.mCloseButton.setOnClickListener(unusedView -> mDelegate.onCloseClicked());
    }

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

    /**
     * Set the progress bar value to {@code progress}. If it is smaller than a previously set
     * progress, do nothing.
     */
    public void setProgress(int progress) {
        mMediator.setProgress(progress);
    }

    /**
     * Make the progress bar pulse, to show that a user interaction is expected (either with the web
     * page or the Autofill Assistant).
     */
    public void enableProgressBarPulsing() {
        mMediator.enableProgressBarPulsing();
    }

    /**
     * Stop the progress bar pulsing animation.
     */
    public void disableProgressBarPulsing() {
        mMediator.disableProgressBarPulsing();
    }
}
