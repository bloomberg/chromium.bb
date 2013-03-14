// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

/**
 * Constants required for AutofillDialog.
 */
public class AutofillDialogConstants {

    // Section ID constants. This list should be kept in sync with enum DialogSection in
    // autofill_dialog_types.h

    public static final int SECTION_EMAIL = 0;
    // The Autofill-backed dialog uses separate CC and billing sections.
    public static final int SECTION_CC = 1;
    public static final int SECTION_BILLING = 2;
    // The wallet-backed dialog uses a combined CC and billing section.
    public static final int SECTION_CC_BILLING = 3;
    public static final int SECTION_SHIPPING = 4;
    public static final int NUM_SECTIONS = 5;


    // Filed Type ID constants. This list should be kept in sync with enum AutofillFieldType
    // in field_types.h

    // Server indication that it has no data for the requested field.
    public static final int NO_SERVER_DATA = 0;
    // Client indication that the text entered did not match anything in the
    // personal data.
    public static final int UNKNOWN_TYPE = 1;
    // The "empty" type indicates that the user hasn't entered anything
    // in this field.
    public static final int EMPTY_TYPE = 2;
    // Personal Information categorization types.
    public static final int NAME_FIRST = 3;
    public static final int NAME_MIDDLE = 4;
    public static final int NAME_LAST = 5;
    public static final int NAME_MIDDLE_INITIAL = 6;
    public static final int NAME_FULL = 7;
    public static final int NAME_SUFFIX = 8;
    public static final int EMAIL_ADDRESS = 9;
    public static final int PHONE_HOME_NUMBER = 10;
    public static final int PHONE_HOME_CITY_CODE = 11;
    public static final int PHONE_HOME_COUNTRY_CODE = 12;
    public static final int PHONE_HOME_CITY_AND_NUMBER = 13;
    public static final int PHONE_HOME_WHOLE_NUMBER = 14;

    // Work phone numbers (values [15,19]) are deprecated.

    // Fax numbers (values [20,24]) are deprecated in Chrome, but still supported
    // by the server.
    public static final int PHONE_FAX_NUMBER = 20;
    public static final int PHONE_FAX_CITY_CODE = 21;
    public static final int PHONE_FAX_COUNTRY_CODE = 22;
    public static final int PHONE_FAX_CITY_AND_NUMBER = 23;
    public static final int PHONE_FAX_WHOLE_NUMBER = 24;

    // Cell phone numbers (values [25, 29]) are deprecated.

    public static final int ADDRESS_HOME_LINE1 = 30;
    public static final int ADDRESS_HOME_LINE2 = 31;
    public static final int ADDRESS_HOME_APT_NUM = 32;
    public static final int ADDRESS_HOME_CITY = 33;
    public static final int ADDRESS_HOME_STATE = 34;
    public static final int ADDRESS_HOME_ZIP = 35;
    public static final int ADDRESS_HOME_COUNTRY = 36;
    public static final int ADDRESS_BILLING_LINE1 = 37;
    public static final int ADDRESS_BILLING_LINE2 = 38;
    public static final int ADDRESS_BILLING_APT_NUM = 39;
    public static final int ADDRESS_BILLING_CITY = 40;
    public static final int ADDRESS_BILLING_STATE = 41;
    public static final int ADDRESS_BILLING_ZIP = 42;
    public static final int ADDRESS_BILLING_COUNTRY = 43;

    // ADDRESS_SHIPPING values [44,50] are deprecated.

    public static final int CREDIT_CARD_NAME = 51;
    public static final int CREDIT_CARD_NUMBER = 52;
    public static final int CREDIT_CARD_EXP_MONTH = 53;
    public static final int CREDIT_CARD_EXP_2_DIGIT_YEAR = 54;
    public static final int CREDIT_CARD_EXP_4_DIGIT_YEAR = 55;
    public static final int CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR = 56;
    public static final int CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR = 57;
    public static final int CREDIT_CARD_TYPE = 58;
    public static final int CREDIT_CARD_VERIFICATION_CODE = 59;

    public static final int COMPANY_NAME = 60;

    // Generic type whose default value is known.
    public static final int FIELD_WITH_DEFAULT_VALUE = 61;

    // No new types can be added.

    public static final int MAX_VALID_FIELD_TYPE = 62;
}
