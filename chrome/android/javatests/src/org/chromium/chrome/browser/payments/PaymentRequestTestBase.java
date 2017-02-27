// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static java.util.Arrays.asList;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt.CardUnmaskObserverForTest;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppFactoryAddition;
import org.chromium.chrome.browser.payments.PaymentRequestImpl.PaymentRequestServiceObserverForTest;
import org.chromium.chrome.browser.payments.ui.EditorTextField;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection.OptionRow;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A base integration test for payments.
 */
@RetryOnFailure
abstract class PaymentRequestTestBase extends ChromeActivityTestCaseBase<ChromeTabbedActivity>
        implements PaymentRequestObserverForTest, PaymentRequestServiceObserverForTest,
        CardUnmaskObserverForTest {
    /** Flag for installing a payment app without instruments. */
    protected static final int NO_INSTRUMENTS = 0;

    /** Flag for installing a payment app with instruments. */
    protected static final int HAVE_INSTRUMENTS = 1;

    /** Flag for installing a fast payment app. */
    protected static final int IMMEDIATE_RESPONSE = 0;

    /** Flag for installing a slow payment app. */
    protected static final int DELAYED_RESPONSE = 1;

    /** Flag for immediately installing a payment app. */
    protected static final int IMMEDIATE_CREATION = 0;

    /** Flag for installing a payment app with a delay. */
    protected static final int DELAYED_CREATION = 1;

    /** The expiration month dropdown index for December. */
    protected static final int DECEMBER = 11;

    /** The expiration year dropdown index for the next year. */
    protected static final int NEXT_YEAR = 1;

    /** The billing address dropdown index for the first billing address. */
    protected static final int FIRST_BILLING_ADDRESS = 0;

    protected final PaymentsCallbackHelper<PaymentRequestUI> mReadyForInput;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mReadyToPay;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mSelectionChecked;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mResultReady;
    protected final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyForUnmaskInput;
    protected final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyToUnmask;
    protected final PaymentsCallbackHelper<CardUnmaskPrompt> mUnmaskValidationDone;
    protected final CallbackHelper mReadyToEdit;
    protected final CallbackHelper mEditorValidationError;
    protected final CallbackHelper mEditorTextUpdate;
    protected final CallbackHelper mDismissed;
    protected final CallbackHelper mUnableToAbort;
    protected final CallbackHelper mBillingAddressChangeProcessed;
    protected final CallbackHelper mShowFailed;
    protected final CallbackHelper mCanMakePaymentQueryResponded;
    protected final CallbackHelper mExpirationMonthChange;
    protected PaymentRequestUI mUI;

    private final AtomicReference<ContentViewCore> mViewCoreRef;
    private final AtomicReference<WebContents> mWebContentsRef;
    private final String mTestFilePath;
    private CardUnmaskPrompt mCardUnmaskPrompt;

    protected PaymentRequestTestBase(String testFileName) {
        super(ChromeTabbedActivity.class);
        mReadyForInput = new PaymentsCallbackHelper<>();
        mReadyToPay = new PaymentsCallbackHelper<>();
        mSelectionChecked = new PaymentsCallbackHelper<>();
        mResultReady = new PaymentsCallbackHelper<>();
        mReadyForUnmaskInput = new PaymentsCallbackHelper<>();
        mReadyToUnmask = new PaymentsCallbackHelper<>();
        mUnmaskValidationDone = new PaymentsCallbackHelper<>();
        mReadyToEdit = new CallbackHelper();
        mEditorValidationError = new CallbackHelper();
        mEditorTextUpdate = new CallbackHelper();
        mDismissed = new CallbackHelper();
        mUnableToAbort = new CallbackHelper();
        mBillingAddressChangeProcessed = new CallbackHelper();
        mExpirationMonthChange = new CallbackHelper();
        mShowFailed = new CallbackHelper();
        mCanMakePaymentQueryResponded = new CallbackHelper();
        mViewCoreRef = new AtomicReference<>();
        mWebContentsRef = new AtomicReference<>();
        mTestFilePath = UrlUtils.getIsolatedTestFilePath(
                String.format("chrome/test/data/payments/%s", testFileName));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(mTestFilePath);
    }

    protected abstract void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException;

    protected void triggerUIAndWait(PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("buy", helper);
        mUI = helper.getTarget();
    }

    protected void openPageAndClickBuyAndWait(CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("buy", helper);
    }

    protected void triggerUIAndWait(String nodeId, PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait(nodeId, helper);
        mUI = helper.getTarget();
    }

    protected void openPageAndClickNodeAndWait(String nodeId, CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        onMainActivityStarted();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mViewCoreRef.set(getActivity().getCurrentContentViewCore());
                mWebContentsRef.set(mViewCoreRef.get().getWebContents());
                PaymentRequestUI.setObserverForTest(PaymentRequestTestBase.this);
                PaymentRequestImpl.setObserverForTest(PaymentRequestTestBase.this);
                CardUnmaskPrompt.setObserverForTest(PaymentRequestTestBase.this);
            }
        });
        assertWaitForPageScaleFactorMatch(1);
        clickNodeAndWait(nodeId, helper);
    }

    protected void reTriggerUIAndWait(
            String nodeId, PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        clickNodeAndWait(nodeId, helper);
        mUI = helper.getTarget();
    }

    /** Clicks on an HTML node. */
    protected void clickNodeAndWait(String nodeId, CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        int callCount = helper.getCallCount();
        DOMUtils.clickNode(mViewCoreRef.get(), nodeId);
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the payments UI. */
    protected void clickAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getDialogForTest().findViewById(resourceId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /**
     * Clicks on an element in the "Shipping summary" section of the payments UI. This section
     * combines both shipping address and shipping option. It is replaced by "Shipping address" and
     * "Shipping option" sections upon expanding the payments UI.
     */
    protected void clickInShippingSummaryAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getShippingSummarySectionForTest().findViewById(resourceId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Shipping address" section of the payments UI. */
    protected void clickInShippingAddressAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getShippingAddressSectionForTest().findViewById(resourceId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Payment" section of the payments UI. */
    protected void clickInPaymentMethodAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getPaymentMethodSectionForTest().findViewById(resourceId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the "Contact Info" section of the payments UI. */
    protected void clickInContactInfoAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getContactDetailsSectionForTest().findViewById(resourceId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the editor UI for credit cards. */
    protected void clickInCardEditorAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getCardEditorView().findViewById(resourceId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on an element in the editor UI. */
    protected void clickInEditorAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getEditorView().findViewById(resourceId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    protected void clickAndroidBackButtonInEditorAndWait(CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mUI.getEditorView().dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
                mUI.getEditorView().dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK));
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Clicks on a button in the card unmask UI. */
    protected void clickCardUnmaskButtonAndWait(final int dialogButtonId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mCardUnmaskPrompt.getDialogForTest().getButton(dialogButtonId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Gets the button state for the shipping summary section. */
    protected int getSummarySectionButtonState() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mUI.getShippingSummarySectionForTest().getEditButtonState();
            }
        });
    }

    /** Gets the button state for the contact details section. */
    protected int getContactDetailsButtonState() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mUI.getContactDetailsSectionForTest().getEditButtonState();
            }
        });
    }

    /**  Returns the label corresponding to the payment instrument at the specified |index|. */
    protected String getPaymentInstrumentLabel(final int index) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return ((OptionSection) mUI.getPaymentMethodSectionForTest())
                        .getOptionLabelsForTest(index).getText().toString();
            }
        });
    }

    /**
     *  Returns the label corresponding to the contact detail suggestion at the specified
     *  |suggestionIndex|.
     */
    protected String getContactDetailsSuggestionLabel(final int suggestionIndex)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return ((OptionSection) mUI.getContactDetailsSectionForTest())
                        .getOptionLabelsForTest(suggestionIndex).getText().toString();
            }
        });
    }

    /**  Returns the number of payment instruments. */
    protected int getNumberOfPaymentInstruments() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return ((OptionSection) mUI.getPaymentMethodSectionForTest())
                        .getNumberOfOptionLabelsForTest();
            }
        });
    }

    /**  Returns the number of contact detail suggestions. */
    protected int getNumberOfContactDetailSuggestions() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return ((OptionSection) mUI.getContactDetailsSectionForTest())
                        .getNumberOfOptionLabelsForTest();
            }
        });
    }

    /**
     *  Returns the label corresponding to the shipping address suggestion at the specified
     *  |suggestionIndex|.
     */
    protected String getShippingAddressSuggestionLabel(final int suggestionIndex)
            throws ExecutionException {
        assert (suggestionIndex < getNumberOfShippingAddressSuggestions());

        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return ((OptionSection) mUI.getShippingAddressSectionForTest())
                        .getOptionLabelsForTest(suggestionIndex).getText().toString();
            }
        });
    }

    /** Returns the focused view in the card editor view. */
    protected View getCardEditorFocusedView() {
        return mUI.getCardEditorView().getCurrentFocus();
    }

    /**
     *  Clicks on the label corresponding to the shipping address suggestion at the specified
     *  |suggestionIndex|.
     * @throws InterruptedException
     */
    protected void clickOnShippingAddressSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper)
                    throws ExecutionException, TimeoutException, InterruptedException {
        assert (suggestionIndex < getNumberOfShippingAddressSuggestions());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((OptionSection) mUI.getShippingAddressSectionForTest())
                        .getOptionLabelsForTest(suggestionIndex).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /**
     *  Clicks on the label corresponding to the payment method suggestion at the specified
     *  |suggestionIndex|.
     * @throws InterruptedException
     */
    protected void clickOnPaymentMethodSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper)
            throws ExecutionException, TimeoutException, InterruptedException {
        assert (suggestionIndex < getNumberOfPaymentInstruments());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((OptionSection) mUI.getPaymentMethodSectionForTest())
                        .getOptionLabelsForTest(suggestionIndex)
                        .performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /**
     *  Clicks on the edit icon corresponding to the payment method suggestion at the specified
     *  |suggestionIndex|.
     */
    protected void clickOnPaymentMethodSuggestionEditIconAndWait(
            final int suggestionIndex, CallbackHelper helper)
            throws ExecutionException, TimeoutException, InterruptedException {
        assert (suggestionIndex < getNumberOfPaymentInstruments());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((OptionSection) mUI.getPaymentMethodSectionForTest())
                        .getOptionRowAtIndex(suggestionIndex)
                        .getEditIconForTest()
                        .performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    /**
     *  Returns the the number of shipping address suggestions.
     */
    protected int getNumberOfShippingAddressSuggestions() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return ((OptionSection) mUI.getShippingAddressSectionForTest())
                        .getNumberOfOptionLabelsForTest();
            }
        });
    }

    /** Returns the {@link OptionRow} at the given index for the shipping address section. */
    protected OptionRow getShippingAddressOptionRowAtIndex(final int index)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<OptionRow>() {
            @Override
            public OptionRow call() {
                return ((OptionSection) mUI.getShippingAddressSectionForTest())
                        .getOptionRowAtIndex(index);
            }
        });
    }

    /** Returns the selected spinner value in the editor UI for credit cards. */
    protected String getSpinnerSelectionTextInCardEditor(final int dropdownIndex)
             throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getCardEditorView().getDropdownFieldsForTest().get(dropdownIndex)
                        .getSelectedItem().toString();
            }
        });
    }

    /** Returns the spinner value at the specified position in the editor UI for credit cards. */
    protected String getSpinnerTextAtPositionInCardEditor(
            final int dropdownIndex, final int itemPosition) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getCardEditorView()
                        .getDropdownFieldsForTest()
                        .get(dropdownIndex)
                        .getItemAtPosition(itemPosition)
                        .toString();
            }
        });
    }

    /** Returns the number of items offered by the spinner in the editor UI for credit cards. */
    protected int getSpinnerItemCountInCardEditor(final int dropdownIndex)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mUI.getCardEditorView().getDropdownFieldsForTest().get(dropdownIndex)
                        .getCount();
            }
        });
    }

    /** Returns the error message visible to the user in the credit card unmask prompt. */
    protected String getUnmaskPromptErrorMessage() {
        return mCardUnmaskPrompt.getErrorMessage();
    }

    /** Selects the spinner value in the editor UI for credit cards. */
    protected void setSpinnerSelectionsInCardEditorAndWait(final int[] selections,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                List<Spinner> fields = mUI.getCardEditorView().getDropdownFieldsForTest();
                for (int i = 0; i < selections.length && i < fields.size(); i++) {
                    fields.get(i).setSelection(selections[i]);
                }
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Selects the spinner value in the editor UI. */
    protected void setSpinnerSelectionInEditorAndWait(final int selection, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((Spinner) mUI.getEditorView().findViewById(R.id.spinner)).setSelection(selection);
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the editor UI for credit cards. */
    protected void setTextInCardEditorAndWait(final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ViewGroup contents = (ViewGroup)
                        mUI.getCardEditorView().findViewById(R.id.contents);
                assertNotNull(contents);
                for (int i = 0, j = 0; i < contents.getChildCount() && j < values.length; i++) {
                    View view = contents.getChildAt(i);
                    if (view instanceof EditorTextField) {
                        ((EditorTextField) view).getEditText().setText(values[j++]);
                    }
                }
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the editor UI. */
    protected void setTextInEditorAndWait(final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                List<EditText> fields = mUI.getEditorView().getEditableTextFieldsForTest();
                for (int i = 0; i < values.length; i++) {
                    fields.get(i).setText(values[i]);
                }
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the checkbox selection in the editor UI for credit cards. */
    protected void selectCheckboxAndWait(final int resourceId, final boolean isChecked,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((CheckBox) mUI.getCardEditorView().findViewById(resourceId)).setChecked(isChecked);
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the card unmask UI. */
    protected void setTextInCardUnmaskDialogAndWait(final int resourceId, final String input,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                EditText editText =
                        ((EditText) mCardUnmaskPrompt.getDialogForTest().findViewById(resourceId));
                editText.setText(input);
                editText.getOnFocusChangeListener().onFocusChange(null, false);
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Directly sets the text in the expired card unmask UI. */
    protected void setTextInExpiredCardUnmaskDialogAndWait(
            final int[] resourceIds, final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        assert resourceIds.length == values.length;
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < resourceIds.length; ++i) {
                    EditText editText = ((EditText) mCardUnmaskPrompt.getDialogForTest()
                            .findViewById(resourceIds[i]));
                    editText.setText(values[i]);
                    editText.getOnFocusChangeListener().onFocusChange(null, false);
                }
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Verifies the contents of the test webpage. */
    protected void expectResultContains(final String[] contents) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    String result = DOMUtils.getNodeContents(mWebContentsRef.get(), "result");
                    if (result == null) {
                        updateFailureReason("Cannot find 'result' node on test page");
                        return false;
                    }
                    for (int i = 0; i < contents.length; i++) {
                        if (!result.contains(contents[i])) {
                            updateFailureReason(
                                    String.format("Result should contain '%s'", contents[i]));
                            return false;
                        }
                    }
                    return true;
                } catch (InterruptedException e1) {
                    updateFailureReason(e1.getMessage());
                    return false;
                } catch (TimeoutException e2) {
                    updateFailureReason(e2.getMessage());
                    return false;
                }
            }
        });
    }

    /**  Will fail if the OptionRow at |index| is not selected in Contact Details.*/
    protected void expectContactDetailsRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                boolean isSelected = ((OptionSection) mUI.getContactDetailsSectionForTest())
                                             .getOptionRowAtIndex(index)
                                             .isChecked();
                if (!isSelected) {
                    updateFailureReason("Contact Details row at " + index + " was not selected.");
                }
                return isSelected;
            }
        });
    }

    /**  Will fail if the OptionRow at |index| is not selected in Shipping Address section.*/
    protected void expectShippingAddressRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                boolean isSelected = ((OptionSection) mUI.getShippingAddressSectionForTest())
                                             .getOptionRowAtIndex(index)
                                             .isChecked();
                if (!isSelected) {
                    updateFailureReason("Shipping Address row at " + index + " was not selected.");
                }
                return isSelected;
            }
        });
    }

    /**  Will fail if the OptionRow at |index| is not selected in PaymentMethod section.*/
    protected void expectPaymentMethodRowIsSelected(final int index) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                boolean isSelected = ((OptionSection) mUI.getPaymentMethodSectionForTest())
                                             .getOptionRowAtIndex(index)
                                             .isChecked();
                if (!isSelected) {
                    updateFailureReason("Payment Method row at " + index + " was not selected.");
                }
                return isSelected;
            }
        });
    }

    @Override
    public void onPaymentRequestReadyForInput(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mReadyForInput.notifyCalled(ui);
    }

    @Override
    public void onPaymentRequestReadyToEdit() {
        ThreadUtils.assertOnUiThread();
        mReadyToEdit.notifyCalled();
    }

    @Override
    public void onPaymentRequestEditorValidationError() {
        ThreadUtils.assertOnUiThread();
        mEditorValidationError.notifyCalled();
    }

    @Override
    public void onPaymentRequestEditorTextUpdate() {
        ThreadUtils.assertOnUiThread();
        mEditorTextUpdate.notifyCalled();
    }

    @Override
    public void onPaymentRequestReadyToPay(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mReadyToPay.notifyCalled(ui);
    }

    @Override
    public void onPaymentRequestSelectionChecked(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mSelectionChecked.notifyCalled(ui);
    }

    @Override
    public void onPaymentRequestResultReady(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mResultReady.notifyCalled(ui);
    }

    @Override
    public void onPaymentRequestDismiss() {
        ThreadUtils.assertOnUiThread();
        mDismissed.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceUnableToAbort() {
        ThreadUtils.assertOnUiThread();
        mUnableToAbort.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceBillingAddressChangeProcessed() {
        ThreadUtils.assertOnUiThread();
        mBillingAddressChangeProcessed.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceExpirationMonthChange() {
        ThreadUtils.assertOnUiThread();
        mExpirationMonthChange.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceShowFailed() {
        ThreadUtils.assertOnUiThread();
        mShowFailed.notifyCalled();
    }

    @Override
    public void onPaymentRequestServiceCanMakePaymentQueryResponded() {
        ThreadUtils.assertOnUiThread();
        mCanMakePaymentQueryResponded.notifyCalled();
    }

    @Override
    public void onCardUnmaskPromptReadyForInput(CardUnmaskPrompt prompt) {
        ThreadUtils.assertOnUiThread();
        mReadyForUnmaskInput.notifyCalled(prompt);
        mCardUnmaskPrompt = prompt;
    }

    @Override
    public void onCardUnmaskPromptReadyToUnmask(CardUnmaskPrompt prompt) {
        ThreadUtils.assertOnUiThread();
        mReadyToUnmask.notifyCalled(prompt);
    }

    @Override
    public void onCardUnmaskPromptValidationDone(CardUnmaskPrompt prompt) {
        ThreadUtils.assertOnUiThread();
        mUnmaskValidationDone.notifyCalled(prompt);
    }

    /**
     * Listens for UI notifications.
     */
    protected static class PaymentsCallbackHelper<T> extends CallbackHelper {
        private T mTarget;

        /**
         * Returns the UI that is ready for input.
         *
         * @return The UI that is ready for input.
         */
        public T getTarget() {
            return mTarget;
        }

        /**
         * Called when the UI is ready for input.
         *
         * @param target The UI that is ready for input.
         */
        public void notifyCalled(T target) {
            ThreadUtils.assertOnUiThread();
            mTarget = target;
            notifyCalled();
        }
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
        installPaymentApp("https://bobpay.com", instrumentPresence, responseSpeed);
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
        installPaymentApp(methodName, instrumentPresence, responseSpeed, IMMEDIATE_CREATION);
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
        installPaymentApp(asList(methodName), instrumentPresence, responseSpeed, creationSpeed);
    }

    protected void installPaymentApp(final List<String> appMethodNames,
            final int instrumentPresence, final int responseSpeed, final int creationSpeed) {
        PaymentAppFactory.getInstance().addAdditionalFactory(new PaymentAppFactoryAddition() {
            @Override
            public void create(WebContents webContents, Set<String> methodNames,
                    final PaymentAppFactory.PaymentAppCreatedCallback callback) {
                final TestPay app = new TestPay(appMethodNames, instrumentPresence, responseSpeed);
                if (creationSpeed == IMMEDIATE_CREATION) {
                    callback.onPaymentAppCreated(app);
                    callback.onAllPaymentAppsCreated();
                } else {
                    new Handler().postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            callback.onPaymentAppCreated(app);
                            callback.onAllPaymentAppsCreated();
                        }
                    }, 100);
                }
            }
        });
    }

    /** A payment app implementation for test. */
    protected static class TestPay implements PaymentApp {
        private final List<String> mMethodNames;
        private final int mInstrumentPresence;
        private final int mResponseSpeed;
        private InstrumentsCallback mCallback;

        TestPay(List<String> methodNames, int instrumentPresence, int responseSpeed) {
            mMethodNames = methodNames;
            mInstrumentPresence = instrumentPresence;
            mResponseSpeed = responseSpeed;
        }

        @Override
        public void getInstruments(Map<String, PaymentMethodData> methodData, String origin,
                byte[][] certificateChain, InstrumentsCallback instrumentsCallback) {
            mCallback = instrumentsCallback;
            respond();
        }

        void respond() {
            final List<PaymentInstrument> instruments = new ArrayList<>();
            if (mInstrumentPresence == HAVE_INSTRUMENTS) {
                for (String methodName : mMethodNames) {
                    instruments.add(
                            new TestPayInstrument(getAppIdentifier(), methodName, methodName));
                }
            }
            Runnable instrumentsReady = new Runnable() {
                @Override
                public void run() {
                    ThreadUtils.assertOnUiThread();
                    mCallback.onInstrumentsReady(TestPay.this, instruments);
                }
            };
            if (mResponseSpeed == IMMEDIATE_RESPONSE) {
                instrumentsReady.run();
            } else if (mResponseSpeed == DELAYED_RESPONSE) {
                new Handler().postDelayed(instrumentsReady, 100);
            }
        }

        @Override
        public Set<String> getAppMethodNames() {
            Set<String> methodNames = new HashSet<>();
            methodNames.addAll(mMethodNames);
            return methodNames;
        }

        @Override
        public boolean supportsMethodsAndData(Map<String, PaymentMethodData> methodsAndData) {
            assert methodsAndData != null;
            Set<String> methodNames = new HashSet<>(methodsAndData.keySet());
            methodNames.retainAll(getAppMethodNames());
            return !methodNames.isEmpty();
        }

        @Override
        public String getAppIdentifier() {
            return TestPay.this.toString();
        }
    }

    /** A payment instrument implementation for test. */
    private static class TestPayInstrument extends PaymentInstrument {
        private final String mMethodName;

        TestPayInstrument(String appId, String methodName, String label) {
            super(appId + methodName, label, null, null);
            mMethodName = methodName;
        }

        @Override
        public Set<String> getInstrumentMethodNames() {
            Set<String> result = new HashSet<>();
            result.add(mMethodName);
            return result;
        }

        @Override
        public void invokePaymentApp(String merchantName, String origin, byte[][] certificateChain,
                Map<String, PaymentMethodData> methodData, PaymentItem total,
                List<PaymentItem> displayItems, Map<String, PaymentDetailsModifier> modifiers,
                InstrumentDetailsCallback detailsCallback) {
            detailsCallback.onInstrumentDetailsReady(
                    mMethodName, "{\"transaction\": 1337}");
        }

        @Override
        public void dismissInstrument() {}
    }
}
