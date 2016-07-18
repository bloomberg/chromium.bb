// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

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
            {"555-5555", "j@d.co", true, "555-5555", "j@d.co", "j@d.co", "555-5555"},
            {null, "j@d.co", true, "j@d.co", null, "j@d.co", null},
            {"", "j@d.co", true, "j@d.co", null, "j@d.co", null},
            {"555-5555", null, true, "555-5555", null, null, "555-5555"},
            {"555-5555", "", false, "555-5555", null, null, "555-5555"},
        });
    }

    private final String mPayerPhone;
    private final String mPayerEmail;
    private final boolean mIsComplete;
    private final String mExpectedLabel;
    private final String mExpectedSublabel;
    private final String mExpectedPayerEmail;
    private final String mExpectedPayerPhone;

    public AutofillContactTest(String payerPhone, String payerEmail, boolean isComplete,
            String expectedLabel, String expectedSublabel, String expectedPayerEmail,
            String expectedPayerPhone) {
        mPayerPhone = payerPhone;
        mPayerEmail = payerEmail;
        mIsComplete = isComplete;
        mExpectedLabel = expectedLabel;
        mExpectedSublabel = expectedSublabel;
        mExpectedPayerEmail = expectedPayerEmail;
        mExpectedPayerPhone = expectedPayerPhone;
    }

    @Test
    public void test() {
        AutofillProfile profile = new AutofillProfile();
        AutofillContact contact =
                new AutofillContact(profile, mPayerPhone, mPayerEmail, mIsComplete);

        Assert.assertEquals(
                mIsComplete ? "Contact should be complete" : "Contact should be incomplete",
                mIsComplete, contact.isComplete());
        Assert.assertEquals("Contact's profile should be the same as passed into the constructor",
                profile, contact.getProfile());
        assertIdPhoneEmailLabelSublabel(profile.getGUID(), mExpectedPayerPhone, mExpectedPayerEmail,
                mExpectedLabel, mExpectedSublabel, contact);

        contact.completeContact("some-guid-here", "999-9999", "a@b.com");
        Assert.assertTrue("Contact should be complete", contact.isComplete());
        assertIdPhoneEmailLabelSublabel("some-guid-here", "999-9999", "a@b.com", "999-9999",
                "a@b.com", contact);
    }

    private void assertIdPhoneEmailLabelSublabel(String id, String expectedPhone,
            String expectedEmail, String expectedLabel, String expectedSublabel,
            AutofillContact actual) {
        Assert.assertEquals("Identifier should be " + id, id, actual.getIdentifier());
        Assert.assertEquals(
                "Phone should be " + expectedPhone, expectedPhone, actual.getPayerPhone());
        Assert.assertEquals(
                "Email should be " + expectedEmail, expectedEmail, actual.getPayerEmail());
        Assert.assertEquals("Label should be " + expectedLabel, expectedLabel, actual.getLabel());
        Assert.assertEquals(
                "Sublabel should be " + expectedSublabel, expectedSublabel, actual.getSublabel());
    }
}
