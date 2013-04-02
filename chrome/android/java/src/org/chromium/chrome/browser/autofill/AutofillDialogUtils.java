// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.widget.EditText;
import android.widget.Spinner;

import org.chromium.chrome.browser.autofill.AutofillDialogConstants;

import org.chromium.chrome.R;

/**
 * Utility class for converting section/field IDs to Android IDs for the AutofillDialog.
 */
public class AutofillDialogUtils {

    // The default invalid ID to return when no such field/section is present.
    public static final int INVALID_ID = -1;
    public static final int INVALID_SECTION = -1;

    private AutofillDialogUtils() {
    }

    /**
     * Returns the {@link Spinner} ID for the given section in the AutofillDialog
     * layout
     * @param section The section to return the spinner ID for.
     * @return The Android ID for the spinner dropdown for the given section.
     */
    public static int getSpinnerIDForSection(int section) {
        switch (section) {
            case AutofillDialogConstants.SECTION_CC :
                return R.id.cc_spinner;
            case AutofillDialogConstants.SECTION_CC_BILLING :
                return R.id.cc_billing_spinner;
            case AutofillDialogConstants.SECTION_SHIPPING :
                return R.id.address_spinner;
            case AutofillDialogConstants.SECTION_BILLING :
                return R.id.billing_spinner;
            case AutofillDialogConstants.SECTION_EMAIL :
                return R.id.email_spinner;
            default:
                assert(false);
                return INVALID_ID;
        }
    }

