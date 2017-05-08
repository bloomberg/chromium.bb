// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Web payments test for data URL.  */
public class PaymentRequestDataUrlTest extends PaymentRequestTestBase {
    public PaymentRequestDataUrlTest() {
        super("data:text/html,<html><head>"
                + "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, "
                + "maximum-scale=1\"></head><body><button id=\"buy\" onclick=\"try { "
                + "(new PaymentRequest([{supportedMethods: ['basic-card']}], "
                + "{total: {label: 'Total', "
                + " amount: {currency: 'USD', value: '1.00'}}})).show(); "
                + "} catch(e) { "
                + "document.getElementById('result').innerHTML = e; "
                + "}\">Data URL Test</button><div id='result'></div></body></html>");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {}

    @MediumTest
    @Feature({"Payments"})
    public void test() throws InterruptedException, ExecutionException, TimeoutException {
        openPageAndClickNode("buy");
        expectResultContains(new String[] {"SecurityError: Failed to construct 'PaymentRequest': "
                + "Must be in a secure context"});
    }
}
