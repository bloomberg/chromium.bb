// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
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
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

// TODO(yolandyan): move this class to its test rule once JUnit4 migration is over
final class PaymentRequestTestCommon implements PaymentRequestObserverForTest,
                                                PaymentRequestServiceObserverForTest,
                                                CardUnmaskObserverForTest {
    /** Flag for installing a payment app without instruments. */
    static final int NO_INSTRUMENTS = 0;

    /** Flag for installing a payment app with instruments. */
    static final int HAVE_INSTRUMENTS = 1;

    /** Flag for installing a fast payment app. */
    static final int IMMEDIATE_RESPONSE = 0;

    /** Flag for installing a slow payment app. */
    static final int DELAYED_RESPONSE = 1;

    /** Flag for immediately installing a payment app. */
    static final int IMMEDIATE_CREATION = 0;

    /** Flag for installing a payment app with a delay. */
    static final int DELAYED_CREATION = 1;

    /** The expiration month dropdown index for December. */
    static final int DECEMBER = 11;

    /** The expiration year dropdown index for the next year. */
    static final int NEXT_YEAR = 1;

    /** The billing address dropdown index for the first billing address. */
    static final int FIRST_BILLING_ADDRESS = 0;

    final PaymentsCallbackHelper<PaymentRequestUI> mReadyForInput;
    final PaymentsCallbackHelper<PaymentRequestUI> mReadyToPay;
    final PaymentsCallbackHelper<PaymentRequestUI> mSelectionChecked;
    final PaymentsCallbackHelper<PaymentRequestUI> mResultReady;
    final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyForUnmaskInput;
    final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyToUnmask;
    final PaymentsCallbackHelper<CardUnmaskPrompt> mUnmaskValidationDone;
    final CallbackHelper mReadyToEdit;
    final CallbackHelper mEditorValidationError;
    final CallbackHelper mEditorTextUpdate;
    final CallbackHelper mDismissed;
    final CallbackHelper mUnableToAbort;
    final CallbackHelper mBillingAddressChangeProcessed;
    final CallbackHelper mShowFailed;
    final CallbackHelper mCanMakePaymentQueryResponded;
    final CallbackHelper mExpirationMonthChange;
    PaymentRequestUI mUI;

    private final AtomicReference<ContentViewCore> mViewCoreRef;

    private final AtomicReference<WebContents> mWebContentsRef;

    private final String mTestFilePath;

    private CardUnmaskPrompt mCardUnmaskPrompt;

    private final PaymentRequestTestCommonCallback mCallback;

    PaymentRequestTestCommon(PaymentRequestTestCommonCallback callback, String testFileName) {
        mCallback = callback;
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
        mTestFilePath = testFileName.startsWith("data:")
                ? testFileName
                : UrlUtils.getIsolatedTestFilePath(
                          String.format("chrome/test/data/payments/%s", testFileName));
    }

    public void startMainActivity() throws InterruptedException {
        mCallback.startMainActivityWithURL(mTestFilePath);
    }

    private void openPage() throws InterruptedException, ExecutionException, TimeoutException {
        mCallback.onMainActivityStarted();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mViewCoreRef.set(mCallback.getActivity().getCurrentContentViewCore());
                mWebContentsRef.set(mViewCoreRef.get().getWebContents());
                PaymentRequestUI.setObserverForTest(PaymentRequestTestCommon.this);
                PaymentRequestImpl.setObserverForTest(PaymentRequestTestCommon.this);
                CardUnmaskPrompt.setObserverForTest(PaymentRequestTestCommon.this);
            }
        });
        mCallback.assertWaitForPageScaleFactorMatch(1);
    }

    protected void triggerUIAndWait(PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("buy", helper);
        mUI = helper.getTarget();
    }

    protected void openPageAndClickNodeAndWait(String nodeId, CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        openPage();
        clickNodeAndWait(nodeId, helper);
    }

    protected void openPageAndClickBuyAndWait(CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait("buy", helper);
    }

    protected void openPageAndClickNode(String nodeId)
            throws InterruptedException, ExecutionException, TimeoutException {
        openPage();
        DOMUtils.clickNode(mViewCoreRef.get(), nodeId);
    }

    protected void triggerUIAndWait(String nodeId, PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNodeAndWait(nodeId, helper);
        mUI = helper.getTarget();
    }

    protected void reTriggerUIAndWait(
            String nodeId, PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, TimeoutException {
        clickNodeAndWait(nodeId, helper);
        mUI = helper.getTarget();
    }

    protected void clickNodeAndWait(String nodeId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        DOMUtils.clickNode(mViewCoreRef.get(), nodeId);
        helper.waitForCallback(callCount);
    }

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

    protected void clickInCardEditorAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getCardEditorDialog().findViewById(resourceId).performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    protected void clickInEditorAndWait(final int resourceId, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mUI.getEditorDialog().findViewById(resourceId).performClick();
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
                mUI.getEditorDialog().dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
                mUI.getEditorDialog().dispatchKeyEvent(
                        new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK));
            }
        });
        helper.waitForCallback(callCount);
    }

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

    protected int getShippingAddressSectionButtonState() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mUI.getShippingAddressSectionForTest().getEditButtonState();
            }
        });
    }

    protected int getContactDetailsButtonState() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mUI.getContactDetailsSectionForTest().getEditButtonState();
            }
        });
    }

    protected String getPaymentInstrumentLabel(final int index) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return ((OptionSection) mUI.getPaymentMethodSectionForTest())
                        .getOptionLabelsForTest(index)
                        .getText()
                        .toString();
            }
        });
    }

    protected String getSelectedPaymentInstrumentLabel() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                OptionSection section = ((OptionSection) mUI.getPaymentMethodSectionForTest());
                int size = section.getNumberOfOptionLabelsForTest();
                for (int i = 0; i < size; i++) {
                    if (section.getOptionRowAtIndex(i).isChecked()) {
                        return section.getOptionRowAtIndex(i).getLabelText().toString();
                    }
                }
                return null;
            }
        });
    }

    protected String getOrderSummaryTotal() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getOrderSummaryTotalTextViewForTest().getText().toString();
            }
        });
    }

    protected String getContactDetailsSuggestionLabel(final int suggestionIndex)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return ((OptionSection) mUI.getContactDetailsSectionForTest())
                        .getOptionLabelsForTest(suggestionIndex)
                        .getText()
                        .toString();
            }
        });
    }

    protected int getNumberOfPaymentInstruments() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return ((OptionSection) mUI.getPaymentMethodSectionForTest())
                        .getNumberOfOptionLabelsForTest();
            }
        });
    }

    protected int getNumberOfContactDetailSuggestions() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return ((OptionSection) mUI.getContactDetailsSectionForTest())
                        .getNumberOfOptionLabelsForTest();
            }
        });
    }

    protected String getShippingAddressSuggestionLabel(final int suggestionIndex)
            throws ExecutionException {
        Assert.assertTrue(suggestionIndex < getNumberOfShippingAddressSuggestions());

        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getShippingAddressSectionForTest()
                        .getOptionLabelsForTest(suggestionIndex)
                        .getText()
                        .toString();
            }
        });
    }

    protected String getShippingAddressSummary() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getShippingAddressSectionForTest()
                        .getLeftSummaryLabelForTest()
                        .getText()
                        .toString();
            }
        });
    }

    protected String getShippingOptionSummary() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getShippingOptionSectionForTest()
                        .getLeftSummaryLabelForTest()
                        .getText()
                        .toString();
            }
        });
    }

    protected String getShippingOptionCostSummaryOnBottomSheet() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getShippingOptionSectionForTest()
                        .getRightSummaryLabelForTest()
                        .getText()
                        .toString();
            }
        });
    }

    protected View getCardEditorFocusedView() {
        return mUI.getCardEditorDialog().getCurrentFocus();
    }

    protected void clickOnShippingAddressSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper)
            throws ExecutionException, TimeoutException, InterruptedException {
        Assert.assertTrue(suggestionIndex < getNumberOfShippingAddressSuggestions());

        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((OptionSection) mUI.getShippingAddressSectionForTest())
                        .getOptionLabelsForTest(suggestionIndex)
                        .performClick();
            }
        });
        helper.waitForCallback(callCount);
    }

    protected void clickOnPaymentMethodSuggestionOptionAndWait(
            final int suggestionIndex, CallbackHelper helper)
            throws ExecutionException, TimeoutException, InterruptedException {
        Assert.assertTrue(suggestionIndex < getNumberOfPaymentInstruments());

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

    protected void clickOnPaymentMethodSuggestionEditIconAndWait(
            final int suggestionIndex, CallbackHelper helper)
            throws ExecutionException, TimeoutException, InterruptedException {
        Assert.assertTrue(suggestionIndex < getNumberOfPaymentInstruments());

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

    protected int getNumberOfShippingAddressSuggestions() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return ((OptionSection) mUI.getShippingAddressSectionForTest())
                        .getNumberOfOptionLabelsForTest();
            }
        });
    }

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

    protected String getSpinnerSelectionTextInCardEditor(final int dropdownIndex)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getCardEditorDialog()
                        .getDropdownFieldsForTest()
                        .get(dropdownIndex)
                        .getSelectedItem()
                        .toString();
            }
        });
    }

    protected String getSpinnerTextAtPositionInCardEditor(
            final int dropdownIndex, final int itemPosition) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return mUI.getCardEditorDialog()
                        .getDropdownFieldsForTest()
                        .get(dropdownIndex)
                        .getItemAtPosition(itemPosition)
                        .toString();
            }
        });
    }

    protected int getSpinnerItemCountInCardEditor(final int dropdownIndex)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mUI.getCardEditorDialog()
                        .getDropdownFieldsForTest()
                        .get(dropdownIndex)
                        .getCount();
            }
        });
    }

    protected String getUnmaskPromptErrorMessage() {
        return mCardUnmaskPrompt.getErrorMessage();
    }

    protected void setSpinnerSelectionsInCardEditorAndWait(final int[] selections,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                List<Spinner> fields = mUI.getCardEditorDialog().getDropdownFieldsForTest();
                for (int i = 0; i < selections.length && i < fields.size(); i++) {
                    fields.get(i).setSelection(selections[i]);
                }
            }
        });
        helper.waitForCallback(callCount);
    }

    protected void setSpinnerSelectionInEditorAndWait(final int selection, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((Spinner) mUI.getEditorDialog().findViewById(R.id.spinner))
                        .setSelection(selection);
            }
        });
        helper.waitForCallback(callCount);
    }

    protected void setTextInCardEditorAndWait(final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ViewGroup contents =
                        (ViewGroup) mUI.getCardEditorDialog().findViewById(R.id.contents);
                Assert.assertNotNull(contents);
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

    protected void setTextInEditorAndWait(final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                List<EditText> fields = mUI.getEditorDialog().getEditableTextFieldsForTest();
                for (int i = 0; i < values.length; i++) {
                    fields.get(i).setText(values[i]);
                }
            }
        });
        helper.waitForCallback(callCount);
    }

    protected void selectCheckboxAndWait(final int resourceId, final boolean isChecked,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((CheckBox) mUI.getCardEditorDialog().findViewById(resourceId))
                        .setChecked(isChecked);
            }
        });
        helper.waitForCallback(callCount);
    }

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

    protected void setTextInExpiredCardUnmaskDialogAndWait(
            final int[] resourceIds, final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        assert resourceIds.length == values.length;
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < resourceIds.length; ++i) {
                    EditText editText =
                            ((EditText) mCardUnmaskPrompt.getDialogForTest().findViewById(
                                    resourceIds[i]));
                    editText.setText(values[i]);
                    editText.getOnFocusChangeListener().onFocusChange(null, false);
                }
            }
        });
        helper.waitForCallback(callCount);
    }

    /* package */ void hitSoftwareKeyboardSubmitButtonAndWait(final int resourceId,
            CallbackHelper helper) throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                EditText editText =
                        (EditText) mCardUnmaskPrompt.getDialogForTest().findViewById(resourceId);
                editText.requestFocus();
                editText.onEditorAction(EditorInfo.IME_ACTION_DONE);
            }
        });
        helper.waitForCallback(callCount);
    }

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

    protected void assertOnlySpecificAbortMetricLogged(int abortReason) {
        for (int i = 0; i < AbortReason.MAX; ++i) {
            Assert.assertEquals(
                    String.format(Locale.getDefault(), "Found %d instead of %d", i, abortReason),
                    (i == abortReason ? 1 : 0),
                    RecordHistogram.getHistogramValueCountForTesting(
                            "PaymentRequest.CheckoutFunnel.Aborted", i));
        }
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
    static class PaymentsCallbackHelper<T> extends CallbackHelper {
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

    void installPaymentApp(int instrumentPresence, int responseSpeed) {
        installPaymentApp("https://bobpay.com", instrumentPresence, responseSpeed);
    }

    void installPaymentApp(String methodName, int instrumentPresence, int responseSpeed) {
        installPaymentApp(methodName, instrumentPresence, responseSpeed, IMMEDIATE_CREATION);
    }

    void installPaymentApp(final String appMethodName, final int instrumentPresence,
            final int responseSpeed, final int creationSpeed) {
        PaymentAppFactory.getInstance().addAdditionalFactory(new PaymentAppFactoryAddition() {
            @Override
            public void create(WebContents webContents, Set<String> methodNames,
                    final PaymentAppFactory.PaymentAppCreatedCallback callback) {
                final TestPay app = new TestPay(appMethodName, instrumentPresence, responseSpeed);
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
    static class TestPay implements PaymentApp {
        private final String mDefaultMethodName;
        private final int mInstrumentPresence;
        private final int mResponseSpeed;
        private InstrumentsCallback mCallback;

        TestPay(String defaultMethodName, int instrumentPresence, int responseSpeed) {
            mDefaultMethodName = defaultMethodName;
            mInstrumentPresence = instrumentPresence;
            mResponseSpeed = responseSpeed;
        }

        @Override
        public void getInstruments(Map<String, PaymentMethodData> methodData, String origin,
                String iframeOrigin, byte[][] certificateChain, PaymentItem total,
                InstrumentsCallback instrumentsCallback) {
            mCallback = instrumentsCallback;
            respond();
        }

        void respond() {
            final List<PaymentInstrument> instruments = new ArrayList<>();
            if (mInstrumentPresence == HAVE_INSTRUMENTS) {
                instruments.add(new TestPayInstrument(
                        getAppIdentifier(), mDefaultMethodName, mDefaultMethodName));
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
            Set<String> result = new HashSet<>();
            result.add(mDefaultMethodName);
            return result;
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

        @Override
        public int getAdditionalAppTextResourceId() {
            return 0;
        }
    }

    /** A payment instrument implementation for test. */
    private static class TestPayInstrument extends PaymentInstrument {
        private final String mDefaultMethodName;

        TestPayInstrument(String appId, String defaultMethodName, String label) {
            super(appId + defaultMethodName, label, null, null);
            mDefaultMethodName = defaultMethodName;
        }

        @Override
        public Set<String> getInstrumentMethodNames() {
            Set<String> result = new HashSet<>();
            result.add(mDefaultMethodName);
            return result;
        }

        @Override
        public void invokePaymentApp(String id, String merchantName, String origin,
                String iframeOrigin, byte[][] certificateChain,
                Map<String, PaymentMethodData> methodData, PaymentItem total,
                List<PaymentItem> displayItems, Map<String, PaymentDetailsModifier> modifiers,
                InstrumentDetailsCallback detailsCallback) {
            detailsCallback.onInstrumentDetailsReady(mDefaultMethodName, "{\"transaction\": 1337}");
        }

        @Override
        public void dismissInstrument() {}
    }

    public interface PaymentRequestTestCommonCallback {
        ChromeTabbedActivity getActivity();
        void onMainActivityStarted()
                throws InterruptedException, ExecutionException, TimeoutException;
        void startMainActivityWithURL(String url) throws InterruptedException;
        void assertWaitForPageScaleFactorMatch(float expectedScale);
    }
}
