// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Handler;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.AlwaysDismissedDialog;

import java.util.List;

/**
 * The PaymentRequest UI.
 */
public class PaymentRequestUI implements DialogInterface.OnDismissListener, View.OnClickListener {
    /**
     * The interface to be implemented by the consumer of the PaymentRequest UI.
     */
    public interface Client {
        /**
         * Asynchronously returns the default payment information.
         */
        void getDefaultPaymentInformation(Callback<PaymentInformation> callback);

        /**
         * Asynchronously returns the full bill. The last line item is the total.
         */
        void getLineItems(Callback<List<LineItem>> callback);

        /**
         * Asynchronously returns the full list of available shipping addresses.
         */
        void getShippingAddresses(Callback<SectionInformation> callback);

        /**
         * Asynchronously returns the full list of available shipping options.
         */
        void getShippingOptions(Callback<SectionInformation> callback);

        /**
         * Asynchronously returns the full list of available payment methods.
         */
        void getPaymentMethods(Callback<SectionInformation> callback);

        /**
         * Called when the user changes the current shipping address. This may update the line items
         * and/or the shipping options.
         */
        void onShippingAddressChanged(PaymentOption selectedShippingAddress);

        /**
         * Called when the user changes the current shipping option. This may update the line items.
         */
        void onShippingOptionChanged(PaymentOption selectedShippingOption);

        /**
         * Called when the user changes the current payment method.
         */
        void onPaymentMethodChanged(PaymentOption selectedPaymentMethod);

        /**
         * Called when the user clicks on the “Pay” button. At this point, the UI is disabled and is
         * showing a spinner.
         */
        void onPayClicked(PaymentOption selectedShippingAddress,
                PaymentOption selectedShippingOption, PaymentOption selectedPaymentMethod);

        /**
         * Called when the user dismisses the UI via the “back” button on their phone
         * or the “X” button in UI.
         */
        void onDismiss();
    }

    /**
     * The number of milliseconds to display "Payment processed" or "Error processing payment"
     * message.
     */
    private static final int SHOW_RESULT_DELAY_MS = 3000;

    private final Context mContext;
    private final Client mClient;
    private final boolean mRequestShipping;

    private final Dialog mDialog;
    private final ViewGroup mContainer;
    private final View mPaymentContainer;
    private final View mButtonBar;
    private final View mWaitingOverlay;
    private final View mWaitingProgressBar;
    private final View mWaitingSuccess;
    private final TextView mWaitingMessage;
    private final Button mEditButton;
    private final Button mPayButton;
    private final View mCloseButton;

    private final View mOrderSummaryLabel;
    private final ViewGroup mOrderSummary;

    private final View mShippingSummaryLabel;
    private final View mShippingSummary;
    private final TextView mShippingSummaryAddress;
    private final TextView mShippingSummaryOption;
    private final View mShippingSummarySeparator;

    private final View mShippingAddressesLabel;
    private final ViewGroup mShippingAddresses;
    private final View mShippingAddressesSeparator;

    private final View mShippingOptionsLabel;
    private final View mSelectShippingOptionPrompt;
    private final ViewGroup mShippingOptions;
    private final View mShippingOptionsSeparator;

    private final View mPaymentMethodsLabel;
    private final ViewGroup mPaymentMethods;

    private View mSelectedSectionLabel;
    private ViewGroup mSelectedSection;

    private List<LineItem> mLineItems;
    private SectionInformation mPaymentMethodsSection;
    private SectionInformation mShippingAddressesSection;
    private SectionInformation mShippingOptionsSection;

