// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

/**
 * An option that the user can select, e.g., a shipping option, a shipping address, or a payment
 * method.
 */
public class PaymentOption {
    /**
     * The placeholder value that indicates the absence of an icon for this option.
     */
    public static final int NO_ICON = 0;

    private final String mId;
    private final String mLabel;
    private final String mSublabel;
    private final int mIcon;

    /**
     * Constructs a payment option.
     *
     * @param id The identifier.
     * @param label The label.
     * @param sublabel The optional sublabel.
     * @param icon The drawable icon identifier or NO_ICON.
     */
    public PaymentOption(String id, String label, String sublabel, int icon) {
        mId = id;
        mLabel = label;
        mSublabel = sublabel;
        mIcon = icon;
    }

    /**
     * The non-human readable identifier for this option. For example, "standard_shipping" or the
     * GUID of an autofill card.
     */
    public String getIdentifier() {
        return mId;
    }

    /**
     * The primary label of this option. For example, “Visa***1234” or "2-day shipping".
     */
    public String getLabel() {
        return mLabel;
    }

    /**
     * The optional sublabel of this option. For example, “Expiration date: 12/2025”.
     */
    public String getSublabel() {
        return mSublabel;
    }

    /**
     * The identifier for the drawable icon for this payment option. For example,
     * R.drawable.visa_card_issuer_icon or NO_ICON.
     */
    public int getDrawableIconId() {
        return mIcon;
    }
}
