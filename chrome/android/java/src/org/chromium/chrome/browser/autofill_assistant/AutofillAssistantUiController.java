// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
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
    private long mNativeUiController;

    private final AssistantCoordinator mCoordinator;

    @CalledByNative
    private static AutofillAssistantUiController createAndStartUi(
            WebContents webContents, long nativeUiController) {
        return new AutofillAssistantUiController(
                ChromeActivity.fromWebContents(webContents), webContents, nativeUiController);
    }

    private AutofillAssistantUiController(
            ChromeActivity activity, WebContents webContents, long nativeUiController) {
        mNativeUiController = nativeUiController;
        mCoordinator = new AssistantCoordinator(activity, webContents, this);

        initForCustomTab(activity);
    }

    private void initForCustomTab(ChromeActivity activity) {
        if (!(activity instanceof CustomTabActivity)) {
            return;
        }

        Tab activityTab = activity.getActivityTab();
        activityTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                if (!isAttached) {
                    activityTab.removeObserver(this);
                    mCoordinator.shutdownImmediately(DropOutReason.TAB_DETACHED);
                }
            }
        });

        // Shut down Autofill Assistant when the selected tab (foreground tab) is changed.
        TabModel currentTabModel = activity.getTabModelSelector().getCurrentModel();
        currentTabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                // Shutdown the Autofill Assistant if the user switches to another tab.
                if (!activityTab.equals(tab)) {
                    currentTabModel.removeObserver(this);
                    mCoordinator.gracefulShutdown(
                            /* showGiveUpMessage= */ true, DropOutReason.TAB_CHANGED);
                }
            }
        });
    }

    /**
     * Java => native methods.
     */

    @Override
    public void stop() {
        safeNativeStop();
    }

    /**
     * Native => Java methods.
     */

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

    @CalledByNative
    private void onShutdown(@DropOutReason int reason) {
        mCoordinator.shutdownImmediately(reason);
    }

    @CalledByNative
    private void onShutdownGracefully(@DropOutReason int reason) {
        mCoordinator.gracefulShutdown(/* showGiveUpMessage= */ false, reason);
    }

    @CalledByNative
    private void onClose() {
        mCoordinator.close();
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
    void safeNativeStop() {
        if (mNativeUiController != 0) nativeStop(mNativeUiController);
    }
    private native void nativeStop(long nativeUiControllerAndroid);

}
