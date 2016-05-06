// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.chrome.R;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that does not require shipping address.
 */
public class PaymentRequestNoShippingTest extends PaymentRequestTestBase {
    public PaymentRequestNoShippingTest() {
        super("payment_request_no_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {}

    @MediumTest
    public void testCloseDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyToClose);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    @MediumTest
    public void testEditAndCloseDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickAndWait(R.id.button_secondary, mReadyToClose);
        clickAndWait(R.id.close_button, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }

    @MediumTest
    public void testEditAndCancelDialog() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyForInput);
        clickAndWait(R.id.button_secondary, mReadyForInput);
        clickAndWait(R.id.button_secondary, mDismissed);
        expectResultContains(new String[] {"Request cancelled"});
    }
}
