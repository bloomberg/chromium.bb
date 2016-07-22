// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
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
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojom.payments.PaymentItem;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A base integration test for payments.
 */
abstract class PaymentRequestTestBase extends ChromeActivityTestCaseBase<ChromeActivity>
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

    protected final PaymentsCallbackHelper<PaymentRequestUI> mReadyForInput;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mReadyToPay;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mReadyToClose;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mSelectionChecked;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mResultReady;
    protected final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyForUnmaskInput;
    protected final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyToUnmask;
    protected final CallbackHelper mReadyToEdit;
    protected final CallbackHelper mEditorValidationError;
    protected final CallbackHelper mEditorTextUpdate;
    protected final CallbackHelper mDismissed;
    protected final CallbackHelper mUnableToAbort;
    protected final CallbackHelper mBillingAddressChangeProcessed;
    protected final CallbackHelper mShowFailed;
    protected PaymentRequestUI mUI;

    private final AtomicReference<ContentViewCore> mViewCoreRef;
    private final AtomicReference<WebContents> mWebContentsRef;
    private final String mTestFilePath;
    private CardUnmaskPrompt mCardUnmaskPrompt;

    protected PaymentRequestTestBase(String testFileName) {
        super(ChromeActivity.class);
        mReadyForInput = new PaymentsCallbackHelper<>();
        mReadyToPay = new PaymentsCallbackHelper<>();
        mReadyToClose = new PaymentsCallbackHelper<>();
        mSelectionChecked = new PaymentsCallbackHelper<>();
        mResultReady = new PaymentsCallbackHelper<>();
        mReadyForUnmaskInput = new PaymentsCallbackHelper<>();
        mReadyToUnmask = new PaymentsCallbackHelper<>();
        mReadyToEdit = new CallbackHelper();
        mEditorValidationError = new CallbackHelper();
        mEditorTextUpdate = new CallbackHelper();
        mDismissed = new CallbackHelper();
        mUnableToAbort = new CallbackHelper();
        mBillingAddressChangeProcessed = new CallbackHelper();
        mShowFailed = new CallbackHelper();
        mViewCoreRef = new AtomicReference<>();
        mWebContentsRef = new AtomicReference<>();
        mTestFilePath = UrlUtils.getIsolatedTestFilePath(
                String.format("chrome/test/data/android/payments/%s", testFileName));
    }

    @Override
    public void startMainActivity() throws InterruptedException {}

    protected abstract void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException;

    protected void triggerUIAndWait(PaymentsCallbackHelper<PaymentRequestUI> helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        triggerUIAndWait((CallbackHelper) helper);
        mUI = helper.getTarget();
    }

    protected void triggerUIAndWait(CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        startMainActivityWithURL(mTestFilePath);
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
        clickNodeAndWait("buy", helper);
    }

    /** Clicks on an HTML node. */
    protected void clickNodeAndWait(String nodeId, CallbackHelper helper)
            throws InterruptedException, ExecutionException, TimeoutException {
        int callCount = helper.getCallCount();
        DOMUtils.clickNode(this, mViewCoreRef.get(), nodeId);
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

    /** Returns the left summary label of the "Shipping summary" section. */
    protected String getAddressSectionLabel() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<String>() {
            @Override
            public String call() {
                return ((TextView) mUI.getShippingSummarySectionForTest().findViewById(
                        R.id.payments_left_summary_label)).getText().toString();
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

    /**  Returns the the number of payment instruments. */
    protected int getNumberOfPaymentInstruments() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() {
                return ((OptionSection) mUI.getPaymentMethodSectionForTest())
                        .getNumberOfOptionLabelsForTest();
            }
        });
    }

    /**  Returns the the number of contact detail suggestions. */
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
                ((EditText) mCardUnmaskPrompt.getDialogForTest().findViewById(resourceId))
                        .setText(input);
            }
        });
        helper.waitForCallback(callCount);
    }

    /** Verifies the contents of the test webpage. */
    protected void expectResultContains(final String[] contents) throws InterruptedException {
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
    public void onPaymentRequestReadyToClose(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mReadyToClose.notifyCalled(ui);
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
    public void onPaymentRequestServiceShowFailed() {
        ThreadUtils.assertOnUiThread();
        mShowFailed.notifyCalled();
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
         * @param ui The UI that is ready for input.
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
    protected void installPaymentApp(final int instrumentPresence, final int responseSpeed) {
        PaymentAppFactory.setAdditionalFactory(new PaymentAppFactoryAddition() {
            @Override
            public List<PaymentApp> create(WebContents webContents) {
                List<PaymentApp> additionalApps = new ArrayList<>();
                additionalApps.add(new BobPay(instrumentPresence, responseSpeed));
                return additionalApps;
            }
        });
    }

    /** A payment app implementation for test. */
    private static class BobPay implements PaymentApp {
        private final int mInstrumentPresence;
        private final int mResponseSpeed;

        BobPay(int instrumentPresence, int responseSpeed) {
            mInstrumentPresence = instrumentPresence;
            mResponseSpeed = responseSpeed;
        }

        @Override
        public void getInstruments(JSONObject details, final InstrumentsCallback
                instrumentsCallback) {
            final List<PaymentInstrument> instruments = new ArrayList<>();
            if (mInstrumentPresence == HAVE_INSTRUMENTS) instruments.add(new BobPayInstrument());
            Runnable instrumentsReady = new Runnable() {
                @Override
                public void run() {
                    ThreadUtils.assertOnUiThread();
                    instrumentsCallback.onInstrumentsReady(BobPay.this, instruments);
                }
            };
            if (mResponseSpeed == IMMEDIATE_RESPONSE) {
                instrumentsReady.run();
            } else {
                new Handler().postDelayed(instrumentsReady, 100);
            }
        }

        @Override
        public Set<String> getSupportedMethodNames() {
            Set<String> methodNames = new HashSet<>();
            methodNames.add("https://bobpay.com");
            return methodNames;
        }

        @Override
        public String getIdentifier() {
            return "https://bobpay.com";
        }
    }

    /** A payment instrument implementation for test. */
    private static class BobPayInstrument extends PaymentInstrument {
        BobPayInstrument() {
            super("https://bobpay.com", "Bob Pay", null, NO_ICON);
        }

        @Override
        public String getMethodName() {
            return "https://bobpay.com";
        }

        @Override
        public void getDetails(String merchantName, String origin, PaymentItem total,
                List<PaymentItem> cart, JSONObject details, DetailsCallback detailsCallback) {
            detailsCallback.onInstrumentDetailsReady(
                    "https://bobpay.com", "{\"transaction\": 1337}");
        }

        @Override
        public void dismiss() {}
    }
}
