// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt.CardUnmaskObserverForTest;
import org.chromium.chrome.browser.payments.PaymentRequestImpl.PaymentRequestServiceObserverForTest;
import org.chromium.chrome.browser.payments.ui.EditorTextField;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A base integration test for payments.
 */
@CommandLineFlags.Add({ChromeSwitches.EXPERIMENTAL_WEB_PLAFTORM_FEATURES})
abstract class PaymentRequestTestBase extends ChromeActivityTestCaseBase<ChromeActivity>
        implements PaymentRequestObserverForTest, PaymentRequestServiceObserverForTest,
        CardUnmaskObserverForTest {
    protected final PaymentsCallbackHelper<PaymentRequestUI> mReadyForInput;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mReadyToPay;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mReadyToClose;
    protected final PaymentsCallbackHelper<PaymentRequestUI> mResultReady;
    protected final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyForUnmaskInput;
    protected final PaymentsCallbackHelper<CardUnmaskPrompt> mReadyToUnmask;
    protected final CallbackHelper mReadyToEdit;
    protected final CallbackHelper mEditorValidationError;
    protected final CallbackHelper mEditorTextUpdate;
    protected final CallbackHelper mEditorDismissed;
    protected final CallbackHelper mDismissed;
    protected final CallbackHelper mUnableToAbort;
    private final AtomicReference<ContentViewCore> mViewCoreRef;
    private final AtomicReference<WebContents> mWebContentsRef;
    private final String mTestFilePath;
    private PaymentRequestUI mUI;
    private CardUnmaskPrompt mCardUnmaskPrompt;

    protected PaymentRequestTestBase(String testFileName) {
        super(ChromeActivity.class);
        mReadyForInput = new PaymentsCallbackHelper<>();
        mReadyToPay = new PaymentsCallbackHelper<>();
        mReadyToClose = new PaymentsCallbackHelper<>();
        mResultReady = new PaymentsCallbackHelper<>();
        mReadyForUnmaskInput = new PaymentsCallbackHelper<>();
        mReadyToUnmask = new PaymentsCallbackHelper<>();
        mReadyToEdit = new CallbackHelper();
        mEditorValidationError = new CallbackHelper();
        mEditorTextUpdate = new CallbackHelper();
        mEditorDismissed = new CallbackHelper();
        mDismissed = new CallbackHelper();
        mUnableToAbort = new CallbackHelper();
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
        mUI = helper.getTarget();
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

    /** Selects the spinner value in the editor UI. */
    protected void setSpinnerSelectionInEditor(final int selection, CallbackHelper helper)
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

    /** Directly sets the text in the editor UI. */
    protected void setTextInEditorAndWait(final String[] values, CallbackHelper helper)
            throws InterruptedException, TimeoutException {
        int callCount = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                List<EditorTextField> fields = mUI.getEditorView().getEditorTextFields();
                for (int i = 0; i < values.length; i++) {
                    fields.get(i).getEditText().setText(values[i]);
                }
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
    public void onPaymentRequestEditorDismissed() {
        ThreadUtils.assertOnUiThread();
        mEditorDismissed.notifyCalled();
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
}
