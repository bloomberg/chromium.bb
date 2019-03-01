// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel.ALIGNMENT;

import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChipType;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;
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
            ChromeActivity activity, long nativeUiController) {
        assert activity != null;
        return new AutofillAssistantUiController(activity, nativeUiController);
    }

    private AutofillAssistantUiController(ChromeActivity activity, long nativeUiController) {
        mNativeUiController = nativeUiController;
        mActivity = activity;
        mCoordinator = new AssistantCoordinator(activity, this);
        mActivityTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activity.getActivityTabProvider()) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab) {
                        if (mWebContents == null) return;

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
                            AutofillAssistantClient.fromWebContents(mWebContents).destroyUi();
                        }
                    }
                };

        initForCustomTab(activity);
    }

    private void initForCustomTab(ChromeActivity activity) {
        if (!(activity instanceof CustomTabActivity)) {
            return;
        }

        // Shut down Autofill Assistant when the selected tab (foreground tab) is changed.
        TabModel currentTabModel = activity.getTabModelSelector().getCurrentModel();
        currentTabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                // Shutdown the Autofill Assistant if the user switches to another tab.
                if (tab.getWebContents() != mWebContents) {
                    currentTabModel.removeObserver(this);
                    safeNativeOnFatalError(activity.getString(R.string.autofill_assistant_give_up),
                            DropOutReason.TAB_CHANGED);
                }
            }
        });
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
    private void setSuggestions(int[] types, String[] texts) {
        assert types.length == texts.length;
        setChips(getModel().getSuggestionsModel(),
                buildChips(types, texts, this::safeNativeOnSuggestionSelected));
    }

    private List<AssistantChip> buildChips(
            int[] types, String[] texts, Callback<Integer> callback) {
        List<AssistantChip> chips = new ArrayList<>();
        for (int i = 0; i < types.length; i++) {
            int index = i;
            int type = types[i];
            chips.add(new AssistantChip(type, texts[i], () -> callback.onResult(index)));
        }
        return chips;
    }

    @AssistantCarouselModel.Alignment
    private int computeAlignment(List<AssistantChip> chips) {
        int alignment = AssistantCarouselModel.Alignment.START;
        for (AssistantChip chip : chips) {
            if (chip.getType() != AssistantChipType.CHIP_ASSISTIVE) {
                alignment = chips.size() == 1 ? AssistantCarouselModel.Alignment.CENTER
                                              : AssistantCarouselModel.Alignment.END;
            }
        }
        return alignment;
    }

    private void setChips(AssistantCarouselModel model, List<AssistantChip> chips) {
        model.set(ALIGNMENT, computeAlignment(chips));

        // We apply the minimum set of operations on the current chips to transform it in the target
        // list of chips. When testing for chip equivalence, we only compare their type and text but
        // all substitutions will still be applied so we are sure we display the given {@code chips}
        // with their associated callbacks.
        EditDistance.transform(model.getChipsModel(), chips,
                (a, b) -> a.getType() == b.getType() && a.getText().equals(b.getText()));
    }

    @CalledByNative
    private void setActions(
            int[] types, String[] texts, boolean isStopping, boolean isShowingPaymentRequest) {
        AssistantCarouselModel actionsModel = getModel().getActionsModel();
        if (isShowingPaymentRequest) {
            actionsModel.getChipsModel().set(Collections.emptyList());
            return;
        }

        assert types.length == texts.length;
        List<AssistantChip> chips = buildChips(types, texts, this::safeNativeOnActionSelected);
        addCancelOrCloseButton(chips, isStopping);
        setChips(actionsModel, chips);
    }

    private void addCancelOrCloseButton(List<AssistantChip> chips, boolean isStopping) {
        int textResId = isStopping ? R.string.close : R.string.cancel;
        chips.add(new AssistantChip(AssistantChipType.BUTTON_HAIRLINE,
                mActivity.getResources().getString(textResId), () -> {
                    if (isStopping) {
                        safeNativeOnCloseButtonClicked();
                    } else {
                        safeNativeOnCancelButtonClicked();
                    }
                }));
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

    private void safeNativeOnCancelButtonClicked() {
        if (mNativeUiController != 0) nativeOnCancelButtonClicked(mNativeUiController);
    }
    private native void nativeOnCancelButtonClicked(long nativeUiControllerAndroid);

    private void safeNativeOnCloseButtonClicked() {
        if (mNativeUiController != 0) nativeOnCloseButtonClicked(mNativeUiController);
    }
    private native void nativeOnCloseButtonClicked(long nativeUiControllerAndroid);
}
