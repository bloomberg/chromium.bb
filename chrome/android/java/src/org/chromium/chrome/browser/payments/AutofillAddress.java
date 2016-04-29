// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.payments.ui.PaymentOption;
import org.chromium.mojom.payments.ShippingAddress;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The locally stored autofill address.
 */
public class AutofillAddress extends PaymentOption {
    /**
     * The pattern for a valid region code.
     */
    public static final String REGION_CODE_PATTERN = "^[A-Z]{2}$";

    // Language/script code pattern and capture group numbers.
    private static final String LANGUAGE_SCRIPT_CODE_PATTERN =
            "^([a-z]{2})(-([A-Z][a-z]{3}))?(-[A-Za-z]+)*$";
    private static final int LANGUAGE_CODE_GROUP = 1;
    private static final int SCRIPT_CODE_GROUP = 3;

    private final AutofillProfile mProfile;
    private Matcher mLanguageScriptCodeMatcher;

    /**
     * Builds for the autofill address.
     */
    public AutofillAddress(AutofillProfile profile) {
        super(profile.getGUID(), profile.getLabel(), profile.getFullName(), PaymentOption.NO_ICON);

        assert profile.getCountryCode() != null : "Country code should not be null";
        assert Pattern.compile(REGION_CODE_PATTERN).matcher(profile.getCountryCode()).matches()
                : "Country code should be in valid format";

        assert profile.getStreetAddress() != null : "Street address should not be null";
        assert profile.getRegion() != null : "Region should not be null";
        assert profile.getLocality() != null : "Locality should not be null";
        assert profile.getDependentLocality() != null : "Dependent locality should not be null";
        assert profile.getPostalCode() != null : "Postal code should not be null";
        assert profile.getSortingCode() != null : "Sorting code should not be null";
        assert profile.getCompanyName() != null : "Company name should not be null";
        assert profile.getFullName() != null : "Full name should not be null";

        mProfile = profile;
    }

    /**
     * Returns the shipping address for mojo.
     */
    public ShippingAddress toShippingAddress() {
        ShippingAddress result = new ShippingAddress();

        result.regionCode = mProfile.getCountryCode();
        result.addressLine = mProfile.getStreetAddress().split("\n");
        result.administrativeArea = mProfile.getRegion();
        result.locality = mProfile.getLocality();
        result.dependentLocality = mProfile.getDependentLocality();
        result.postalCode = mProfile.getPostalCode();
        result.sortingCode = mProfile.getSortingCode();
        result.organization = mProfile.getCompanyName();
        result.recipient = mProfile.getFullName();
        result.languageCode = "";
        result.scriptCode = "";

        if (mProfile.getLanguageCode() == null) return result;

        if (mLanguageScriptCodeMatcher == null) {
            mLanguageScriptCodeMatcher = Pattern.compile(LANGUAGE_SCRIPT_CODE_PATTERN)
                                                 .matcher(mProfile.getLanguageCode());
        }

        if (mLanguageScriptCodeMatcher.matches()) {
            String languageCode = mLanguageScriptCodeMatcher.group(LANGUAGE_CODE_GROUP);
            result.languageCode = languageCode != null ? languageCode : "";

            String scriptCode = mLanguageScriptCodeMatcher.group(SCRIPT_CODE_GROUP);
            result.scriptCode = scriptCode != null ? scriptCode : "";
        }

        return result;
    }
}
