// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A base integration test for payments.
 */
@CommandLineFlags.Add({ChromeSwitches.EXPERIMENTAL_WEB_PLAFTORM_FEATURES})
abstract class PaymentRequestTestBase extends ChromeActivityTestCaseBase<ChromeActivity>
        implements PaymentRequestObserverForTest {
    protected final PaymentsCallbackHelper mReadyForInput = new PaymentsCallbackHelper();
    protected final PaymentsCallbackHelper mReadyToClose = new PaymentsCallbackHelper();
    protected final CallbackHelper mDismissed = new CallbackHelper();
    private final AtomicReference<ContentViewCore> mViewCoreRef =
            new AtomicReference<ContentViewCore>();
    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<WebContents>();
    private final String mTestFilePath;
    private PaymentRequestUI mUI;

    protected PaymentRequestTestBase(String testFileName) {
        super(ChromeActivity.class);
        mTestFilePath = UrlUtils.getIsolatedTestFilePath(
                String.format("chrome/test/data/android/payments/%s", testFileName));
    }

    @Override
    public void startMainActivity() throws InterruptedException {}

    protected abstract void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException;

    protected void triggerUIAndWait(PaymentsCallbackHelper helper) throws InterruptedException,
            ExecutionException, TimeoutException {
        startMainActivityWithURL(mTestFilePath);
        onMainActivityStarted();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mViewCoreRef.set(getActivity().getCurrentContentViewCore());
                mWebContentsRef.set(mViewCoreRef.get().getWebContents());
                PaymentRequestUI.setObserverForTest(PaymentRequestTestBase.this);
            }
        });
        assertWaitForPageScaleFactorMatch(1);

        int callCount = helper.getCallCount();
        DOMUtils.clickNode(this, mViewCoreRef.get(), "buy");
        helper.waitForCallback(callCount);
        mUI = helper.getUI();
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
    public void onPaymentRequestReadyToClose(PaymentRequestUI ui) {
        ThreadUtils.assertOnUiThread();
        mReadyToClose.notifyCalled(ui);
    }

    @Override
    public void onPaymentRequestDismiss() {
        ThreadUtils.assertOnUiThread();
        mDismissed.notifyCalled();
    }

    /**
     * Listens for payments UI being ready for input or closing.
     */
    protected static class PaymentsCallbackHelper extends CallbackHelper {
        private PaymentRequestUI mUI;

        /**
         * Returns the payments UI that is ready for input.
         *
         * @return The payments UI that is ready for input.
         */
        public PaymentRequestUI getUI() {
            ThreadUtils.assertOnUiThread();
            return mUI;
        }

        /**
         * Called when the payments UI is ready for input.
         *
         * @param ui The payments UI that is ready for input or closing.
         */
        public void notifyCalled(PaymentRequestUI ui) {
            ThreadUtils.assertOnUiThread();
            mUI = ui;
            notifyCalled();
        }
    }
}
