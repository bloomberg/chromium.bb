// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.v7.app.AlertDialog;
import android.view.View;
import android.widget.EditText;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
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
abstract class PaymentRequestTestBase extends ChromeActivityTestCaseBase<ChromeActivity> {
    private final AtomicReference<ContentViewCore> mViewCoreRef =
            new AtomicReference<ContentViewCore>();
    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<WebContents>();
    private final String mTestFilePath;

    protected PaymentRequestTestBase(String testFileName) {
        super(ChromeActivity.class);
        mTestFilePath = UrlUtils.getIsolatedTestFilePath(
                String.format("chrome/test/data/android/payments/%s", testFileName));
    }

    @Override
    public void startMainActivity() throws InterruptedException {}

    protected abstract void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException;

    protected void triggerPaymentUI()
            throws InterruptedException, ExecutionException, TimeoutException {
        startMainActivityWithURL(mTestFilePath);
        onMainActivityStarted();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mViewCoreRef.set(getActivity().getCurrentContentViewCore());
                mWebContentsRef.set(mViewCoreRef.get().getWebContents());
            }
        });
        assertWaitForPageScaleFactorMatch(1);
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), "buy");
        DOMUtils.clickNode(this, mViewCoreRef.get(), "buy");
    }

    protected void clickClosePaymentUIButton() throws InterruptedException {
        clickPaymentUIButton("close", R.id.close_button);
    }

    protected void clickSecondaryPaymentUIButton() throws InterruptedException {
        clickPaymentUIButton("secondary", R.id.button_secondary);
    }

    protected void clickPrimaryPaymentUIButton() throws InterruptedException {
        clickPaymentUIButton("primary", R.id.button_primary);
    }

    private void clickPaymentUIButton(final String buttonName, final int resourceId)
            throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                PaymentRequestUI ui = PaymentRequestUI.getCurrentUIForTest();
                if (ui == null) {
                    updateFailureReason("Payment UI was not shown");
                    return false;
                }

                View button = ui.getDialogForTest().findViewById(resourceId);
                if (button == null) {
                    updateFailureReason(
                            String.format("Cannot find the %s button on payment UI", buttonName));
                    return false;
                }

                if (!button.isEnabled()) {
                    updateFailureReason(
                            String.format("The %s button on payment UI is disabled", buttonName));
                    return false;
                }

                ui.setShowResultDelayForTest(0);
                button.performClick();
                return true;
            }
        });
    }

    protected void waitForPaymentUIHidden() throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria("Payment UI should be hidden") {
            @Override
            public boolean isSatisfied() {
                return PaymentRequestUI.getCurrentUIForTest() == null;
            }
        });
    }

    protected void typeInCvc(final String cvc) throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                CardUnmaskPrompt prompt = CardUnmaskPrompt.getCurrentPromptForTest();
                if (prompt == null) {
                    updateFailureReason("CVC prompt is not shown");
                    return false;
                }

                EditText cvcInput =
                        (EditText) prompt.getDialogForTest().findViewById(R.id.card_unmask_input);
                if (cvcInput == null) {
                    updateFailureReason("Cannot find CVC input on the prompt");
                    return false;
                }

                if (!cvcInput.isEnabled()) {
                    updateFailureReason("CVC input is disabled");
                    return false;
                }

                cvcInput.setText(cvc);
                return true;
            }
        });
    }

    protected void clickPrimaryCVCPromptButton() throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                CardUnmaskPrompt prompt = CardUnmaskPrompt.getCurrentPromptForTest();
                if (prompt == null) {
                    updateFailureReason("CVC prompt was not shown");
                    return false;
                }

                View positiveButton =
                        prompt.getDialogForTest().getButton(AlertDialog.BUTTON_POSITIVE);
                if (positiveButton == null) {
                    updateFailureReason("Cannot find the positive button on the CVC prompt");
                    return false;
                }

                if (!positiveButton.isEnabled()) {
                    updateFailureReason("The positive button the CVC prompt is disabled");
                    return false;
                }

                prompt.setShowResultDelayForTest(0);
                positiveButton.performClick();
                return true;
            }
        });
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
}
