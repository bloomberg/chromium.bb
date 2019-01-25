// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

/**
 * The main coordinator for the Autofill Assistant, responsible for instantiating all other
 * sub-components and shutting down the Autofill Assistant.
 */
class AssistantCoordinator implements TouchEventFilterView.Delegate {
    interface Delegate {
        /**
         * Completely stop the Autofill Assistant.
         */
        void stop();

        /**
         * Asks for an update of the touchable area.
         */
        void updateTouchableArea();

        /**
         * Called when interaction within allowed touchable area was detected. The interaction
         * could be any gesture.
         */
        void onUserInteractionInsideTouchableArea();
    }

    private static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";
    private static final int SNACKBAR_DELAY_MS = 5_000;
    private static final int GRACEFUL_SHUTDOWN_DELAY_MS = 5_000;

    private final ChromeActivity mActivity;
    private final Delegate mDelegate;

    private final AssistantModel mModel;
    private final View mAssistantView;

    private final AssistantBottomBarCoordinator mBottomBarCoordinator;
    private final AssistantKeyboardCoordinator mKeyboardCoordinator;
    private final AssistantOverlayCoordinator mOverlayCoordinator;

    private boolean mIsShuttingDownGracefully;

    AssistantCoordinator(ChromeActivity activity, Delegate delegate) {
        mActivity = activity;
        mDelegate = delegate;
        mModel = new AssistantModel();

        // Inflate autofill_assistant_sheet layout and add it to the main coordinator view.
        ViewGroup coordinator = activity.findViewById(R.id.coordinator);
        mAssistantView = activity.getLayoutInflater()
                                 .inflate(R.layout.autofill_assistant_sheet, coordinator)
                                 .findViewById(R.id.autofill_assistant);

        // Instantiate child components.
        mBottomBarCoordinator =
                new AssistantBottomBarCoordinator(mActivity, mAssistantView, mModel);
        mKeyboardCoordinator = new AssistantKeyboardCoordinator(activity);
        mOverlayCoordinator = new AssistantOverlayCoordinator(activity, mAssistantView, this);

        showAssistantView();
    }

    /**
     * Shut down the Autofill Assistant immediately, without showing a message.
     */
    public void shutdownImmediately() {
        detachAssistantView();
        mOverlayCoordinator.destroy();
        mDelegate.stop();
    }

    /**
     * Schedule the shut down of the Autofill Assistant such that any status message currently
     * visible will still be shown for a few seconds before shutting down. Optionally replace
     * the status message with a generic error message iff {@code showGiveUpMessage} is true.
     */
    // TODO(crbug.com/806868): Move this method to native.
    public void gracefulShutdown(boolean showGiveUpMessage) {
        mIsShuttingDownGracefully = true;

        // Make sure bottom bar is expanded.
        mBottomBarCoordinator.expand();

        // Hide everything except header.
        mOverlayCoordinator.hide();
        mModel.getDetailsModel().clearDetails();
        mBottomBarCoordinator.getPaymentRequestCoordinator().setVisible(false);
        mModel.getCarouselModel().clearChips();

        if (showGiveUpMessage) {
            mModel.getHeaderModel().set(AssistantHeaderModel.STATUS_MESSAGE,
                    mActivity.getString(R.string.autofill_assistant_give_up));
        }
        ThreadUtils.postOnUiThreadDelayed(this::shutdownImmediately, GRACEFUL_SHUTDOWN_DELAY_MS);
    }

    /**
     * Shut down the Autofill Assistant and close the current Chrome tab.
     */
    public void close() {
        shutdownImmediately();
        mActivity.finish();
    }

