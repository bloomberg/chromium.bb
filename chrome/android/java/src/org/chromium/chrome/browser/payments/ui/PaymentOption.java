// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import javax.annotation.Nullable;

/**
 * An option that the user can select, e.g., a shipping option, a shipping address, or a payment
 * method.
 */
public class PaymentOption {
    /** The placeholder value that indicates the absence of an icon for this option. */
    public static final int NO_ICON = 0;

    private final String mId;
    private final int mIcon;
    @Nullable private String mLabel;
    @Nullable private String mSublabel;
    private boolean mIsValid = true;

    /**
     * Constructs a payment option.
     *
     * @param id The identifier.
     * @param label The label.
     * @param sublabel The optional sublabel.
     * @param icon The drawable icon identifier or NO_ICON.
     */
    public PaymentOption(String id, @Nullable String label, @Nullable String sublabel, int icon) {
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
    @Nullable public String getLabel() {
        return mLabel;
    }

    /**
     * The optional sublabel of this option. For example, “Expiration date: 12/2025”.
     */
    @Nullable public String getSublabel() {
        return mSublabel;
    }

    /**
     * Updates the label and sublabel of this option. Called after the user has edited this option.
     *
     * @param label    The new label to use. Should not be null.
     * @param sublabel The new sublabel to use. Can be null.
     */
    protected void updateLabels(String label, @Nullable String sublabel) {
        mLabel = label;
        mSublabel = sublabel;
    }

    /**
     * The identifier for the drawable icon for this payment option. For example,
     * R.drawable.visa_card_issuer_icon or NO_ICON.
     */
    public int getDrawableIconId() {
        return mIcon;
    }

    /**
     * Marks this option as invalid. For example, this can be a shipping address that's not served
     * by the merchant.
     */
    public void setInvalid() {
        mIsValid = false;
    }

    /** @return True if this option is valid. */
    public boolean isValid() {
        return mIsValid;
    }
}
