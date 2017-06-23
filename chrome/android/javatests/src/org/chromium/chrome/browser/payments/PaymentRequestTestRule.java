// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.view.View;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt;
import org.chromium.chrome.browser.payments.PaymentRequestTestCommon.PaymentRequestTestCommonCallback;
import org.chromium.chrome.browser.payments.PaymentRequestTestCommon.PaymentsCallbackHelper;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection.OptionRow;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.test.ChromeActivityTestRule;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Custom ActivityTestRule for integration test for payments.
 */
public class PaymentRequestTestRule extends ChromeActivityTestRule<ChromeTabbedActivity>
        implements PaymentRequestTestCommonCallback {
    /** Flag for installing a payment app without instruments. */
    public static final int NO_INSTRUMENTS = PaymentRequestTestCommon.NO_INSTRUMENTS;

    /** Flag for installing a payment app with instruments. */
    public static final int HAVE_INSTRUMENTS = PaymentRequestTestCommon.HAVE_INSTRUMENTS;

    /** Flag for installing a fast payment app. */
    public static final int IMMEDIATE_RESPONSE = PaymentRequestTestCommon.IMMEDIATE_RESPONSE;

    /** Flag for installing a slow payment app. */
    public static final int DELAYED_RESPONSE = PaymentRequestTestCommon.DELAYED_RESPONSE;

    /** Flag for immediately installing a payment app. */
    public static final int IMMEDIATE_CREATION = PaymentRequestTestCommon.IMMEDIATE_CREATION;

    /** Flag for installing a payment app with a delay. */
    public static final int DELAYED_CREATION = PaymentRequestTestCommon.DELAYED_CREATION;

    /** The expiration month dropdown index for December. */
    public static final int DECEMBER = PaymentRequestTestCommon.DECEMBER;

    /** The expiration year dropdown index for the next year. */
    public static final int NEXT_YEAR = PaymentRequestTestCommon.NEXT_YEAR;

    /** The billing address dropdown index for the first billing address. */
    public static final int FIRST_BILLING_ADDRESS = PaymentRequestTestCommon.FIRST_BILLING_ADDRESS;

    private final PaymentRequestTestCommon mTestCommon;
    private final MainActivityStartCallback mCallback;

    public PaymentRequestTestRule(String testFileName, MainActivityStartCallback callback) {
        super(ChromeTabbedActivity.class);
        mTestCommon = new PaymentRequestTestCommon(this, testFileName);
        mCallback = callback;
    }

    public PaymentRequestTestRule(String testFileName) {
        this(testFileName, null);
    }

    public PaymentsCallbackHelper<PaymentRequestUI> getReadyForInput() {
        return mTestCommon.mReadyForInput;
    }
    public PaymentsCallbackHelper<PaymentRequestUI> getReadyToPay() {
        return mTestCommon.mReadyToPay;
    }
    public PaymentsCallbackHelper<PaymentRequestUI> getSelectionChecked() {
        return mTestCommon.mSelectionChecked;
    }
    public PaymentsCallbackHelper<PaymentRequestUI> getResultReady() {
        return mTestCommon.mResultReady;
    }
    public PaymentsCallbackHelper<CardUnmaskPrompt> getReadyForUnmaskInput() {
        return mTestCommon.mReadyForUnmaskInput;
    }
    public PaymentsCallbackHelper<CardUnmaskPrompt> getReadyToUnmask() {
        return mTestCommon.mReadyToUnmask;
    }
    public PaymentsCallbackHelper<CardUnmaskPrompt> getUnmaskValidationDone() {
        return mTestCommon.mUnmaskValidationDone;
    }
    public CallbackHelper getReadyToEdit() {
        return mTestCommon.mReadyToEdit;
    }
    public CallbackHelper getEditorValidationError() {
        return mTestCommon.mEditorValidationError;
    }
    public CallbackHelper getEditorTextUpdate() {
        return mTestCommon.mEditorTextUpdate;
    }
    public CallbackHelper getDismissed() {
        return mTestCommon.mDismissed;
    }
    public CallbackHelper getUnableToAbort() {
        return mTestCommon.mUnableToAbort;
    }
    public CallbackHelper getBillingAddressChangeProcessed() {
        return mTestCommon.mBillingAddressChangeProcessed;
    }
    public CallbackHelper getShowFailed() {
        return mTestCommon.mShowFailed;
    }
    public CallbackHelper getCanMakePaymentQueryResponded() {
        return mTestCommon.mCanMakePaymentQueryResponded;
    }
    public CallbackHelper getExpirationMonthChange() {
        return mTestCommon.mExpirationMonthChange;
    }
    public PaymentRequestUI getPaymentRequestUI() {
        return mTestCommon.mUI;
    }

    public void startMainActivity() throws InterruptedException {
        mTestCommon.startMainActivity();
    }

    protected void triggerUIAndWait(PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        mTestCommon.triggerUIAndWait(helper);
    }

    protected void openPageAndClickBuyAndWait(CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        mTestCommon.openPageAndClickBuyAndWait(helper);
    }

    protected void triggerUIAndWait(String nodeId, PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        mTestCommon.triggerUIAndWait(nodeId, helper);
    }

    protected void openPageAndClickNodeAndWait(String nodeId, CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        mTestCommon.openPageAndClickNodeAndWait(nodeId, helper);
    }

    protected void openPageAndClickNode(String nodeId)
            throws InterruptedException, ExecutionException, TimeoutException {
        mTestCommon.openPageAndClickNode(nodeId);
    }

    protected void reTriggerUIAndWait(
            String nodeId, PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.reTriggerUIAndWait(nodeId, helper);
    }

    /** Clicks on an HTML node. */
    protected void clickNodeAndWait(String nodeId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickNodeAndWait(nodeId, helper);
    }

    /** Clicks on an element in the payments UI. */
    protected void clickAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickAndWait(resourceId, helper);
    }

    /** Clicks on an element in the "Shipping address" section of the payments UI. */
    protected void clickInShippingAddressAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickInShippingAddressAndWait(resourceId, helper);
    }

    /** Clicks on an element in the "Payment" section of the payments UI. */
    protected void clickInPaymentMethodAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickInPaymentMethodAndWait(resourceId, helper);
    }

    /** Clicks on an element in the "Contact Info" section of the payments UI. */
    protected void clickInContactInfoAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickInContactInfoAndWait(resourceId, helper);
    }

    /** Clicks on an element in the editor UI for credit cards. */
    protected void clickInCardEditorAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickInCardEditorAndWait(resourceId, helper);
    }

    /** Clicks on an element in the editor UI. */
    protected void clickInEditorAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickInEditorAndWait(resourceId, helper);
    }

    protected void clickAndroidBackButtonInEditorAndWait(CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickAndroidBackButtonInEditorAndWait(helper);
    }

    /** Clicks on a button in the card unmask UI. */
    protected void clickCardUnmaskButtonAndWait(final int dialogButtonId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.clickCardUnmaskButtonAndWait(dialogButtonId, helper);
    }

    /** Gets the button state for the shipping summary section. */
    protected int getShippingAddressSectionButtonState() throws ExecutionException {
        return mTestCommon.getShippingAddressSectionButtonState();
    }

    /** Gets the button state for the contact details section. */
    protected int getContactDetailsButtonState() throws ExecutionException {
        return mTestCommon.getContactDetailsButtonState();
    }

    /**  Returns the label corresponding to the payment instrument at the specified |index|. */
    protected String getPaymentInstrumentLabel(final int index) throws ExecutionException {
        return mTestCommon.getPaymentInstrumentLabel(index);
    }

    /** Returns the label of the selected payment instrument. */
    protected String getSelectedPaymentInstrumentLabel() throws ExecutionException {
        return mTestCommon.getSelectedPaymentInstrumentLabel();
    }

    /**  Returns the total amount in order summary section. */
    protected String getOrderSummaryTotal() throws ExecutionException {
        return mTestCommon.getOrderSummaryTotal();
    }

    /**
     *  Returns the label corresponding to the contact detail suggestion at the specified
     *  |suggestionIndex|.
     */
    protected String getContactDetailsSuggestionLabel(final int suggestionIndex)
            throws ExecutionException {
        return mTestCommon.getContactDetailsSuggestionLabel(suggestionIndex);
    }

    /**  Returns the number of payment instruments. */
    protected int getNumberOfPaymentInstruments() throws ExecutionException {
        return mTestCommon.getNumberOfPaymentInstruments();
    }

    /**  Returns the number of contact detail suggestions. */
    protected int getNumberOfContactDetailSuggestions() throws ExecutionException {
        return mTestCommon.getNumberOfContactDetailSuggestions();
    }

    /**
     *  Returns the label corresponding to the shipping address suggestion at the specified
     *  |suggestionIndex|.
     */
    protected String getShippingAddressSuggestionLabel(final int suggestionIndex)
            throws ExecutionException {
        return mTestCommon.getShippingAddressSuggestionLabel(suggestionIndex);
    }

    /**
     *  Returns the summary text of the shipping address section.
     */
    protected String getShippingAddressSummaryLabel() throws ExecutionException {
        return mTestCommon.getShippingAddressSummary();
    }

    /**
     *  Returns the summary text of the shipping option section.
     */
    protected String getShippingOptionSummaryLabel() throws ExecutionException {
        return mTestCommon.getShippingOptionSummary();
    }

    /**
     *  Returns the cost text of the shipping option section on the bottom sheet.
     */
    protected String getShippingOptionCostSummaryLabelOnBottomSheet() throws ExecutionException {
        return mTestCommon.getShippingOptionCostSummaryOnBottomSheet();
    }

    /** Returns the focused view in the card editor view. */
    protected View getCardEditorFocusedView() {
        return mTestCommon.getCardEditorFocusedView();
    }

    /**
     *  Clicks on the label corresponding to the shipping address suggestion at the specified
     *  |suggestionIndex|.
     * @throws InterruptedException
     */
    protected void clickOnShippingAddressSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper)
            throws ExecutionException, TimeoutException, InterruptedException {
        mTestCommon.clickOnShippingAddressSuggestionOptionAndWait(suggestionIndex, helper);
    }

    /**
     *  Clicks on the label corresponding to the payment method suggestion at the specified
     *  |suggestionIndex|.
     * @throws InterruptedException
     */
    protected void clickOnPaymentMethodSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper)
            throws ExecutionException, TimeoutException, InterruptedException {
        mTestCommon.clickOnPaymentMethodSuggestionOptionAndWait(suggestionIndex, helper);
    }

    /**
     *  Clicks on the edit icon corresponding to the payment method suggestion at the specified
     *  |suggestionIndex|.
     */
    protected void clickOnPaymentMethodSuggestionEditIconAndWait(
            final int suggestionIndex, CallbackHelper helper)
            throws ExecutionException, TimeoutException, InterruptedException {
        mTestCommon.clickOnPaymentMethodSuggestionEditIconAndWait(suggestionIndex, helper);
    }

    /**
     *  Returns the number of shipping address suggestions.
     */
    protected int getNumberOfShippingAddressSuggestions() throws ExecutionException {
        return mTestCommon.getNumberOfShippingAddressSuggestions();
    }

    /** Returns the {@link OptionRow} at the given index for the shipping address section. */
    protected OptionRow getShippingAddressOptionRowAtIndex(final int index)
            throws ExecutionException {
        return mTestCommon.getShippingAddressOptionRowAtIndex(index);
    }

    /** Returns the selected spinner value in the editor UI for credit cards. */
    protected String getSpinnerSelectionTextInCardEditor(final int dropdownIndex)
            throws ExecutionException {
        return mTestCommon.getSpinnerSelectionTextInCardEditor(dropdownIndex);
    }

    /** Returns the spinner value at the specified position in the editor UI for credit cards. */
    protected String getSpinnerTextAtPositionInCardEditor(
            final int dropdownIndex, final int itemPosition) throws ExecutionException {
        return mTestCommon.getSpinnerTextAtPositionInCardEditor(dropdownIndex, itemPosition);
    }

    /** Returns the number of items offered by the spinner in the editor UI for credit cards. */
    protected int getSpinnerItemCountInCardEditor(final int dropdownIndex)
            throws ExecutionException {
        return mTestCommon.getSpinnerItemCountInCardEditor(dropdownIndex);
    }

    /** Returns the error message visible to the user in the credit card unmask prompt. */
    protected String getUnmaskPromptErrorMessage() {
        return mTestCommon.getUnmaskPromptErrorMessage();
    }

    /** Selects the spinner value in the editor UI for credit cards. */
    protected void setSpinnerSelectionsInCardEditorAndWait(final int[] selections,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        mTestCommon.setSpinnerSelectionsInCardEditorAndWait(selections, helper);
    }

    /** Selects the spinner value in the editor UI. */
    protected void setSpinnerSelectionInEditorAndWait(final int selection, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.setSpinnerSelectionInEditorAndWait(selection, helper);
    }

    /** Directly sets the text in the editor UI for credit cards. */
    protected void setTextInCardEditorAndWait(final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.setTextInCardEditorAndWait(values, helper);
    }

    /** Directly sets the text in the editor UI. */
    protected void setTextInEditorAndWait(final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.setTextInEditorAndWait(values, helper);
    }

    /** Directly sets the checkbox selection in the editor UI for credit cards. */
    protected void selectCheckboxAndWait(final int resourceId, final boolean isChecked,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        mTestCommon.selectCheckboxAndWait(resourceId, isChecked, helper);
    }

    /** Directly sets the text in the card unmask UI. */
    protected void setTextInCardUnmaskDialogAndWait(final int resourceId, final String input,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        mTestCommon.setTextInCardUnmaskDialogAndWait(resourceId, input, helper);
    }

    /** Directly sets the text in the expired card unmask UI. */
    protected void setTextInExpiredCardUnmaskDialogAndWait(
            final int[] resourceIds, final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.setTextInExpiredCardUnmaskDialogAndWait(resourceIds, values, helper);
    }

    /** Focues a view and hits the "submit" button on the software keyboard. */
    /* package */ void hitSoftwareKeyboardSubmitButtonAndWait(int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        mTestCommon.hitSoftwareKeyboardSubmitButtonAndWait(resourceId, helper);
    }

    /** Verifies the contents of the test webpage. */
    protected void expectResultContains(final String[] contents) {
        mTestCommon.expectResultContains(contents);
    }

    /**  Will fail if the OptionRow at |index| is not selected in Contact Details.*/
    protected void expectContactDetailsRowIsSelected(final int index) {
        mTestCommon.expectContactDetailsRowIsSelected(index);
    }

    /**  Will fail if the OptionRow at |index| is not selected in Shipping Address section.*/
    protected void expectShippingAddressRowIsSelected(final int index) {
        mTestCommon.expectShippingAddressRowIsSelected(index);
    }

    /**  Will fail if the OptionRow at |index| is not selected in PaymentMethod section.*/
    protected void expectPaymentMethodRowIsSelected(final int index) {
        mTestCommon.expectPaymentMethodRowIsSelected(index);
    }

    /**
     * Asserts that only the specified reason for abort is logged.
     *
     * @param abortReason The only bucket in the abort histogram that should have a record.
     */
    protected void assertOnlySpecificAbortMetricLogged(int abortReason) {
        mTestCommon.assertOnlySpecificAbortMetricLogged(abortReason);
    }

    public void onPaymentRequestReadyForInput(PaymentRequestUI ui) {
        mTestCommon.onPaymentRequestReadyForInput(ui);
    }

    public void onPaymentRequestReadyToEdit() {
        mTestCommon.onPaymentRequestReadyToEdit();
    }

    public void onPaymentRequestEditorValidationError() {
        mTestCommon.onPaymentRequestEditorValidationError();
    }

    public void onPaymentRequestEditorTextUpdate() {
        mTestCommon.onPaymentRequestEditorTextUpdate();
    }

    public void onPaymentRequestReadyToPay(PaymentRequestUI ui) {
        mTestCommon.onPaymentRequestReadyToPay(ui);
    }

    public void onPaymentRequestSelectionChecked(PaymentRequestUI ui) {
        mTestCommon.onPaymentRequestSelectionChecked(ui);
    }

    public void onPaymentRequestResultReady(PaymentRequestUI ui) {
        mTestCommon.onPaymentRequestResultReady(ui);
    }

    public void onPaymentRequestDismiss() {
        mTestCommon.onPaymentRequestDismiss();
    }

    public void onPaymentRequestServiceUnableToAbort() {
        mTestCommon.onPaymentRequestServiceUnableToAbort();
    }

    public void onPaymentRequestServiceBillingAddressChangeProcessed() {
        mTestCommon.onPaymentRequestServiceBillingAddressChangeProcessed();
    }

    public void onPaymentRequestServiceExpirationMonthChange() {
        mTestCommon.onPaymentRequestServiceExpirationMonthChange();
    }

    public void onPaymentRequestServiceShowFailed() {
        mTestCommon.onPaymentRequestServiceShowFailed();
    }

    public void onPaymentRequestServiceCanMakePaymentQueryResponded() {
        mTestCommon.onPaymentRequestServiceCanMakePaymentQueryResponded();
    }

    public void onCardUnmaskPromptReadyForInput(CardUnmaskPrompt prompt) {
        mTestCommon.onCardUnmaskPromptReadyForInput(prompt);
    }

    public void onCardUnmaskPromptReadyToUnmask(CardUnmaskPrompt prompt) {
        mTestCommon.onCardUnmaskPromptReadyToUnmask(prompt);
    }

    public void onCardUnmaskPromptValidationDone(CardUnmaskPrompt prompt) {
        mTestCommon.onCardUnmaskPromptValidationDone(prompt);
    }

    /**
     * Installs a payment app for testing.
     *
     * @param instrumentPresence Whether the app has any payment instruments. Either NO_INSTRUMENTS
     *                           or HAVE_INSTRUMENTS.
     * @param responseSpeed      How quickly the app will respond to "get instruments" query. Either
     *                           IMMEDIATE_RESPONSE or DELAYED_RESPONSE.
     */
    protected void installPaymentApp(int instrumentPresence, int responseSpeed) {
        mTestCommon.installPaymentApp(instrumentPresence, responseSpeed);
    }

    /**
     * Installs a payment app for testing.
     *
     * @param methodName         The name of the payment method used in the payment app.
     * @param instrumentPresence Whether the app has any payment instruments. Either NO_INSTRUMENTS
     *                           or HAVE_INSTRUMENTS.
     * @param responseSpeed      How quickly the app will respond to "get instruments" query. Either
     *                           IMMEDIATE_RESPONSE or DELAYED_RESPONSE.
     */
    protected void installPaymentApp(String methodName, int instrumentPresence, int responseSpeed) {
        mTestCommon.installPaymentApp(methodName, instrumentPresence, responseSpeed);
    }

    /**
     * Installs a payment app for testing.
     *
     * @param methodName         The name of the payment method used in the payment app.
     * @param instrumentPresence Whether the app has any payment instruments. Either NO_INSTRUMENTS
     *                           or HAVE_INSTRUMENTS.
     * @param responseSpeed      How quickly the app will respond to "get instruments" query. Either
     *                           IMMEDIATE_RESPONSE or DELAYED_RESPONSE.
     * @param creationSpeed      How quickly the app factory will create this app. Either
     *                           IMMEDIATE_CREATION or DELAYED_CREATION.
     */
    protected void installPaymentApp(
            String methodName, int instrumentPresence, int responseSpeed, int creationSpeed) {
        mTestCommon.installPaymentApp(methodName, instrumentPresence, responseSpeed, creationSpeed);
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        if (mCallback != null) {
            mCallback.onMainActivityStarted();
        }
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                startMainActivity();
                base.evaluate();
            }
        }, description);
    }

    /** The interface for being notified of the main activity startup. */
    public interface MainActivityStartCallback {
        /** Called when the main activity has started up. */
        void onMainActivityStarted() throws
                InterruptedException, ExecutionException, TimeoutException;
    }
}