    /**
     * Show the onboarding screen and call {@code onAccept} if the user agreed to proceed, shutdown
     * otherwise.
     */
    public void showOnboarding(Runnable onAccept) {
        // Hide header buttons.
        mModel.getHeaderModel().set(AssistantHeaderModel.FEEDBACK_VISIBLE, false);
        mModel.getHeaderModel().set(AssistantHeaderModel.CLOSE_VISIBLE, false);

        // Show overlay to prevent user from interacting with the page during onboarding.
        mOverlayCoordinator.showFullOverlay();

        AssistantOnboardingCoordinator.show(mActivity, mBottomBarCoordinator.getView())
                .then(accepted -> {
                    if (!accepted) {
                        shutdownImmediately();
                        return;
                    }

                    // Show header buttons.
                    mModel.getHeaderModel().set(AssistantHeaderModel.FEEDBACK_VISIBLE, true);
                    mModel.getHeaderModel().set(AssistantHeaderModel.CLOSE_VISIBLE, true);

                    // Hide overlay.
                    mOverlayCoordinator.hide();

                    onAccept.run();
                });
    }

    /**
     * Get the model representing the current state of the UI.
     */

    public AssistantModel getModel() {
        return mModel;
    }

    // Getters to retrieve the sub coordinators.

    public AssistantBottomBarCoordinator getBottomBarCoordinator() {
        return mBottomBarCoordinator;
    }

    public AssistantKeyboardCoordinator getKeyboardCoordinator() {
        return mKeyboardCoordinator;
    }

    public AssistantOverlayCoordinator getOverlayCoordinator() {
        return mOverlayCoordinator;
    }

    /**
     * Dismiss the assistant view and show a cancellable snackbar alerting the user that the
     * Autofill assistant is shutting down.
     */
    public void dismissAndShowSnackbar(String message) {
        if (mIsShuttingDownGracefully) {
            shutdownImmediately();
            return;
        }

        hideAssistantView();

        Snackbar snackBar =
                Snackbar.make(message,
                                new SnackbarManager.SnackbarController() {
                                    @Override
                                    public void onAction(Object actionData) {
                                        // Shutdown was cancelled.
                                        showAssistantView();
                                    }

                                    @Override
                                    public void onDismissNoAction(Object actionData) {
                                        shutdownImmediately();
                                    }
                                },
                                Snackbar.TYPE_ACTION, Snackbar.UMA_AUTOFILL_ASSISTANT_STOP_UNDO)
                        .setAction(mActivity.getString(R.string.undo), /* actionData= */ null);
        snackBar.setSingleLine(false);
        snackBar.setDuration(SNACKBAR_DELAY_MS);
        mActivity.getSnackbarManager().showSnackbar(snackBar);
    }

    private void dismissAndShowSnackbar(int message) {
        dismissAndShowSnackbar(mActivity.getString(message));
    }

    /**
     * Show the Chrome feedback form.
     */
    public void showFeedback(String debugContext) {
        HelpAndFeedback.getInstance(mActivity).showFeedback(mActivity, Profile.getLastUsedProfile(),
                mActivity.getActivityTab().getUrl(), FEEDBACK_CATEGORY_TAG,
                FeedbackContext.buildContextString(mActivity, debugContext, 4));
    }

    // Implementation of methods from {@link TouchEventFilterView.Delegate}.

    @Override
    public void onUnexpectedTaps() {
        dismissAndShowSnackbar(R.string.autofill_assistant_maybe_give_up);
    }

    @Override
    public void updateTouchableArea() {
        mDelegate.updateTouchableArea();
    }

    @Override
    public void onUserInteractionInsideTouchableArea() {
        mDelegate.onUserInteractionInsideTouchableArea();
    }

    // Private methods.

    private void showAssistantView() {
        mAssistantView.setVisibility(View.VISIBLE);
        mKeyboardCoordinator.enableListenForKeyboardVisibility(true);

        mBottomBarCoordinator.expand();
        mBottomBarCoordinator.getView().announceForAccessibility(
                mActivity.getString(R.string.autofill_assistant_available_accessibility));
    }

    private void hideAssistantView() {
        mAssistantView.setVisibility(View.GONE);
        mKeyboardCoordinator.enableListenForKeyboardVisibility(false);
    }

    private void detachAssistantView() {
        mActivity.<ViewGroup>findViewById(R.id.coordinator).removeView(mAssistantView);
    }
}
