// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

import java.util.ArrayDeque;
import java.util.Queue;

/**
 * Class holder for the AutofillAssistantUiDelegate to make sure we don't make UI changes when
 * we are in a pause state (i.e. few seconds before stopping completely).
 */
class UiDelegateHolder {
    /** Display the final message for that long before shutting everything down. */
    private static final long GRACEFUL_SHUTDOWN_DELAY_MS = 5_000;

    private final AutofillAssistantUiController mUiController;
    private final AutofillAssistantUiDelegate mUiDelegate;

    private boolean mPaused;
    private boolean mHasBeenShutdown;
    private boolean mIsShuttingDown;
    private SnackbarManager.SnackbarController mDismissSnackbar;
    private final Queue<Callback<AutofillAssistantUiDelegate>> mPendingUiOperations =
            new ArrayDeque<>();

    UiDelegateHolder(
            AutofillAssistantUiController controller, AutofillAssistantUiDelegate uiDelegate) {
        mUiController = controller;
        mUiDelegate = uiDelegate;
    }

    /**
     * Perform a UI operation:
     *  - directly if we are not in a pause state.
     *  - later if the shutdown is cancelled.
     *  - never if Autofill Assistant is shut down.
     */
    void performUiOperation(Callback<AutofillAssistantUiDelegate> operation) {
        if (mHasBeenShutdown || mIsShuttingDown) {
            return;
        }

        if (mPaused) {
            mPendingUiOperations.add(operation);
            return;
        }

        operation.onResult(mUiDelegate);
    }

    /**
     * Handles the dismiss operation.
     *
     * In normal mode, hides the UI, pauses UI operations and, unless undone within the time
     * delay, eventually destroy everything. In graceful shutdown mode, shutdown immediately.
     */
    void dismiss(int stringResourceId, Object... formatArgs) {
        assert !mHasBeenShutdown;

        if (mIsShuttingDown) {
            shutdown();
            return;
        }

        if (mDismissSnackbar != null) {
            // Remove duplicate calls.
            return;
        }

        pauseUiOperations();
        mUiDelegate.hide();
        mDismissSnackbar = new SnackbarManager.SnackbarController() {
            @Override
            public void onAction(Object actionData) {
                // Shutdown was cancelled.
                mDismissSnackbar = null;
                mUiDelegate.show();
                unpauseUiOperations();
            }

            @Override
            public void onDismissNoAction(Object actionData) {
                shutdown();
            }
        };
        mUiDelegate.showAutofillAssistantStoppedSnackbar(
                mDismissSnackbar, stringResourceId, formatArgs);
    }

    /** Displays the give up message and enter graceful shutdown mode. */
    void giveUp() {
        performUiOperation(uiDelegate -> uiDelegate.showGiveUpMessage());
        enterGracefulShutdownMode();
    }

    /** Enters graceful shutdown mode once we can again perform UI operations. */
    void enterGracefulShutdownMode() {
        performUiOperation(uiDelegate -> {
            mIsShuttingDown = true;
            mPendingUiOperations.clear();
            uiDelegate.enterGracefulShutdownMode();
            ThreadUtils.postOnUiThreadDelayed(this ::shutdown, GRACEFUL_SHUTDOWN_DELAY_MS);
        });
    }

    /**
     * Hides the UI and destroys everything.
     *
     * <p>Shutdown is final: After this call from the C++ side, as it's been deleted and no UI
     * operation can run.
     */
    void shutdown() {
        if (mHasBeenShutdown) {
            return;
        }

        mHasBeenShutdown = true;
        mPendingUiOperations.clear();
        if (mDismissSnackbar != null) {
            mUiDelegate.dismissSnackbar(mDismissSnackbar);
        }
        mUiDelegate.hide();
        mUiController.unsafeDestroy();
    }

    /**
     * Pause all UI operations such that they can potentially be ran later using {@link
     * #unpauseUiOperations()}.
     */
    private void pauseUiOperations() {
        mPaused = true;
    }

    /**
     * Unpause and trigger all UI operations received by {@link #performUiOperation(Callback)}
     * since the last {@link #pauseUiOperations()}.
     */
    private void unpauseUiOperations() {
        mPaused = false;
        while (!mPendingUiOperations.isEmpty()) {
            mPendingUiOperations.remove().onResult(mUiDelegate);
        }
    }
}
