// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.mojom.payments.ShippingAddress;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.regex.Pattern;

/**
 * Unit tests for the AutofillAddress class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillAddressTest {
    @Test
    public void testRegionCodePattern() {
        Pattern pattern = Pattern.compile(AutofillAddress.REGION_CODE_PATTERN);

        Assert.assertTrue(pattern.matcher("US").matches());
        Assert.assertTrue(pattern.matcher("GB").matches());

        Assert.assertFalse(pattern.matcher("USA").matches());
        Assert.assertFalse(pattern.matcher("gb").matches());
        Assert.assertFalse(pattern.matcher("U2").matches());
        Assert.assertFalse(pattern.matcher("").matches());
    }

    @Test
    public void testToShippingAddress() {
        AutofillAddress input = new AutofillAddress(new AutofillProfile("guid", "origin",
                true /* isLocal */, "full name", "company name", "street\naddress", "region",
                "locality", "dependent locality", "postal code", "sorting code", "US",
                "phone number", "email@address.com", "en-Latn-US"));
        ShippingAddress output = input.toShippingAddress();
        Assert.assertEquals("US", output.regionCode);
        Assert.assertArrayEquals(new String[]{"street", "address"}, output.addressLine);
        Assert.assertEquals("region", output.administrativeArea);
        Assert.assertEquals("locality", output.locality);
        Assert.assertEquals("dependent locality", output.dependentLocality);
        Assert.assertEquals("postal code", output.postalCode);
        Assert.assertEquals("sorting code", output.sortingCode);
        Assert.assertEquals("company name", output.organization);
        Assert.assertEquals("full name", output.recipient);
        Assert.assertEquals("en", output.languageCode);
        Assert.assertEquals("Latn", output.scriptCode);
    }

    @Test
    public void testToShippingAddressWithoutMatchingLanguageScriptCode() {
        AutofillAddress input = new AutofillAddress(new AutofillProfile("guid", "origin",
                true /* isLocal */, "full name", "company name", "street\naddress", "region",
                "locality", "dependent locality", "postal code", "sorting code", "US",
                "phone number", "email@address.com", "language-code"));
        ShippingAddress output = input.toShippingAddress();
        Assert.assertEquals("", output.languageCode);
        Assert.assertEquals("", output.scriptCode);
    }

    @Test
    public void testToShippingAddressWithoutAnyLanguageScriptCode() {
        AutofillAddress input = new AutofillAddress(new AutofillProfile("guid", "origin",
                true /* isLocal */, "full name", "company name", "street\naddress", "region",
                "locality", "dependent locality", "postal code", "sorting code", "US",
                "phone number", "email@address.com", ""));
        ShippingAddress output = input.toShippingAddress();
        Assert.assertEquals("", output.languageCode);
        Assert.assertEquals("", output.scriptCode);
    }

    @Test
    public void testToShippingAddressWithNullLanguageScriptCode() {
        AutofillAddress input = new AutofillAddress(new AutofillProfile("guid", "origin",
                true /* isLocal */, "full name", "company name", "street\naddress", "region",
                "locality", "dependent locality", "postal code", "sorting code", "US",
                "phone number", "email@address.com", null));
        ShippingAddress output = input.toShippingAddress();
        Assert.assertEquals("", output.languageCode);
        Assert.assertEquals("", output.scriptCode);
    }

    @Test
    public void testToShippingAddressTwice() {
        AutofillAddress input = new AutofillAddress(new AutofillProfile("guid", "origin",
                true /* isLocal */, "full name", "company name", "street\naddress", "region",
                "locality", "dependent locality", "postal code", "sorting code", "US",
                "phone number", "email@address.com", "en-Latn"));
        ShippingAddress output1 = input.toShippingAddress();
        ShippingAddress output2 = input.toShippingAddress();
        Assert.assertEquals("en", output1.languageCode);
        Assert.assertEquals("en", output2.languageCode);
        Assert.assertEquals("Latn", output1.scriptCode);
        Assert.assertEquals("Latn", output2.scriptCode);
    }
}
