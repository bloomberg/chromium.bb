// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;

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
            {"Jon Doe", "555-5555", "j@d.co", true,
             "Jon Doe", "555-5555", "j@d.co",
             "Jon Doe", "555-5555", "j@d.co"},
            {null, "555-5555", "j@d.co", true,
             "555-5555", "j@d.co", null,
             null, "555-5555", "j@d.co"},
            {"", "555-5555", "j@d.co", true,
             "555-5555", "j@d.co", null,
             null, "555-5555", "j@d.co"},
            {"Jon Doe", null, "j@d.co", true,
             "Jon Doe", "j@d.co", null,
             "Jon Doe", null, "j@d.co"},
            {"Jon Doe", "", "j@d.co", true,
             "Jon Doe", "j@d.co", null,
             "Jon Doe", null, "j@d.co"},
            {"Jon Doe", "555-5555", null, true,
             "Jon Doe", "555-5555", null,
             "Jon Doe", "555-5555", null},
            {"Jon Doe", "555-5555", "", true,
             "Jon Doe", "555-5555", null,
             "Jon Doe", "555-5555", null},
            {null, "555-5555", null, true,
             "555-5555", null, null,
             null, "555-5555", null},
            {"", "555-5555", "", true,
             "555-5555", null, null,
             null, "555-5555", null},
            {null, null, "j@d.co", true,
             "j@d.co", null, null,
             null, null, "j@d.co"},
            {"", "", "j@d.co", true,
             "j@d.co", null, null,
             null, null, "j@d.co"},
            {"", "555-5555", "", false,
             "555-5555", null, null,
             null, "555-5555", null}
        });
    }

    private final String mPayerName;
    private final String mPayerPhone;
    private final String mPayerEmail;
    private final boolean mIsComplete;
    private final String mExpectedLabel;
    private final String mExpectedSublabel;
    private final String mExpectedTertiaryLabel;
    private final String mExpectedPayerName;
    private final String mExpectedPayerEmail;
    private final String mExpectedPayerPhone;

    public AutofillContactTest(String payerName, String payerPhone, String payerEmail,
            boolean isComplete, String expectedLabel, String expectedSublabel,
            String expectedTertiaryLabel, String expectedPayerName, String expectedPayerPhone,
            String expectedPayerEmail) {
        mPayerName = payerName;
        mPayerPhone = payerPhone;
        mPayerEmail = payerEmail;
        mIsComplete = isComplete;
        mExpectedLabel = expectedLabel;
        mExpectedSublabel = expectedSublabel;
        mExpectedTertiaryLabel = expectedTertiaryLabel;
        mExpectedPayerName = expectedPayerName;
        mExpectedPayerPhone = expectedPayerPhone;
        mExpectedPayerEmail = expectedPayerEmail;
    }

    @Test
    public void test() {
        AutofillProfile profile = new AutofillProfile();
        AutofillContact contact =
                new AutofillContact(profile, mPayerName, mPayerPhone, mPayerEmail, mIsComplete);

        Assert.assertEquals(
                mIsComplete ? "Contact should be complete" : "Contact should be incomplete",
                mIsComplete, contact.isComplete());
        Assert.assertEquals("Contact's profile should be the same as passed into the constructor",
                profile, contact.getProfile());
        assertContact(profile.getGUID(), mExpectedPayerName, mExpectedPayerPhone,
                mExpectedPayerEmail, mExpectedLabel, mExpectedSublabel, mExpectedTertiaryLabel,
                contact);

        contact.completeContact("some-guid-here", "Jon Doe", "999-9999", "a@b.com");
        Assert.assertTrue("Contact should be complete", contact.isComplete());
        assertContact("some-guid-here", "Jon Doe", "999-9999", "a@b.com",
                "Jon Doe", "999-9999", "a@b.com", contact);
    }

    private void assertContact(String id, String expectedName, String expectedPhone,
            String expectedEmail, String expectedLabel, String expectedSublabel,
            String expectedTertiaryLabel, AutofillContact actual) {
        Assert.assertEquals("Identifier should be " + id, id, actual.getIdentifier());
        Assert.assertEquals("Name should be " + expectedName, expectedName, actual.getPayerName());
        Assert.assertEquals(
                "Phone should be " + expectedPhone, expectedPhone, actual.getPayerPhone());
        Assert.assertEquals(
                "Email should be " + expectedEmail, expectedEmail, actual.getPayerEmail());
        Assert.assertEquals("Label should be " + expectedLabel, expectedLabel, actual.getLabel());
        Assert.assertEquals(
                "Sublabel should be " + expectedSublabel, expectedSublabel, actual.getSublabel());
        Assert.assertEquals("TertiaryLabel should be " + expectedTertiaryLabel,
                expectedTertiaryLabel, actual.getTertiaryLabel());
    }
}
