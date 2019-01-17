// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetails;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderCoordinator;
import org.chromium.chrome.browser.autofill_assistant.payment.AssistantPaymentRequestCoordinator;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

import java.util.Arrays;

/**
 * The main coordinator for the Autofill Assistant, responsible for instantiating all other
 * sub-components and shutting down the Autofill Assistant.
 */
class AssistantCoordinator
        implements AssistantHeaderCoordinator.Delegate, TouchEventFilterView.Delegate {
    interface Delegate {
        /**
         * Completely stop the Autofill Assistant.
         */
        void stop();

        /**
         * Get the debug context used when submitting feedback.
         */
        String getDebugContext();

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

    private final View mAssistantView;

    private final AssistantBottomBarCoordinator mBottomBarCoordinator;
    private final AssistantHeaderCoordinator mHeaderCoordinator;
    private final AssistantDetailsCoordinator mDetailsCoordinator;
    private final AssistantPaymentRequestCoordinator mPaymentRequestCoordinator;
    private final AssistantCarouselCoordinator mCarouselCoordinator;
    private final AssistantKeyboardCoordinator mKeyboardCoordinator;
    private final AssistantOverlayCoordinator mOverlayCoordinator;

    private boolean mIsShuttingDownGracefully;

    AssistantCoordinator(ChromeActivity activity, Delegate delegate) {
        mActivity = activity;
        mDelegate = delegate;

        // Inflate autofill_assistant_sheet layout and add it to the main coordinator view.
        ViewGroup coordinator = activity.findViewById(R.id.coordinator);
        mAssistantView = activity.getLayoutInflater()
                                 .inflate(R.layout.autofill_assistant_sheet, coordinator)
                                 .findViewById(R.id.autofill_assistant);

        // Instantiate child components.
        mBottomBarCoordinator = new AssistantBottomBarCoordinator(
                mAssistantView, mActivity.getResources().getDisplayMetrics());
        mHeaderCoordinator =
                new AssistantHeaderCoordinator(mActivity, mBottomBarCoordinator.getView(), this);
        mCarouselCoordinator = new AssistantCarouselCoordinator(
                mActivity, mBottomBarCoordinator::onChildVisibilityChanged);
        mDetailsCoordinator = new AssistantDetailsCoordinator(
                mActivity, mBottomBarCoordinator::onChildVisibilityChanged);
        mPaymentRequestCoordinator = new AssistantPaymentRequestCoordinator(
                mActivity, mBottomBarCoordinator::onChildVisibilityChanged);
        mKeyboardCoordinator = new AssistantKeyboardCoordinator(activity);
        mOverlayCoordinator = new AssistantOverlayCoordinator(activity, mAssistantView, this);

        // Attach child views to the bottom bar.
        mBottomBarCoordinator.setDetailsView(mDetailsCoordinator.getView());
        mBottomBarCoordinator.setPaymentRequestView(mPaymentRequestCoordinator.getView());
        mBottomBarCoordinator.setCarouselView(mCarouselCoordinator.getView());

        // Details, PR and carousel are initially hidden.
        mDetailsCoordinator.setVisible(false);
        mPaymentRequestCoordinator.setVisible(false);
        mCarouselCoordinator.setVisible(false);

        showAssistantView();

        // TODO(crbug.com/806868): Keep details on the native side and get rid of this duplicated,
        // misplaced, call to extractParameters.
        AssistantDetails initialDetails = AssistantDetails.makeFromParameters(
                AutofillAssistantFacade.extractParameters(activity.getInitialIntent().getExtras()));
        if (initialDetails != null) {
            mDetailsCoordinator.showDetails(initialDetails);
        }
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
    public void gracefulShutdown(boolean showGiveUpMessage) {
        mIsShuttingDownGracefully = true;

        // Make sure bottom bar is expanded.
        mBottomBarCoordinator.expand();

        // Hide everything except header.
        mOverlayCoordinator.hide();
        mDetailsCoordinator.setVisible(false);
        mPaymentRequestCoordinator.setVisible(false);
        mCarouselCoordinator.setVisible(false);

        if (showGiveUpMessage) {
            mHeaderCoordinator.setStatusMessage(
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
        mHeaderCoordinator.setFeedbackButtonVisible(false);
        mHeaderCoordinator.setCloseButtonVisible(false);

        // Show overlay to prevent user from interacting with the page during onboarding.
        mOverlayCoordinator.showFullOverlay();

        // TODO(crbug.com/806868): Remove this hack and call setDetails with initial details from
        // native once onboarding is accepted or skipped instead.
        boolean detailsVisible = mDetailsCoordinator.isVisible();
        mDetailsCoordinator.setVisible(false);

        AssistantOnboardingCoordinator.show(mActivity, mBottomBarCoordinator.getView())
                .then(accepted -> {
                    if (!accepted) {
                        mDelegate.stop();
                        return;
                    }

                    // Show header buttons.
                    mHeaderCoordinator.setFeedbackButtonVisible(true);
                    mHeaderCoordinator.setCloseButtonVisible(true);

                    // Hide overlay.
                    // TODO(crbug.com/806868): Maybe it would be better to save the state of the
                    // overlay before showing the onboarding and restore it here.
                    mOverlayCoordinator.hide();

                    // TODO(crbug.com/806868): Remove this.
                    if (detailsVisible) {
                        mDetailsCoordinator.setVisible(true);
                    }

                    onAccept.run();
                });
    }

    /**
     * Update the details shown to the user and ask the user if we should continue in case approval
     * is required. Return {@code true} if no approval was required or if user agreed to proceed,
     * {@code false} otherwise.
     */
    // TODO(crbug.com/806868): Remove that logic to native side.
    public Promise<Boolean> showDetailsForApproval(AssistantDetails details) {
        mDetailsCoordinator.showDetails(details);

        if (details.getUserApprovalRequired()) {
            return askForUserApproval(details);
        }

        return Promise.fulfilled(true);
    }

    private Promise<Boolean> askForUserApproval(AssistantDetails details) {
        Promise<Boolean> promise = new Promise<>();

        String oldStatusMessage = mHeaderCoordinator.getStatusMessage();
        mHeaderCoordinator.setStatusMessage(
                mActivity.getString(R.string.autofill_assistant_details_differ));
        mHeaderCoordinator.enableProgressBarPulsing();
        mCarouselCoordinator.setChips(Arrays.asList(
                new AssistantChip(AssistantChip.TYPE_BUTTON_FILLED_BLUE,
                        mActivity.getString(R.string.continue_button),
                        () -> {
                            mHeaderCoordinator.setStatusMessage(oldStatusMessage);
                            mHeaderCoordinator.disableProgressBarPulsing();
                            mCarouselCoordinator.clearChips();

                            // Reset the styles.
                            details.clearChangedFlags();
                            mDetailsCoordinator.showDetails(details);

                            promise.fulfill(true);
                        }),
                new AssistantChip(AssistantChip.TYPE_BUTTON_TEXT,
                        mActivity.getString(R.string.autofill_assistant_details_differ_go_back),
                        () -> promise.fulfill(false))));

        return promise;
    }

    /**
     * Show {@code message} to the user, unless we are shutting down.
     */
    public void showStatusMessage(String message) {
        if (!mIsShuttingDownGracefully) {
            mHeaderCoordinator.setStatusMessage(message);
        }
    }

    // Getters to retrieve the sub coordinators.

    public AssistantBottomBarCoordinator getBottomBarCoordinator() {
        return mBottomBarCoordinator;
    }

    public AssistantHeaderCoordinator getHeaderCoordinator() {
        return mHeaderCoordinator;
    }

    public AssistantDetailsCoordinator getDetailsCoordinator() {
        return mDetailsCoordinator;
    }

    public AssistantPaymentRequestCoordinator getPaymentRequestCoordinator() {
        return mPaymentRequestCoordinator;
    }

    public AssistantCarouselCoordinator getCarouselCoordinator() {
        return mCarouselCoordinator;
    }

    public AssistantKeyboardCoordinator getKeyboardCoordinator() {
        return mKeyboardCoordinator;
    }

    public AssistantOverlayCoordinator getOverlayCoordinator() {
        return mOverlayCoordinator;
    }

    // Implementation of methods from {@link AssistantHeaderCoordinator.Delegate}.

    @Override
    public void onCloseClicked() {
        if (mIsShuttingDownGracefully) {
            shutdownImmediately();
            return;
        }

        dismissAndShowSnackbar(R.string.autofill_assistant_stopped);
    }

    private void dismissAndShowSnackbar(int message) {
        hideAssistantView();

        Snackbar snackBar =
                Snackbar.make(mActivity.getString(message),
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

    @Override
    public void onFeedbackClicked() {
        HelpAndFeedback.getInstance(mActivity).showFeedback(mActivity, Profile.getLastUsedProfile(),
                mActivity.getActivityTab().getUrl(), FEEDBACK_CATEGORY_TAG,
                FeedbackContext.buildContextString(mActivity, mDelegate.getDebugContext(),
                        mDetailsCoordinator.getCurrentDetails(),
                        mHeaderCoordinator.getStatusMessage(), 4));
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
