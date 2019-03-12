// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel.ALIGNMENT;

import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip.Type;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 * This controller is purely a translation and forwarding layer between Native side and the
 * different Java coordinators.
 */
@JNINamespace("autofill_assistant")
// TODO(crbug.com/806868): This class should be removed once all logic is in native side and the
// model is directly modified by the native AssistantMediator.
class AutofillAssistantUiController implements AssistantCoordinator.Delegate {
    private long mNativeUiController;

    private final ChromeActivity mActivity;
    private final AssistantCoordinator mCoordinator;
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;
    private WebContents mWebContents;
    private SnackbarController mSnackbarController;

    /**
     * Finds an activity to which a AA UI can be added.
     *
     * <p>The activity must not already have an AA UI installed.
     */
    @CalledByNative
    @Nullable
    private static ChromeActivity findAppropriateActivity(WebContents webContents) {
        ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
        if (activity != null && AssistantCoordinator.isActive(activity)) {
            return null;
        }

        return activity;
    }

    @CalledByNative
    private static AutofillAssistantUiController create(
            ChromeActivity activity, boolean allowTabSwitching, long nativeUiController) {
        assert activity != null;
        return new AutofillAssistantUiController(activity, allowTabSwitching, nativeUiController);
    }

    private AutofillAssistantUiController(
            ChromeActivity activity, boolean allowTabSwitching, long nativeUiController) {
        mNativeUiController = nativeUiController;
        mActivity = activity;
        mCoordinator = new AssistantCoordinator(activity, this);
        mActivityTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activity.getActivityTabProvider()) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab) {
                        if (mWebContents == null) return;

                        if (!allowTabSwitching) {
                            if (tab == null || tab.getWebContents() != mWebContents) {
                                safeNativeOnFatalError(
                                        activity.getString(R.string.autofill_assistant_give_up),
                                        DropOutReason.TAB_CHANGED);
                            }
                            return;
                        }

                        // Get rid of any undo snackbars right away before switching tabs, to avoid
                        // confusion.
                        dismissSnackbar();

                        AssistantModel model = getModel();
                        if (tab == null) {
                            // A null tab indicates that there's no selected tab; Most likely, we're
                            // in the process of selecting a new tab. Hide the UI for possible reuse
                            // later.
                            getModel().setVisible(false);
                        } else if (tab.getWebContents() == mWebContents) {
                            // The original tab was re-selected. Show it again
                            model.setVisible(true);
                        } else {
                            // A new tab was selected. If Autofill Assistant is running on it,
                            // attach the UI to that other instance, otherwise destroy the UI.
                            AutofillAssistantClient.fromWebContents(mWebContents)
                                    .transferUiTo(tab.getWebContents());
                        }
                    }

                    @Override
                    public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                        if (mWebContents == null) return;

                        if (!isAttached && tab.getWebContents() == mWebContents) {
                            if (!allowTabSwitching) {
                                safeNativeStop(DropOutReason.TAB_DETACHED);
                                return;
                            }
                            AutofillAssistantClient.fromWebContents(mWebContents).destroyUi();
                        }
                    }
                };
    }

    // Java => native methods.

    /** Shut down the Autofill Assistant immediately, without showing a message. */
    @Override
    public void stop(@DropOutReason int reason) {
        safeNativeStop(reason);
    }

    // Native => Java methods.

    // TODO(crbug.com/806868): Some of these functions still have a little bit of logic (e.g. make
    // the progress bar pulse when hiding overlay). Maybe it would be better to forward all calls to
    // AssistantCoordinator (that way this bridge would only have a reference to that one) which in
    // turn will forward calls to the other sub coordinators. The main reason this is not done yet
    // is to avoid boilerplate.

    @CalledByNative
    private void setWebContents(@Nullable WebContents webContents) {
        mWebContents = webContents;
    }

    @CalledByNative
    private AssistantModel getModel() {
        return mCoordinator.getModel();
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeUiController = 0;
        mActivityTabObserver.destroy();
        mCoordinator.destroy();
    }

    /**
     * Close CCT after the current task has finished running - usually after Autofill Assistant has
     * finished shutting itself down.
     */
    @CalledByNative
    private void scheduleCloseCustomTab() {
        if (mActivity instanceof CustomTabActivity) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mActivity::finish);
        }
    }

    @CalledByNative
    private void onShowOnboarding(Runnable onAccept) {
        mCoordinator.showOnboarding(onAccept);
    }

    @CalledByNative
    private void expandBottomSheet() {
        mCoordinator.getBottomBarCoordinator().expand();
    }

    @CalledByNative
    private void showFeedback(String debugContext) {
        mCoordinator.showFeedback(debugContext);
    }

    @CalledByNative
    private void showSnackbar(String message) {
        mSnackbarController = AssistantSnackbar.show(mActivity, message, this::safeSnackbarResult);
    }

    private void dismissSnackbar() {
        if (mSnackbarController != null) {
            mActivity.getSnackbarManager().dismissSnackbars(mSnackbarController);
            mSnackbarController = null;
        }
    }

    @CalledByNative
    private void setSuggestions(String[] texts) {
        List<AssistantChip> chips = new ArrayList<>();
        for (int i = 0; i < texts.length; i++) {
            final int suggestionIndex = i;
            chips.add(new AssistantChip(AssistantChip.Type.CHIP_ASSISTIVE, texts[i],
                    /* disabled= */ false, () -> safeNativeOnSuggestionSelected(suggestionIndex)));
        }
        AssistantCarouselModel model = getModel().getSuggestionsModel();
        model.set(ALIGNMENT, AssistantCarouselModel.Alignment.START);
        setChips(model, chips);
    }

    /** Creates an empty list of chips. */
    @CalledByNative
    private static List<AssistantChip> createChipList() {
        return new ArrayList<>();
    }

    /**
     * Adds an action button to the chip list, which executes the action {@code actionIndex}.
     */
    @CalledByNative
    private void addActionButton(List<AssistantChip> chips, String text, int actionIndex) {
        chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, text, /* disabled= */ false,
                () -> safeNativeOnActionSelected(actionIndex)));
    }

    /**
     * Adds a highlighted action button to the chip list, which executes the action {@code
     * actionIndex}.
     */
    @CalledByNative
    private void addHighlightedActionButton(
            List<AssistantChip> chips, String text, int actionIndex, boolean disabled) {
        chips.add(new AssistantChip(Type.BUTTON_FILLED_BLUE, text, disabled,
                () -> safeNativeOnActionSelected(actionIndex)));
    }

    /**
     * Adds a cancel action button to the chip list, which shows the snackbar and then executes
     * {@code actionIndex}, or shuts down Autofill Assistant if {@code actionIndex} is {@code -1}.
     */
    @CalledByNative
    private void addCancelButton(List<AssistantChip> chips, String text, int actionIndex) {
        chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, text, /* disabled= */ false,
                () -> safeNativeOnCancelButtonClicked(actionIndex)));
    }

    /**
     * Adds a close action button to the chip list, which shuts down Autofill Assistant.
     */
    @CalledByNative
    private void addCloseButton(List<AssistantChip> chips, String text) {
        chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, text, /* disabled= */ false,
                this::safeNativeOnCloseButtonClicked));
    }

    @CalledByNative
    private void setActions(List<AssistantChip> chips) {
        AssistantCarouselModel model = getModel().getActionsModel();
        model.set(ALIGNMENT,
                (chips.size() == 1 && chips.get(0).getType() != Type.BUTTON_FILLED_BLUE)
                        ? AssistantCarouselModel.Alignment.CENTER
                        : AssistantCarouselModel.Alignment.END);
        setChips(model, chips);
    }

    private void setChips(AssistantCarouselModel model, List<AssistantChip> chips) {
        // We apply the minimum set of operations on the current chips to transform it in the target
        // list of chips. When testing for chip equivalence, we only compare their type and text but
        // all substitutions will still be applied so we are sure we display the given {@code chips}
        // with their associated callbacks.
        EditDistance.transform(model.getChipsModel(), chips,
                (a, b) -> a.getType() == b.getType() && a.getText().equals(b.getText()));
    }

    // Native methods.
    private void safeSnackbarResult(boolean undo) {
        if (mNativeUiController != 0) nativeSnackbarResult(mNativeUiController, undo);
    }
    private native void nativeSnackbarResult(long nativeUiControllerAndroid, boolean undo);

    private void safeNativeStop(@DropOutReason int reason) {
        if (mNativeUiController != 0) nativeStop(mNativeUiController, reason);
    }
    private native void nativeStop(long nativeUiControllerAndroid, @DropOutReason int reason);

    private void safeNativeOnFatalError(String message, @DropOutReason int reason) {
        if (mNativeUiController != 0) nativeOnFatalError(mNativeUiController, message, reason);
    }
    private native void nativeOnFatalError(
            long nativeUiControllerAndroid, String message, @DropOutReason int reason);

    private void safeNativeOnSuggestionSelected(int index) {
        if (mNativeUiController != 0) nativeOnSuggestionSelected(mNativeUiController, index);
    }
    private native void nativeOnSuggestionSelected(long nativeUiControllerAndroid, int index);

    private void safeNativeOnActionSelected(int index) {
        if (mNativeUiController != 0) nativeOnActionSelected(mNativeUiController, index);
    }
    private native void nativeOnActionSelected(long nativeUiControllerAndroid, int index);

    private void safeNativeOnCancelButtonClicked(int index) {
        if (mNativeUiController != 0) nativeOnCancelButtonClicked(mNativeUiController, index);
    }
    private native void nativeOnCancelButtonClicked(long nativeUiControllerAndroid, int index);

    private void safeNativeOnCloseButtonClicked() {
        if (mNativeUiController != 0) nativeOnCloseButtonClicked(mNativeUiController);
    }
    private native void nativeOnCloseButtonClicked(long nativeUiControllerAndroid);
}
