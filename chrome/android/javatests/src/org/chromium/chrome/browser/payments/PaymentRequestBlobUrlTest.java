// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Web payments test for blob URL.  */
public class PaymentRequestBlobUrlTest extends PaymentRequestTestBase {
    public PaymentRequestBlobUrlTest() {
        super("payment_request_blob_url_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {}

    @MediumTest
    @Feature({"Payments"})
    public void test() throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNode("buy");
        assertWaitForPageScaleFactorMatch(2);
        expectResultContains(new String[] {"SecurityError: Failed to construct 'PaymentRequest': "
                + "Must be in a secure context"});
    }
}
