// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

/**
 * The data to show in the PaymentRequest UI when first showing the UI.
 */
public class PaymentInformation {
    private final LineItem mTotal;
    private final SectionInformation mShippingAddresses;
    private final SectionInformation mShippingOptions;
    private final SectionInformation mPaymentMethods;

    /**
     * Builds the default payment information to show in the initial PaymentRequest view.
     *
     * @param totalPrice The total price.
     * @param defaultShippingAddress The default shipping address.
     * @param defaultShippingOption The default shipping option.
     * @param defaultPaymentMethod The default payment method.
    */
    public PaymentInformation(LineItem totalPrice, PaymentOption defaultShippingAddress,
            PaymentOption defaultShippingOption, PaymentOption defaultPaymentMethod) {
        mTotal = totalPrice;
        mShippingAddresses = new SectionInformation(
                PaymentRequestUI.TYPE_SHIPPING_ADDRESSES, defaultShippingAddress);
        mShippingOptions = new SectionInformation(
                PaymentRequestUI.TYPE_SHIPPING_OPTIONS, defaultShippingOption);
        mPaymentMethods = new SectionInformation(
                PaymentRequestUI.TYPE_PAYMENT_METHODS, defaultPaymentMethod);
    }

    /**
     * Returns the total price on the bill.
     *
     * @return The total price on the bill.
     */
    public LineItem getTotal() {
        return mTotal;
    }

    /**
     * Returns the shipping addresses.
     *
     * @return The shipping addresses.
     */
    public SectionInformation getShippingAddresses() {
        return mShippingAddresses;
    }

    /**
     * Returns the label for the selected shipping address.
     *
     * @return The label for the selected shipping address or null.
     */
    public String getSelectedShippingAddressLabel() {
        PaymentOption address = mShippingAddresses.getSelectedItem();
        return address != null ? address.getLabel() : null;
    }

    /**
     * Returns the sublabel for the selected shipping address.
     *
     * @return The sublabel for the selected shipping address or null.
     */
    public String getSelectedShippingAddressSublabel() {
        PaymentOption address = mShippingAddresses.getSelectedItem();
        return address != null ? address.getSublabel() : null;
    }

    /**
     * Returns the shipping options.
     *
     * @return The shipping options.
     */
    public SectionInformation getShippingOptions() {
        return mShippingOptions;
    }

    /**
     * Returns the label for the selected shipping option.
     *
     * @return The label for the selected shipping option or null.
     */
    public String getSelectedShippingOptionLabel() {
        PaymentOption option = mShippingOptions.getSelectedItem();
        return option != null ? option.getLabel() : null;
    }

    /**
     * Returns the payment methods.
     *
     * @return The payment methods.
     */
    public SectionInformation getPaymentMethods() {
        return mPaymentMethods;
    }
}
