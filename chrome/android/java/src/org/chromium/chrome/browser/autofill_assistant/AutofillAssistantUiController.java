// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;

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
    private static final int GRACEFUL_SHUTDOWN_DELAY_MS = 5_000;

    private long mNativeUiController;

    private final ChromeActivity mActivity;
    private final AssistantCoordinator mCoordinator;
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;

    @CalledByNative
    private static AutofillAssistantUiController createAndStartUi(
            WebContents webContents, long nativeUiController) {
        return new AutofillAssistantUiController(
                ChromeActivity.fromWebContents(webContents), webContents, nativeUiController);
    }

    private AutofillAssistantUiController(
            ChromeActivity activity, WebContents webContents, long nativeUiController) {
        mNativeUiController = nativeUiController;
        mActivity = activity;
        mCoordinator = new AssistantCoordinator(activity, webContents, this);
        mActivityTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activity.getActivityTabProvider()) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab) {
                        // A null tab indicates that there's no selected tab; We're in the process
                        // of selecting a new tab. TODO(crbug/925947): Hide AssistantCoordinator
                        // instead of destroying it, in case the tab that's eventually selected also
                        // has AutofillAssistant enabled or is the same tab.
                        if (tab == null || tab.getWebContents() != webContents) {
                            safeNativeDestroyUI();
                        }
                    }

                    @Override
                    public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                        if (!isAttached && tab.getWebContents() == webContents) {
                            safeNativeDestroyUI();
                        }
                    }
                };

        initForCustomTab(activity, webContents);
    }

    private void initForCustomTab(ChromeActivity activity, WebContents webContents) {
        if (!(activity instanceof CustomTabActivity)) {
            return;
        }

        // Shut down Autofill Assistant when the selected tab (foreground tab) is changed.
        TabModel currentTabModel = activity.getTabModelSelector().getCurrentModel();
        currentTabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                // Shutdown the Autofill Assistant if the user switches to another tab.
                if (tab.getWebContents() != webContents) {
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
    private AssistantModel getModel() {
        return mCoordinator.getModel();
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeUiController = 0;
    }

    /** Destroys this instance and {@link AssistantCoordinator}. */
    @CalledByNative
    private void destroy(boolean delayed) {
        mActivityTabObserver.destroy();

        if (delayed) {
            // Give some time to the user to read any error message.
            PostTask.postDelayedTask(
                    UiThreadTaskTraits.DEFAULT, mCoordinator::destroy, GRACEFUL_SHUTDOWN_DELAY_MS);
            return;
        }
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
    private void dismissAndShowSnackbar(String message, @DropOutReason int reason) {
        mCoordinator.dismissAndShowSnackbar(message, reason);
    }

    // Native methods.
    private void safeNativeStop(@DropOutReason int reason) {
        if (mNativeUiController != 0) nativeStop(mNativeUiController, reason);
    }
    private native void nativeStop(long nativeUiControllerAndroid, @DropOutReason int reason);

    private void safeNativeDestroyUI() {
        if (mNativeUiController != 0) nativeDestroyUI(mNativeUiController);
    }
    private native void nativeDestroyUI(long nativeUiControllerAndroid);

    private void safeNativeOnFatalError(String message, @DropOutReason int reason) {
        if (mNativeUiController != 0) nativeOnFatalError(mNativeUiController, message, reason);
    }
    private native void nativeOnFatalError(
            long nativeUiControllerAndroid, String message, @DropOutReason int reason);
}
