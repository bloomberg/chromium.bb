// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.graphics.RectF;
import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.Date;
import java.util.List;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantUiController extends AbstractAutofillAssistantUiController {
    private final WebContents mWebContents;

    // TODO(crbug.com/806868): Move mCurrentDetails and mStatusMessage to a Model (refactor to MVC).
    private Details mCurrentDetails = Details.EMPTY_DETAILS;
    private String mStatusMessage;

    /** Native pointer to the UIController. */
    private long mNativeUiController;

    private UiDelegateHolder mUiDelegateHolder;

    /**
     * Creates the UI Controller and UI.
     *
     * @param webContents WebContents the controller is associated to
     * @param mNativeUiController native autofill_assistant::NativeUiController instance
     */
    @CalledByNative
    private static AutofillAssistantUiController createAndStartUi(
            WebContents webContents, long nativeUiController) {
        ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
        assert activity != null;
        // TODO(crbug.com/806868): Keep details on the native side and get rid of this duplicated,
        // misplaced, call to extractParameters.
        Details details = Details.makeFromParameters(
                AutofillAssistantFacade.extractParameters(activity.getInitialIntent().getExtras()));
        AutofillAssistantUiController controller =
                new AutofillAssistantUiController(webContents, nativeUiController, details);
        controller.mUiDelegateHolder = AutofillAssistantUiDelegate.start(activity, controller);
        return controller;
    }

    private AutofillAssistantUiController(
            WebContents webContents, long nativeUiController, Details details) {
        mWebContents = webContents;
        mNativeUiController = nativeUiController;
        mCurrentDetails = details;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeUiController = 0;
    }

    @Override
    public void onDismiss() {
        mUiDelegateHolder.dismiss(R.string.autofill_assistant_stopped);
    }

    @Override
    public Details getDetails() {
        return mCurrentDetails;
    }

    @Override
    public String getStatusMessage() {
        return mStatusMessage;
    }

    @Override
    public void onUnexpectedTaps() {
        mUiDelegateHolder.dismiss(R.string.autofill_assistant_maybe_give_up);
    }

    @Override
    public void updateTouchableArea() {
        if (mNativeUiController != 0) nativeUpdateTouchableArea(mNativeUiController);
    }

    @Override
    public void onUserInteractionInsideTouchableArea() {
        if (mNativeUiController != 0)
            nativeOnUserInteractionInsideTouchableArea(mNativeUiController);
    }

    @Override
    public void onScriptSelected(String scriptPath) {
        if (mNativeUiController != 0) nativeOnScriptSelected(mNativeUiController, scriptPath);
    }

    @Override
    public void onChoice(byte[] serverPayload) {
        if (mNativeUiController != 0) nativeOnChoice(mNativeUiController, serverPayload);
    }

    @Override
    public void onAddressSelected(String guid) {
        if (mNativeUiController != 0) nativeOnAddressSelected(mNativeUiController, guid);
    }

    @Override
    public void onCardSelected(String guid) {
        if (mNativeUiController != 0) nativeOnCardSelected(mNativeUiController, guid);
    }

    @Override
    public void onDetailsAcknowledged(Details displayedDetails, boolean canContinue) {
        mCurrentDetails = displayedDetails;
        if (mNativeUiController != 0) nativeOnShowDetails(mNativeUiController, canContinue);
    }

    @Override
    public String getDebugContext() {
        if (mNativeUiController == 0) return "";
        return nativeOnRequestDebugContext(mNativeUiController);
    }

    @Override
    public void onCompleteShutdown() {
        if (mNativeUiController != 0) nativeStop(mNativeUiController);
    }

    @CalledByNative
    private void onAllowShowingSoftKeyboard(boolean allowed) {
        mUiDelegateHolder.performUiOperation(
                uiDelegate -> uiDelegate.allowShowingSoftKeyboard(allowed));
    }

    @CalledByNative
    private void onShowStatusMessage(String message) {
        mStatusMessage = message;
        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.showStatusMessage(message));
    }

    @CalledByNative
    private String onGetStatusMessage() {
        return mStatusMessage;
    }

    @CalledByNative
    private void onShowOverlay() {
        mUiDelegateHolder.performUiOperation(uiDelegate -> {
            uiDelegate.showOverlay();
            uiDelegate.disableProgressBarPulsing();
        });
    }

    @CalledByNative
    private void onHideOverlay() {
        mUiDelegateHolder.performUiOperation(uiDelegate -> {
            uiDelegate.hideOverlay();
            uiDelegate.enableProgressBarPulsing();
        });
    }

    @CalledByNative
    private void onShutdown() {
        mUiDelegateHolder.shutdown();
    }

    @CalledByNative
    private void onClose() {
        mUiDelegateHolder.close();
    }

    @CalledByNative
    private void onShutdownGracefully() {
        mUiDelegateHolder.enterGracefulShutdownMode();
    }

    @CalledByNative
    private void onUpdateScripts(
            String[] scriptNames, String[] scriptPaths, boolean[] scriptsHighlightFlags) {
        assert scriptNames.length == scriptPaths.length;
        assert scriptNames.length == scriptsHighlightFlags.length;

        List<AutofillAssistantUiDelegate.ScriptHandle> scriptHandles = new ArrayList<>();
        // Note that scriptNames, scriptsHighlightFlags and scriptPaths are one-on-one matched by
        // index.
        for (int i = 0; i < scriptNames.length; i++) {
            scriptHandles.add(new AutofillAssistantUiDelegate.ScriptHandle(
                    scriptNames[i], scriptsHighlightFlags[i], scriptPaths[i]));
        }

        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.updateScripts(scriptHandles));
    }

    @CalledByNative
    private void onChoose(String[] names, byte[][] serverPayloads, boolean[] highlightFlags) {
        assert names.length == serverPayloads.length;
        assert names.length == highlightFlags.length;

        // An empty choice list is supported, as selection can still be forced. onForceChoose should
        // be a no-op in this case.
        if (names.length == 0) return;

        List<AutofillAssistantUiDelegate.Choice> choices = new ArrayList<>();
        assert (names.length == serverPayloads.length);
        for (int i = 0; i < names.length; i++) {
            choices.add(new AutofillAssistantUiDelegate.Choice(
                    names[i], highlightFlags[i], serverPayloads[i]));
        }
        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.showChoices(choices));
    }

    @CalledByNative
    private void onForceChoose() {
        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.clearCarousel());
    }

    @CalledByNative
    private void onChooseAddress() {
        // TODO(crbug.com/806868): Remove this method once all scripts use payment request.
        mUiDelegateHolder.performUiOperation(uiDelegate
                -> uiDelegate.showProfiles(PersonalDataManager.getInstance().getProfilesToSuggest(
                        /* includeNameInLabel= */ true)));
    }

    @CalledByNative
    private void onChooseCard() {
        // TODO(crbug.com/806868): Remove this method once all scripts use payment request.
        mUiDelegateHolder.performUiOperation(uiDelegate
                -> uiDelegate.showCards(PersonalDataManager.getInstance().getCreditCardsToSuggest(
                        /* includeServerCards= */ true)));
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

        mUiDelegateHolder.performUiOperation(uiDelegate -> {
            uiDelegate.showPaymentRequest(mWebContents, paymentOptions, title,
                    supportedBasicCardNetworks, defaultEmail, (selectedPaymentInformation -> {
                        uiDelegate.closePaymentRequest();
                        if (selectedPaymentInformation.succeed) {
                            if (mNativeUiController != 0) {
                                nativeOnGetPaymentInformation(mNativeUiController,
                                        selectedPaymentInformation.succeed,
                                        selectedPaymentInformation.card,
                                        selectedPaymentInformation.address,
                                        selectedPaymentInformation.payerName,
                                        selectedPaymentInformation.payerPhone,
                                        selectedPaymentInformation.payerEmail,
                                        selectedPaymentInformation.isTermsAndConditionsAccepted);
                            }
                        } else {
                            // A failed payment request flow indicates that the UI was either
                            // dismissed or the back button was clicked. In that case we gracefully
                            // shut down.
                            mUiDelegateHolder.giveUp();
                        }
                    }));
        });
    }

    /**
     * Updates the currently shown details.
     *
     * @param newDetails details to display.
     */
    void maybeUpdateDetails(Details newDetails) {
        if (mCurrentDetails.isEmpty() && newDetails.isEmpty()) {
            // No update on UI needed.
            if (mNativeUiController != 0) {
                nativeOnShowDetails(mNativeUiController, /* canContinue= */ true);
            }
            return;
        }

        Details mergedDetails = Details.merge(mCurrentDetails, newDetails);
        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.showDetails(mergedDetails));
    }

    @CalledByNative
    private void onHideDetails() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::hideDetails);
    }

    @CalledByNative
    private void onShowDetails(String title, String url, String description, String mId,
            String price, int year, int month, int day, int hour, int minute, int second) {
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

        maybeUpdateDetails(new Details(title, url, date, description, mId, price,
                /* isFinal= */ true, Collections.emptySet()));
    }

    @CalledByNative
    private void onShowProgressBar(int progress, String message) {
        mUiDelegateHolder.performUiOperation(
                uiDelegate -> uiDelegate.showProgressBar(progress, message));
    }

    @CalledByNative
    private void onHideProgressBar() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::hideProgressBar);
    }

    @CalledByNative
    private void updateTouchableArea(boolean enabled, float[] coords) {
        List<RectF> boxes = new ArrayList<>();
        for (int i = 0; i < coords.length; i += 4) {
            boxes.add(new RectF(/* left= */ coords[i], /* top= */ coords[i + 1],
                    /* right= */ coords[i + 2], /* bottom= */ coords[i + 3]));
        }
        mUiDelegateHolder.performUiOperation(
                uiDelegate -> { uiDelegate.updateTouchableArea(enabled, boxes); });
    }

    @CalledByNative
    private void expandBottomSheet() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::expandBottomSheet);
    }

    // native methods.
    private native void nativeStop(long nativeUiControllerAndroid);
    private native void nativeUpdateTouchableArea(long nativeUiControllerAndroid);
    private native void nativeOnUserInteractionInsideTouchableArea(long nativeUiControllerAndroid);
    private native void nativeOnScriptSelected(long nativeUiControllerAndroid, String scriptPath);
    private native void nativeOnChoice(long nativeUiControllerAndroid, byte[] serverPayload);
    private native void nativeOnAddressSelected(long nativeUiControllerAndroid, String guid);
    private native void nativeOnCardSelected(long nativeUiControllerAndroid, String guid);
    private native void nativeOnShowDetails(long nativeUiControllerAndroid, boolean canContinue);
    private native void nativeOnGetPaymentInformation(long nativeUiControllerAndroid,
            boolean succeed, @Nullable PersonalDataManager.CreditCard card,
            @Nullable PersonalDataManager.AutofillProfile address, @Nullable String payerName,
            @Nullable String payerPhone, @Nullable String payerEmail,
            boolean isTermsAndConditionsAccepted);
    private native String nativeOnRequestDebugContext(long nativeUiControllerAndroid);
}