    /**
     * Returns the section that has a spinner with the given id.
     * @param id The Android ID to look up.
     * @return The section with a spinner with the given ID.
     */
    public static int getSectionForSpinnerID(int id) {
        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            if (getSpinnerIDForSection(i) == id) return i;
        }
        return INVALID_SECTION;
    }

    /**
     * Returns whether the given section contains any credit card related
     * information.
     * @param section The section to check.
     * @return Whether the given section is related with credit card information.
     */
    public static boolean containsCreditCardInfo(int section) {
        return section == AutofillDialogConstants.SECTION_CC
                || section == AutofillDialogConstants.SECTION_CC_BILLING;
    }

    /**
     * Returns the {@link ViewGroup} ID for the given section in the AutofillDialog layout.
     * @param section The section to return the layout ID for.
     * @return The Android ID for the edit layout for the given section.
     */
    public static int getLayoutIDForSection(int section) {
        switch (section) {
            case AutofillDialogConstants.SECTION_CC :
                return R.id.editing_layout_credit_card;
            case AutofillDialogConstants.SECTION_CC_BILLING :
                return R.id.editing_layout_cc_billing;
            case AutofillDialogConstants.SECTION_SHIPPING :
                return R.id.editing_layout_shipping;
            case AutofillDialogConstants.SECTION_BILLING :
                return R.id.editing_layout_billing;
            case AutofillDialogConstants.SECTION_EMAIL :
                return INVALID_ID;
            default:
                assert(false);
                return INVALID_ID;
        }
    }

    /**
     * Returns the label ID for the given section in the AutofillDialog
     * layout
     * @param section The section to return the label ID for.
     * @return The Android ID for the label text for the given section.
     */
    public static int getLabelIDForSection(int section) {
        switch (section) {
            case AutofillDialogConstants.SECTION_CC :
                return R.id.cc_label;
            case AutofillDialogConstants.SECTION_CC_BILLING :
                return R.id.cc_billing_label;
            case AutofillDialogConstants.SECTION_SHIPPING :
                return R.id.shipping_label;
            case AutofillDialogConstants.SECTION_BILLING :
                return R.id.billing_label;
            case AutofillDialogConstants.SECTION_EMAIL :
                return R.id.email_label;
            default:
                assert(false);
                return INVALID_ID;
        }
    }

    /**
     * Returns the {@link EditText} ID for the given field in the given section in the
     * AutofillDialog layout.
     * @param section The section that the field belongs to.
     * @param field The field to return the Android ID for.
     * @return The Android ID corresponding to the field.
     */
    public static int getEditTextIDForField(int section, int field) {
        // TODO(yusufo) : Fill out all the fields returning 0 currently when their UI
        // elements are added to code.
        switch (section) {
            case AutofillDialogConstants.SECTION_EMAIL :
                return 0;
            case AutofillDialogConstants.SECTION_CC :
                switch (field) {
                    case AutofillDialogConstants.CREDIT_CARD_NUMBER :
                        return R.id.card_number;
                    case AutofillDialogConstants.CREDIT_CARD_EXP_MONTH :
                        return R.id.expiration_month;
                    case AutofillDialogConstants.CREDIT_CARD_EXP_4_DIGIT_YEAR :
                        return R.id.expiration_year;
                    case AutofillDialogConstants.CREDIT_CARD_VERIFICATION_CODE :
                        return R.id.cvc_code;
                    case AutofillDialogConstants.CREDIT_CARD_NAME :
                        return R.id.cardholder_name;
                    default:
                        assert(false);
                        return INVALID_ID;
                }
            case AutofillDialogConstants.SECTION_BILLING :
                switch (field) {
                    case AutofillDialogConstants.ADDRESS_HOME_LINE1 :
                        return R.id.billing_street_address_1;
                    case AutofillDialogConstants.ADDRESS_HOME_LINE2 :
                        return R.id.billing_street_address_2;
                    case AutofillDialogConstants.ADDRESS_HOME_CITY :
                        return R.id.billing_city;
                    case AutofillDialogConstants.ADDRESS_HOME_STATE :
                        return INVALID_ID;
                    case AutofillDialogConstants.ADDRESS_HOME_ZIP :
                        return R.id.billing_zip_code;
                    case AutofillDialogConstants.ADDRESS_HOME_COUNTRY :
                        return INVALID_ID;
                    case AutofillDialogConstants.PHONE_HOME_WHOLE_NUMBER :
                        return R.id.billing_phone_number;
                    default:
                        assert(false);
                        return INVALID_ID;
                }
            case AutofillDialogConstants.SECTION_CC_BILLING :
                switch (field) {
                    case AutofillDialogConstants.CREDIT_CARD_NUMBER :
                        return R.id.card_number;
                    case AutofillDialogConstants.CREDIT_CARD_EXP_MONTH :
                        return R.id.expiration_month;
                    case AutofillDialogConstants.CREDIT_CARD_EXP_4_DIGIT_YEAR :
                        return R.id.expiration_year;
                    case AutofillDialogConstants.CREDIT_CARD_VERIFICATION_CODE :
                        return R.id.cvc_code;
                    case AutofillDialogConstants.CREDIT_CARD_NAME :
                        return R.id.cardholder_name;
                    case AutofillDialogConstants.ADDRESS_HOME_LINE1 :
                        return R.id.billing_street_address_1;
                    case AutofillDialogConstants.ADDRESS_HOME_LINE2 :
                        return R.id.billing_street_address_2;
                    case AutofillDialogConstants.ADDRESS_HOME_CITY :
                        return R.id.billing_city;
                    case AutofillDialogConstants.ADDRESS_HOME_STATE :
                        return INVALID_ID;
                    case AutofillDialogConstants.ADDRESS_HOME_ZIP :
                        return R.id.billing_zip_code;
                    case AutofillDialogConstants.ADDRESS_HOME_COUNTRY :
                        return INVALID_ID;
                    case AutofillDialogConstants.PHONE_HOME_WHOLE_NUMBER :
                        return R.id.billing_phone_number;
                    default:
                        assert(false);
                        return INVALID_ID;
                }
            case AutofillDialogConstants.SECTION_SHIPPING :
                switch (field) {
                    case AutofillDialogConstants.NAME_FULL :
                        return R.id.recipient_name;
                    case AutofillDialogConstants.ADDRESS_HOME_LINE1 :
                        return R.id.shipping_street_address_1;
                    case AutofillDialogConstants.ADDRESS_HOME_LINE2 :
                        return R.id.shipping_street_address_2;
                    case AutofillDialogConstants.ADDRESS_HOME_CITY :
                        return R.id.shipping_city;
                    case AutofillDialogConstants.ADDRESS_HOME_STATE :
                        return INVALID_ID;
                    case AutofillDialogConstants.ADDRESS_HOME_ZIP :
                        return R.id.shipping_zip_code;
                    case AutofillDialogConstants.ADDRESS_HOME_COUNTRY :
                        return INVALID_ID;
                    case AutofillDialogConstants.PHONE_HOME_WHOLE_NUMBER :
                        return R.id.shipping_phone_number;
                    default:
                        assert(false);
                        return INVALID_ID;
                }
            default:
                assert(false);
                return INVALID_ID;
        }

    }
}