    /**
     * Builds and shows the UI for PaymentRequest.
     *
     * @param activity The activity on top of which the UI should be displayed.
     * @param client The consumer of the PaymentRequest UI.
     * @param requestShipping Whether the UI should show the shipping address and option selection.
     * @param title The title to show at the top of the UI. This can be, for example, the
     *              &lt;title&gt; of the merchant website. If the string is too long for UI,
     *              it elides at the end.
     * @param origin The origin (part of URL) to show under the title. For example,
     *               “https://shop.momandpop.com”. If the origin is too long for the UI,
     *               it should elide according to:
     * https://www.chromium.org/Home/chromium-security/enamel#TOC-Eliding-Origin-Names-And-Hostnames
     */
    public PaymentRequestUI(Activity activity, Client client, boolean requestShipping, String title,
            String origin) {
        mContext = activity;
        mClient = client;
        mRequestShipping = requestShipping;

        mContainer =
                (ViewGroup) LayoutInflater.from(mContext).inflate(R.layout.payment_request, null);
        ((TextView) mContainer.findViewById(R.id.pageTitle)).setText(title);
        ((TextView) mContainer.findViewById(R.id.hostname)).setText(origin);

        mPaymentContainer = mContainer.findViewById(R.id.paymentContainer);
        mButtonBar = mContainer.findViewById(R.id.buttonBar);
        mWaitingOverlay = mContainer.findViewById(R.id.waitingOverlay);
        mWaitingProgressBar = mContainer.findViewById(R.id.waitingProgressBar);
        mWaitingSuccess = mContainer.findViewById(R.id.waitingSuccess);
        mWaitingMessage = (TextView) mContainer.findViewById(R.id.waitingMessage);

        mEditButton = (Button) mContainer.findViewById(R.id.editButton);
        mEditButton.setOnClickListener(this);

        mPayButton = (Button) mContainer.findViewById(R.id.payButton);
        mPayButton.setOnClickListener(this);

        mCloseButton = mContainer.findViewById(R.id.close_button);
        mCloseButton.setOnClickListener(this);

        mOrderSummaryLabel = mContainer.findViewById(R.id.orderSummaryLabel);
        mOrderSummary = (ViewGroup) mContainer.findViewById(R.id.lineItems);
        mOrderSummaryLabel.setOnClickListener(this);
        mOrderSummary.setOnClickListener(this);

        mShippingSummaryLabel = mContainer.findViewById(R.id.shippingSummaryLabel);
        mShippingSummary = mContainer.findViewById(R.id.shippingSummary);
        mShippingSummaryAddress = (TextView) mContainer.findViewById(R.id.shippingSummaryAddress);
        mShippingSummaryOption = (TextView) mContainer.findViewById(R.id.shippingSummaryOption);
        mShippingSummarySeparator = mContainer.findViewById(R.id.shippingSummarySeparator);

        mShippingAddressesLabel = mContainer.findViewById(R.id.shippingAddressesLabel);
        mShippingAddresses = (ViewGroup) mContainer.findViewById(R.id.shippingAddresses);
        mShippingAddressesSeparator = mContainer.findViewById(R.id.shippingAddressesSeparator);

        mShippingOptionsLabel = mContainer.findViewById(R.id.shippingOptionsLabel);
        mSelectShippingOptionPrompt = mContainer.findViewById(R.id.selectShippingOptionPrompt);
        mShippingOptions = (ViewGroup) mContainer.findViewById(R.id.shippingOptions);
        mShippingOptionsSeparator = mContainer.findViewById(R.id.shippingOptionsSeparator);

        if (mRequestShipping) {
            mShippingSummaryLabel.setOnClickListener(this);
            mShippingSummaryAddress.setOnClickListener(this);
            mShippingAddressesLabel.setOnClickListener(this);
            mShippingAddresses.setOnClickListener(this);
            mShippingSummaryOption.setOnClickListener(this);
            mShippingOptionsLabel.setOnClickListener(this);
            mSelectShippingOptionPrompt.setOnClickListener(this);
            mShippingOptions.setOnClickListener(this);
        } else {
            mShippingSummaryLabel.setVisibility(View.GONE);
            mShippingSummary.setVisibility(View.GONE);
            mShippingSummarySeparator.setVisibility(View.GONE);
        }

        mPaymentMethodsLabel = mContainer.findViewById(R.id.paymentsListLabel);
        mPaymentMethods = (ViewGroup) mContainer.findViewById(R.id.paymentsList);
        mPaymentMethodsLabel.setOnClickListener(this);
        mPaymentMethods.setOnClickListener(this);

        mDialog = new AlwaysDismissedDialog(activity, R.style.DialogWhenLarge);
        mDialog.setOnDismissListener(this);
        mDialog.addContentView(
                mContainer, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));
        Window dialogWindow = mDialog.getWindow();
        dialogWindow.setGravity(Gravity.BOTTOM);
        dialogWindow.setLayout(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        mDialog.show();

        mClient.getDefaultPaymentInformation(new Callback<PaymentInformation>() {
            @Override
            public void onResult(PaymentInformation result) {
                updateLineItems(result.getLineItems());

                if (mRequestShipping) {
                    mShippingAddressesSection = result.getShippingAddresses();
                    String selectedAddressLabel = result.getSelectedShippingAddressLabel();
                    if (selectedAddressLabel != null) {
                        mShippingSummaryAddress.setText(selectedAddressLabel);
                    }

                    mShippingOptionsSection = result.getShippingOptions();
                    String selectedShippingOptionLabel = result.getSelectedShippingOptionLabel();
                    if (selectedShippingOptionLabel != null) {
                        mShippingSummaryOption.setText(selectedShippingOptionLabel);
                    } else {
                        mShippingSummaryOption.setText(mContext.getString(
                                R.string.payments_select_shipping_option_prompt));
                    }
                }

                mPaymentMethodsSection = result.getPaymentMethods();
                updateSection(mPaymentMethods, mPaymentMethodsSection, null);

                updatePayButtonEnabled();
            }
        });
    }

