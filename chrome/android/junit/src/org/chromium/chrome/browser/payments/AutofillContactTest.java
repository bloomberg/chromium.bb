// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import java.util.Arrays;
import java.util.Collection;

/**
 * Unit tests for the AutofillContact class.
 */
@RunWith(Parameterized.class)
public class AutofillContactTest {
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {
            {"j@d.co", "555-5555", "555-5555", "j@d.co", "j@d.co", "555-5555"},
            {"j@d.co", null, "j@d.co", null, "j@d.co", null},
            {null, "555-5555", "555-5555", null, null, "555-5555"},
        });
    }

    private final String mPayerEmail;
    private final String mPayerPhone;
    private final String mExpectedLabel;
    private final String mExpectedSublabel;
    private final String mExpectedPayerEmail;
    private final String mExpectedPayerPhone;

    public AutofillContactTest(String payerEmail, String payerPhone, String expectedLabel,
            String expectedSublabel, String expectedPayerEmail, String expectedPayerPhone) {
        mPayerEmail = payerEmail;
        mPayerPhone = payerPhone;
        mExpectedLabel = expectedLabel;
        mExpectedSublabel = expectedSublabel;
        mExpectedPayerEmail = expectedPayerEmail;
        mExpectedPayerPhone = expectedPayerPhone;
    }

    @Test
    public void test() {
        AutofillContact contact = new AutofillContact(mPayerEmail, mPayerPhone);

        Assert.assertEquals("Label should be " + mExpectedLabel,
                mExpectedLabel, contact.getLabel());
        Assert.assertEquals("Sublabel should be " + mExpectedSublabel,
                mExpectedSublabel, contact.getSublabel());
        Assert.assertEquals("Email should be " + mExpectedPayerEmail,
                mExpectedPayerEmail, contact.getPayerEmail());
        Assert.assertEquals("Phone should be " + mExpectedPayerPhone,
                mExpectedPayerPhone, contact.getPayerPhone());
    }
}
