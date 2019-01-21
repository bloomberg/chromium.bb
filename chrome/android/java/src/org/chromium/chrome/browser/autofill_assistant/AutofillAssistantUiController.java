// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.graphics.RectF;
import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetails;
import org.chromium.chrome.browser.autofill_assistant.payment.AutofillAssistantPaymentRequest.SelectedPaymentInformation;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.List;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 * This controller is purely a translation and forwarding layer between Native side and the
 * different Java coordinators.
 */
@JNINamespace("autofill_assistant")
class AutofillAssistantUiController implements AssistantCoordinator.Delegate {
    private long mNativeUiController;

    private final AssistantCoordinator mCoordinator;

    @CalledByNative
    private static AutofillAssistantUiController createAndStartUi(
            WebContents webContents, long nativeUiController) {
        return new AutofillAssistantUiController(
                ChromeActivity.fromWebContents(webContents), nativeUiController);
    }

    private AutofillAssistantUiController(ChromeActivity activity, long nativeUiController) {
        mNativeUiController = nativeUiController;
        mCoordinator = new AssistantCoordinator(activity, this);

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
                    mCoordinator.shutdownImmediately();
                }
            }
        });

        // Shut down Autofill Assistant when the selected tab (foreground tab) is changed.
        TabModel currentTabModel = activity.getTabModelSelector().getCurrentModel();
        currentTabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                currentTabModel.removeObserver(this);
                mCoordinator.gracefulShutdown(/* showGiveUpMessage= */ true);
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

    @Override
    public String getDebugContext() {
        return safeNativeOnRequestDebugContext();
    }

    @Override
    public void updateTouchableArea() {
        safeNativeUpdateTouchableArea();
    }

    @Override
    public void onUserInteractionInsideTouchableArea() {
        safeNativeOnUserInteractionInsideTouchableArea();
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
    private void clearNativePtr() {
        mNativeUiController = 0;
    }

    @CalledByNative
    private void onAllowShowingSoftKeyboard(boolean allowed) {
        mCoordinator.getKeyboardCoordinator().allowShowingSoftKeyboard(allowed);
    }

    @CalledByNative
    private void onShowStatusMessage(String message) {
        mCoordinator.showStatusMessage(message);
    }

    @CalledByNative
    private String onGetStatusMessage() {
        return mCoordinator.getHeaderCoordinator().getStatusMessage();
    }

    @CalledByNative
    private void onHideOverlay() {
        mCoordinator.getOverlayCoordinator().hide();

        // Hiding the overlay generally means that the user is expected to interact with the page,
        // so we enable progress bar pulsing animation.
        mCoordinator.getHeaderCoordinator().enableProgressBarPulsing();
    }

    @CalledByNative
    private void onShowOverlay() {
        mCoordinator.getOverlayCoordinator().showFullOverlay();
        mCoordinator.getHeaderCoordinator().disableProgressBarPulsing();
    }

    @CalledByNative
    private void updateTouchableArea(boolean enabled, float[] coords) {
        List<RectF> boxes = new ArrayList<>();
        for (int i = 0; i < coords.length; i += 4) {
            boxes.add(new RectF(/* left= */ coords[i], /* top= */ coords[i + 1],
                    /* right= */ coords[i + 2], /* bottom= */ coords[i + 3]));
        }
        mCoordinator.getOverlayCoordinator().showPartialOverlay(enabled, boxes);
        mCoordinator.getHeaderCoordinator().enableProgressBarPulsing();
    }

    @CalledByNative
    private void onShutdown() {
        mCoordinator.shutdownImmediately();
    }

    @CalledByNative
    private void onShutdownGracefully() {
        mCoordinator.gracefulShutdown(/* showGiveUpMessage= */ false);
    }

    @CalledByNative
    private void onClose() {
        mCoordinator.close();
    }

    // TODO(crbug.com/806868): It would be better to only expose a onSetChips(AssistantChip) to
    // native side with only nativeOnChipSelected(int) method instead of onUpdateScripts, onChoose
    // and onForceChoose.
    @CalledByNative
    private void onUpdateScripts(
            String[] scriptNames, String[] scriptPaths, boolean[] scriptsHighlightFlags) {
        assert scriptNames.length == scriptPaths.length;
        assert scriptNames.length == scriptsHighlightFlags.length;
        mCoordinator.getCarouselCoordinator().setChips(scriptNames, scriptsHighlightFlags,
                i -> safeNativeOnScriptSelected(scriptPaths[i]));
    }

    @CalledByNative
    private void onChoose(String[] names, byte[][] serverPayloads, boolean[] highlightFlags) {
        assert names.length == serverPayloads.length;
        assert names.length == highlightFlags.length;
        mCoordinator.getCarouselCoordinator().setChips(
                names, highlightFlags, i -> safeNativeOnChoice(serverPayloads[i]));
    }

    @CalledByNative
    private void onForceChoose() {
        mCoordinator.getCarouselCoordinator().clearChips();
    }

    @CalledByNative
    private void onRequestPaymentInformation(String defaultEmail, boolean requestShipping,
            boolean requestPayerName, boolean requestPayerPhone, boolean requestPayerEmail,
            int shippingType, String title, String[] supportedBasicCardNetworks) {
        PaymentOptions paymentOptions = new PaymentOptions();
        paymentOptions.requestShipping = requestShipping;
        paymentOptions.requestPayerName = requestPayerName;
        paymentOptions.requestPayerPhone = requestPayerPhone;
        paymentOptions.requestPayerEmail = requestPayerEmail;
        paymentOptions.shippingType = shippingType;

        mCoordinator.getBottomBarCoordinator().allowSwipingBottomSheet(false);
        mCoordinator.getHeaderCoordinator().enableProgressBarPulsing();
        mCoordinator.getPaymentRequestCoordinator()
                .reset(paymentOptions, supportedBasicCardNetworks, defaultEmail)
                .then(this::onRequestPaymentInformationSuccess,
                        this::onRequestPaymentInformationFailed);
    }

    private void onRequestPaymentInformationSuccess(
            SelectedPaymentInformation selectedInformation) {
        mCoordinator.getBottomBarCoordinator().allowSwipingBottomSheet(true);
        mCoordinator.getHeaderCoordinator().disableProgressBarPulsing();
        safeNativeOnGetPaymentInformation(/* succeed= */ true, selectedInformation.card,
                selectedInformation.address, selectedInformation.payerName,
                selectedInformation.payerPhone, selectedInformation.payerEmail,
                selectedInformation.isTermsAndConditionsAccepted);
    }

    private void onRequestPaymentInformationFailed(Exception unusedException) {
        mCoordinator.getBottomBarCoordinator().allowSwipingBottomSheet(true);
        mCoordinator.gracefulShutdown(/* showGiveUpMessage= */ true);
    }

    @CalledByNative
    private void onHideDetails() {
        mCoordinator.getDetailsCoordinator().hideDetails();
    }

    @CalledByNative
    private void onShowInitialDetails(String title, String description, String mid, String date) {
        mCoordinator.getDetailsCoordinator().showInitialDetails(title, description, mid, date);
    }

    @CalledByNative
    private void onShowDetails(String title, String url, String description, String mId,
            String price, int year, int month, int day, int hour, int minute, int second,
            boolean userApprovalRequired, boolean highlightTitle, boolean highlightDate) {
        Date date;
        if (year > 0 && month > 0 && day > 0 && hour >= 0 && minute >= 0 && second >= 0) {
            Calendar calendar = Calendar.getInstance();
            calendar.clear();
            // Month in Java Date is 0-based, but the one we receive from the server is 1-based.
            calendar.set(year, month - 1, day, hour, minute, second);
            date = calendar.getTime();
        } else {
            date = null;
        }

        if (price.length() == 0) price = null;

        mCoordinator
                .showDetailsForApproval(new AssistantDetails(title, url, date, description, mId,
                        price, userApprovalRequired, highlightTitle, highlightDate,
                        /* showPlaceholdersForEmptyFields= */ false))
                .then(this::safeNativeOnShowDetails,
                        ignoredException -> safeNativeOnShowDetails(/* canContinue= */ false));
    }

    @CalledByNative
    private void onShowProgressBar(int progress, String message) {
        mCoordinator.getHeaderCoordinator().setStatusMessage(message);
        mCoordinator.getHeaderCoordinator().setProgress(progress);
    }

    @CalledByNative
    private void onHideProgressBar() {
        // TODO(crbug.com/806868): Either implement this or remove it from C++ bindings.
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
    private void onChooseAddress() {
        // TODO(crbug.com/806868): Remove calls to this function.
    }

    @CalledByNative
    private void onChooseCard() {
        // TODO(crbug.com/806868): Remove calls to this function.
    }

    // Native methods.
    void safeNativeStop() {
        if (mNativeUiController != 0) nativeStop(mNativeUiController);
    }
    private native void nativeStop(long nativeUiControllerAndroid);

    void safeNativeUpdateTouchableArea() {
        if (mNativeUiController != 0) nativeUpdateTouchableArea(mNativeUiController);
    }
    private native void nativeUpdateTouchableArea(long nativeUiControllerAndroid);

    void safeNativeOnUserInteractionInsideTouchableArea() {
        if (mNativeUiController != 0)
            nativeOnUserInteractionInsideTouchableArea(mNativeUiController);
    }
    private native void nativeOnUserInteractionInsideTouchableArea(long nativeUiControllerAndroid);

    void safeNativeOnScriptSelected(String scriptPath) {
        if (mNativeUiController != 0) nativeOnScriptSelected(mNativeUiController, scriptPath);
    }
    private native void nativeOnScriptSelected(long nativeUiControllerAndroid, String scriptPath);

    void safeNativeOnChoice(byte[] serverPayload) {
        if (mNativeUiController != 0) nativeOnChoice(mNativeUiController, serverPayload);
    }
    private native void nativeOnChoice(long nativeUiControllerAndroid, byte[] serverPayload);

    void safeNativeOnShowDetails(boolean canContinue) {
        if (mNativeUiController != 0) nativeOnShowDetails(mNativeUiController, canContinue);
    }
    private native void nativeOnShowDetails(long nativeUiControllerAndroid, boolean canContinue);

    void safeNativeOnGetPaymentInformation(boolean succeed,
            @Nullable PersonalDataManager.CreditCard card,
            @Nullable PersonalDataManager.AutofillProfile address, @Nullable String payerName,
            @Nullable String payerPhone, @Nullable String payerEmail,
            boolean isTermsAndConditionsAccepted) {
        if (mNativeUiController != 0)
            nativeOnGetPaymentInformation(mNativeUiController, succeed, card, address, payerName,
                    payerPhone, payerEmail, isTermsAndConditionsAccepted);
    }
    private native void nativeOnGetPaymentInformation(long nativeUiControllerAndroid,
            boolean succeed, @Nullable PersonalDataManager.CreditCard card,
            @Nullable PersonalDataManager.AutofillProfile address, @Nullable String payerName,
            @Nullable String payerPhone, @Nullable String payerEmail,
            boolean isTermsAndConditionsAccepted);

    String safeNativeOnRequestDebugContext() {
        if (mNativeUiController != 0) return nativeOnRequestDebugContext(mNativeUiController);
        return "";
    }
    private native String nativeOnRequestDebugContext(long nativeUiControllerAndroid);
}