    /**
     * Closes the UI. Can be invoked in response to, for example:
     * <ul>
     *  <li>Successfully processing the payment.</li>
     *  <li>Failure to process the payment.</li>
     *  <li>The JavaScript calling the abort() method in PaymentRequest API.</li>
     *  <li>The PaymentRequest JavaScript object being destroyed.</li>
     * </ul>
     *
     * Does not call Client.onDismissed().
     *
     * Should not be called multiple times.
     *
     * @param paymentSuccess Whether the payment (if any) was successful.
     * @param callback The callback to notify of finished animations.
     */
    public void close(boolean paymentSuccess, final Runnable callback) {
        mDialog.setOnDismissListener(null);

        if (mWaitingOverlay.getVisibility() == View.GONE) {
            mDialog.dismiss();
            if (callback != null) callback.run();
            return;
        }

        mWaitingProgressBar.setVisibility(View.GONE);

        if (paymentSuccess) {
            mWaitingSuccess.setVisibility(View.VISIBLE);
            mWaitingMessage.setText(mContext.getString(R.string.payments_success_message));
        } else {
            mWaitingMessage.setText(mContext.getString(R.string.payments_error_message));
        }

        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                mDialog.dismiss();
                if (callback != null) callback.run();
            }
        }, SHOW_RESULT_DELAY_MS);
    }

    /**
     * Sets the icon in the top left of the UI. This can be, for example, the favicon of the
     * merchant website. This is not a part of the constructor because favicon retrieval is
     * asynchronous.
     *
     * @param bitmap The bitmap to show next to the title.
     */
    public void setTitleBitmap(Bitmap bitmap) {
        ((ImageView) mContainer.findViewById(R.id.pageFavIcon)).setImageBitmap(bitmap);
    }

    /**
     * Updates the line items in response to a changed shipping address or option.
     *
     * @param lineItems The full bill. The last line item is the total.
     */
    public void updateLineItems(List<LineItem> lineItems) {
        mLineItems = lineItems;
        mOrderSummary.removeAllViews();

        if (mLineItems == null || mLineItems.isEmpty()) {
            mOrderSummary.setVisibility(View.GONE);
            return;
        }

        mOrderSummary.setVisibility(View.VISIBLE);

        LineItem total = lineItems.get(lineItems.size() - 1);
        mOrderSummary.addView(buildLineItem(total));

        if (mSelectedSection != mOrderSummary) return;

        for (int i = 0; i < lineItems.size() - 1; i++) {
            mOrderSummary.addView(buildLineItem(lineItems.get(i)));
        }
    }

    private ViewGroup buildLineItem(LineItem item) {
        ViewGroup line =
                (ViewGroup) LayoutInflater.from(mContext).inflate(R.layout.payment_line_item, null);
        ((TextView) line.findViewById(R.id.lineItemLabel)).setText(item.getLabel());
        ((TextView) line.findViewById(R.id.lineItemCurrencyCode)).setText(item.getCurrency());
        ((TextView) line.findViewById(R.id.lineItemPrice)).setText(item.getPrice());
        return line;
    }

    /**
     * Updates the shipping options in response to a changed shipping address.
     *
     * @param shipping The shipping options.
     */
    public void updateShippingOptions(SectionInformation section) {
        mShippingOptionsSection = section;
        updateSection(mShippingOptions, mShippingOptionsSection, mSelectShippingOptionPrompt);
    }

    /**
     * Selects or deselects the given section. Possible sections are: shipping address, shipping
     * options, and payment methods.
     *
     * A selected section lists out all options as radio buttons.
     *
     * A deselected section shows the currently selected option without radio buttons. If there's no
     * currently selected option, then selectOptionPrompt is displayed instead. This is important
     * for shipping options. If the merchant provides multiple shipping options, then there will be
     * no shipping option selected by default.
     *
     * @param sectionContainer The container for all UI elements of this section.
     * @param section The model that contains all options and specifies the currently selected
     *                option.
     *  @param selectOptionPrompt The view to show if no option is selected. If null, then nothing
     *                            is shown.
     */
    private void updateSection(
            ViewGroup sectionContainer, final SectionInformation section, View selectOptionPrompt) {
        sectionContainer.removeAllViews();
        if (section == null) {
            sectionContainer.setVisibility(View.GONE);
            return;
        }

        sectionContainer.setVisibility(View.VISIBLE);

        PaymentOption selectedItem = section.getSelectedItem();
        if (mSelectedSection != sectionContainer && selectedItem == null) {
            if (selectOptionPrompt != null) selectOptionPrompt.setVisibility(View.VISIBLE);
            return;
        }

        if (selectOptionPrompt != null) selectOptionPrompt.setVisibility(View.GONE);

        if (mSelectedSection != sectionContainer && selectedItem != null) {
            OptionLine line = new OptionLine(mContext, selectedItem);
            sectionContainer.addView(line.container);
            return;
        }

        if (section.isEmpty()) return;

        final OptionLine[] lines = new OptionLine[section.getSize()];
        for (int i = 0; i < lines.length; i++) {
            lines[i] = new OptionLine(mContext, section.getItem(i));
            lines[i].radioButton.setVisibility(mSelectedSection == sectionContainer
                    ? View.VISIBLE : View.GONE);
            lines[i].radioButton.setChecked(i == section.getSelectedItemIndex());
            lines[i].container.setClickable(true);
            lines[i].container.setTag(new OptionLineTag(section, lines, i));
            lines[i].container.setOnClickListener(this);
            sectionContainer.addView(lines[i].container);
        }
    }

    private static class OptionLine {
        public ViewGroup container;
        public RadioButton radioButton;

        public OptionLine(Context context, PaymentOption option) {
            container = (ViewGroup) LayoutInflater.from(context).inflate(
                    R.layout.payment_option_line, null);
            radioButton = (RadioButton) container.findViewById(R.id.optionRadioButton);

            ((TextView) container.findViewById(R.id.optionLabel)).setText(option.getLabel());
            ((TextView) container.findViewById(R.id.optionSubLabel)).setText(option.getSublabel());

            if (option.getDrawableIconId() != PaymentOption.NO_ICON) {
                ((ImageView) container.findViewById(R.id.optionIcon))
                        .setImageResource(option.getDrawableIconId());
            }
        }
    }

    private static class OptionLineTag {
        private final SectionInformation mSection;
        private final OptionLine[] mOptionLines;
        private final int mThisLineIndex;

        public OptionLineTag(SectionInformation section, OptionLine[] lines, int index) {
            mSection = section;
            mOptionLines = lines;
            mThisLineIndex = index;
        }

        public void selectOptionLine() {
            mSection.setSelectedItemIndex(mThisLineIndex);
            mOptionLines[mThisLineIndex].radioButton.setChecked(true);
            for (int i = 0; i < mOptionLines.length; i++) {
                if (i == mThisLineIndex) continue;
                mOptionLines[i].radioButton.setChecked(false);
            }
        }

        public SectionInformation getSection() {
            return mSection;
        }
    }

    /**
     * Called when user clicks anything in the dialog.
     */
    @Override
    public void onClick(View v) {
        if (v == mOrderSummaryLabel || v == mOrderSummary) {
            expand(mOrderSummaryLabel, mOrderSummary);
        } else if (v == mShippingSummaryLabel || v == mShippingSummaryAddress
                || v == mShippingAddressesLabel || v == mShippingAddresses) {
            expand(mShippingAddressesLabel, mShippingAddresses);
        } else if (v == mShippingSummaryOption || v == mShippingOptionsLabel
                || v == mShippingOptions || v == mSelectShippingOptionPrompt) {
            expand(mShippingOptionsLabel, mShippingOptions);
        } else if (v == mPaymentMethodsLabel || v == mPaymentMethods) {
            expand(mPaymentMethodsLabel, mPaymentMethods);
        } else if (v == mPayButton) {
            mPaymentContainer.setVisibility(View.GONE);
            mButtonBar.setVisibility(View.GONE);
            mWaitingOverlay.setVisibility(View.VISIBLE);
            Window dialogWindow = mDialog.getWindow();
            dialogWindow.setGravity(Gravity.CENTER);
            dialogWindow.setLayout(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);

            mClient.onPayClicked(
                    mShippingAddressesSection == null
                            ? null : mShippingAddressesSection.getSelectedItem(),
                    mShippingOptionsSection == null
                            ? null : mShippingOptionsSection.getSelectedItem(),
                    mPaymentMethodsSection.getSelectedItem());
        } else if (v == mEditButton) {
            if (mSelectedSection == null) {
                expand(mOrderSummaryLabel, mOrderSummary);
            } else {
                mDialog.dismiss();
            }
        } else if (v == mCloseButton) {
            mDialog.dismiss();
            return;
        } else if (v.getTag() != null && v.getTag() instanceof OptionLineTag) {
            OptionLineTag tag = (OptionLineTag) v.getTag();
            tag.selectOptionLine();

            if (tag.getSection() == mShippingAddressesSection) {
                mClient.onShippingAddressChanged(tag.getSection().getSelectedItem());
            } else if (tag.getSection() == mShippingOptionsSection) {
                mClient.onShippingOptionChanged(tag.getSection().getSelectedItem());
            } else if (tag.getSection() == mPaymentMethodsSection) {
                mClient.onPaymentMethodChanged(tag.getSection().getSelectedItem());
            }
        }

        updatePayButtonEnabled();
    }

    private void updatePayButtonEnabled() {
        if (mRequestShipping) {
            mPayButton.setEnabled(mShippingAddressesSection != null
                    && mShippingAddressesSection.getSelectedItem() != null
                    && mShippingOptionsSection != null
                    && mShippingOptionsSection.getSelectedItem() != null
                    && mPaymentMethodsSection != null
                    && mPaymentMethodsSection.getSelectedItem() != null);
        } else {
            mPayButton.setEnabled(mPaymentMethodsSection != null
                    && mPaymentMethodsSection.getSelectedItem() != null);
        }
    }

    private void expand(View sectionLabel, ViewGroup section) {
        mEditButton.setText(mContext.getString(R.string.payments_cancel_button));
        mDialog.getWindow().setLayout(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

        boolean isFirstExpansionWithShipping =
                mRequestShipping && mShippingSummaryLabel.getVisibility() != View.GONE;
        if (isFirstExpansionWithShipping) {
            mShippingSummaryLabel.setVisibility(View.GONE);
            mShippingSummary.setVisibility(View.GONE);
            mShippingSummarySeparator.setVisibility(View.GONE);

            mShippingAddressesLabel.setVisibility(View.VISIBLE);
            mShippingAddresses.setVisibility(View.VISIBLE);
            mShippingAddressesSeparator.setVisibility(View.VISIBLE);

            mShippingOptionsLabel.setVisibility(View.VISIBLE);
            mSelectShippingOptionPrompt.setVisibility(View.VISIBLE);
            mShippingOptions.setVisibility(View.VISIBLE);
            mShippingOptionsSeparator.setVisibility(View.VISIBLE);
        }

        if (mSelectedSectionLabel != null) {
            mSelectedSectionLabel.setBackgroundColor(Color.WHITE);
            mSelectedSectionLabel.setClickable(true);
        }

        if (mSelectedSection != null) {
            mSelectedSection.setBackgroundColor(Color.WHITE);
            mSelectedSection.setClickable(true);
        }

        sectionLabel.setBackgroundColor(R.color.default_primary_color);
        sectionLabel.setClickable(false);

        section.setBackgroundColor(R.color.default_primary_color);
        section.setClickable(false);

        ViewGroup noLongerSelectedSection = mSelectedSection;
        mSelectedSectionLabel = sectionLabel;
        mSelectedSection = section;

        if (noLongerSelectedSection == mOrderSummary) {
            updateLineItems(mLineItems);
        }
        if (noLongerSelectedSection == mShippingAddresses
                || (isFirstExpansionWithShipping && mSelectedSection != mShippingAddresses)) {
            updateSection(mShippingAddresses, mShippingAddressesSection, null);
        }
        if (noLongerSelectedSection == mShippingOptions
                || (isFirstExpansionWithShipping && mSelectedSection != mShippingOptions)) {
            updateSection(mShippingOptions, mShippingOptionsSection, mSelectShippingOptionPrompt);
        }
        if (noLongerSelectedSection == mPaymentMethods) {
            updateSection(mPaymentMethods, mPaymentMethodsSection, null);
        }

        if (mSelectedSection == mOrderSummary) {
            mClient.getLineItems(new Callback<List<LineItem>>() {
                @Override
                public void onResult(List<LineItem> result) {
                    updateLineItems(result);
                }
            });
        } else if (mSelectedSection == mShippingAddresses) {
            mClient.getShippingAddresses(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    mShippingAddressesSection = result;
                    updateSection(mShippingAddresses, mShippingAddressesSection, null);
                }
            });
        } else if (mSelectedSection == mShippingOptions) {
            mClient.getShippingOptions(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    mShippingOptionsSection = result;
                    updateSection(
                            mShippingOptions, mShippingOptionsSection, mSelectShippingOptionPrompt);
                }
            });
        } else if (mSelectedSection == mPaymentMethods) {
            mClient.getPaymentMethods(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    mPaymentMethodsSection = result;
                    updateSection(mPaymentMethods, mPaymentMethodsSection, null);
                }
            });
        }
    }

    /**
     * Called when the user dismisses the dialog by clicking the "back" button on the phone or the
     * "X" button in the top-right corner of the dialog.
     */
    @Override
    public void onDismiss(DialogInterface dialog) {
        mClient.onDismiss();
    }
}
