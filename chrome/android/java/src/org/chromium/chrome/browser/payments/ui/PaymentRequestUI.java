// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.os.Handler;
import android.text.TextUtils.TruncateAt;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.ExtraTextSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.LineItemBreakdownSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.SectionSeparator;
import org.chromium.chrome.browser.widget.AlwaysDismissedDialog;
import org.chromium.chrome.browser.widget.DualControlLayout;

import java.util.ArrayList;
import java.util.List;

/**
 * The PaymentRequest UI.
 */
public class PaymentRequestUI implements DialogInterface.OnDismissListener, View.OnClickListener,
        PaymentRequestSection.PaymentsSectionListener {
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
    private final ViewGroup mPaymentContainerLayout;
    private final DualControlLayout mButtonBar;
    private final View mWaitingOverlay;
    private final View mWaitingProgressBar;
    private final View mWaitingSuccess;
    private final TextView mWaitingMessage;
    private final Button mEditButton;
    private final Button mPayButton;
    private final View mCloseButton;

    private final LineItemBreakdownSection mOrderSummarySection;
    private final ExtraTextSection mShippingSummarySection;
    private final OptionSection mShippingAddressSection;
    private final OptionSection mShippingOptionSection;
    private final OptionSection mPaymentMethodSection;
    private final List<SectionSeparator> mSectionSeparators;

    private ViewGroup mSelectedSection;
    private boolean mIsShowingEditDialog;

    private List<LineItem> mLineItems;
    private SectionInformation mPaymentMethodSectionInformation;
    private SectionInformation mShippingAddressSectionInformation;
    private SectionInformation mShippingOptionsSectionInformation;

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
        mWaitingOverlay = mContainer.findViewById(R.id.waitingOverlay);
        mWaitingProgressBar = mContainer.findViewById(R.id.waitingProgressBar);
        mWaitingSuccess = mContainer.findViewById(R.id.waitingSuccess);
        mWaitingMessage = (TextView) mContainer.findViewById(R.id.waitingMessage);

        mCloseButton = mContainer.findViewById(R.id.close_button);
        mCloseButton.setOnClickListener(this);

        // Set up the buttons.
        mPayButton = DualControlLayout.createButtonForLayout(
                activity, true, activity.getString(R.string.payments_pay_button), this);
        mEditButton = DualControlLayout.createButtonForLayout(
                activity, false, activity.getString(R.string.payments_edit_button), this);
        mButtonBar = (DualControlLayout) mContainer.findViewById(R.id.buttonBar);
        mButtonBar.setAlignment(DualControlLayout.ALIGN_END);
        mButtonBar.setStackedMargin(activity.getResources().getDimensionPixelSize(
                R.dimen.infobar_margin_between_stacked_buttons));
        mButtonBar.addView(mPayButton);
        mButtonBar.addView(mEditButton);

        // Create all the possible sections.
        mSectionSeparators = new ArrayList<SectionSeparator>();
        mPaymentContainerLayout = (ViewGroup) mContainer.findViewById(R.id.paymentContainerLayout);
        mOrderSummarySection = new LineItemBreakdownSection(activity,
                activity.getString(R.string.payments_order_summary_label), this);
        mShippingSummarySection = new ExtraTextSection(activity,
                activity.getString(R.string.payments_shipping_summary_label), this);
        mShippingAddressSection = new OptionSection(activity,
                activity.getString(R.string.payments_shipping_address_label),
                activity.getString(R.string.payments_select_shipping_address_prompt), this);
        mShippingOptionSection = new OptionSection(activity,
                activity.getString(R.string.payments_shipping_option_label),
                activity.getString(R.string.payments_select_shipping_option_prompt), this);
        mPaymentMethodSection = new OptionSection(activity,
                activity.getString(R.string.payments_method_of_payment_label), null, this);

        // Add the necessary sections to the layout.
        mPaymentContainerLayout.addView(mOrderSummarySection, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout));
        if (mRequestShipping) {
            // The shipping breakout sections are only added if they are needed.
            mPaymentContainerLayout.addView(mShippingSummarySection, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout));
        }
        mPaymentContainerLayout.addView(mPaymentMethodSection, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        // Set up the dialog.
        mDialog = new AlwaysDismissedDialog(activity, R.style.DialogWhenLarge);
        mDialog.setOnDismissListener(this);
        mDialog.addContentView(
                mContainer, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT));
        Window dialogWindow = mDialog.getWindow();
        dialogWindow.setGravity(Gravity.BOTTOM);
        dialogWindow.setLayout(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        dialogWindow.setDimAmount(0.5f);
        dialogWindow.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        mDialog.show();

        mClient.getDefaultPaymentInformation(new Callback<PaymentInformation>() {
            @Override
            public void onResult(PaymentInformation result) {
                updateOrderSummarySection(result.getLineItems());

                if (mRequestShipping) {
                    updateShippingAddressSection(result.getShippingAddresses());
                    updateShippingOptionsSection(result.getShippingOptions());

                    String selectedShippingAddress = result.getSelectedShippingAddressLabel();
                    String selectedShippingName = result.getSelectedShippingAddressSublabel();
                    String selectedShippingOptionLabel = result.getSelectedShippingOptionLabel();
                    if (selectedShippingAddress == null && selectedShippingOptionLabel == null) {
                        mShippingSummarySection.setSummaryText(mContext.getString(
                                R.string.payments_select_shipping_prompt), null);
                        mShippingSummarySection.setSummaryProperties(null, false, null, false);
                    } else {
                        // Show the shipping address and the name in the summary section.
                        mShippingSummarySection.setSummaryText(selectedShippingAddress == null
                                ? mContext.getString(
                                        R.string.payments_select_shipping_address_prompt)
                                : selectedShippingAddress, selectedShippingName);
                        mShippingSummarySection.setSummaryProperties(
                                TruncateAt.MIDDLE, true, null, true);

                        // Indicate the shipping option below the address.
                        mShippingSummarySection.setExtraText(selectedShippingOptionLabel == null
                                ? mContext.getString(
                                        R.string.payments_select_shipping_option_prompt)
                                : selectedShippingOptionLabel);
                    }
                }

                updatePaymentMethodSection(result.getPaymentMethods());
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
    public void updateOrderSummarySection(List<LineItem> lineItems) {
        mLineItems = lineItems;

        if (mLineItems == null || mLineItems.isEmpty()) {
            mOrderSummarySection.setVisibility(View.GONE);
        } else {
            mOrderSummarySection.setVisibility(View.VISIBLE);
            mOrderSummarySection.update(lineItems);
        }
    }

    public void updateShippingAddressSection(SectionInformation section) {
        mShippingAddressSectionInformation = section;
        mShippingAddressSection.update(section);
    }

    /**
     * Updates the shipping options in response to a changed shipping address.
     *
     * @param shipping The shipping options.
     */
    public void updateShippingOptionsSection(SectionInformation section) {
        mShippingOptionsSectionInformation = section;
        mShippingOptionSection.update(section);
    }

    private void updatePaymentMethodSection(SectionInformation section) {
        mPaymentMethodSectionInformation = section;
        mPaymentMethodSection.update(section);
    }

    @Override
    public void onPaymentOptionChanged(OptionSection section, PaymentOption option) {
        if (section == mShippingAddressSection) {
            mShippingAddressSectionInformation.setSelectedItem(option);
            mClient.onShippingAddressChanged(option);
        } else if (section == mShippingOptionSection) {
            mShippingOptionsSectionInformation.setSelectedItem(option);
            mClient.onShippingOptionChanged(option);
        } else if (section == mPaymentMethodSection) {
            mPaymentMethodSectionInformation.setSelectedItem(option);
            mClient.onPaymentMethodChanged(option);
        }

        updatePayButtonEnabled();
    }

    /**
     * Called when user clicks anything in the dialog.
     */
    @Override
    public void onClick(View v) {
        if (v == mOrderSummarySection) {
            expand(mOrderSummarySection);
        } else if (v == mShippingSummarySection || v == mShippingAddressSection) {
            expand(mShippingAddressSection);
        } else if (v == mShippingOptionSection) {
            expand(mShippingOptionSection);
        } else if (v == mPaymentMethodSection) {
            expand(mPaymentMethodSection);
        } else if (v == mPayButton) {
            mPaymentContainer.setVisibility(View.GONE);
            mButtonBar.setVisibility(View.GONE);
            mWaitingOverlay.setVisibility(View.VISIBLE);
            Window dialogWindow = mDialog.getWindow();
            dialogWindow.setGravity(Gravity.CENTER);
            dialogWindow.setLayout(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);

            mClient.onPayClicked(
                    mShippingAddressSectionInformation == null
                            ? null : mShippingAddressSectionInformation.getSelectedItem(),
                    mShippingOptionsSectionInformation == null
                            ? null : mShippingOptionsSectionInformation.getSelectedItem(),
                    mPaymentMethodSectionInformation.getSelectedItem());
        } else if (v == mEditButton) {
            if (mSelectedSection == null) {
                expand(mOrderSummarySection);
            } else {
                mDialog.dismiss();
            }
        } else if (v == mCloseButton) {
            mDialog.dismiss();
            return;
        }

        updatePayButtonEnabled();
    }

    private void updatePayButtonEnabled() {
        if (mRequestShipping) {
            mPayButton.setEnabled(mShippingAddressSectionInformation != null
                    && mShippingAddressSectionInformation.getSelectedItem() != null
                    && mShippingOptionsSectionInformation != null
                    && mShippingOptionsSectionInformation.getSelectedItem() != null
                    && mPaymentMethodSectionInformation != null
                    && mPaymentMethodSectionInformation.getSelectedItem() != null);
        } else {
            mPayButton.setEnabled(mPaymentMethodSectionInformation != null
                    && mPaymentMethodSectionInformation.getSelectedItem() != null);
        }
    }

    private void expand(ViewGroup section) {
        if (!mIsShowingEditDialog) {
            // Swap out Views that combine multiple fields with individual fields.
            if (mRequestShipping && mShippingSummarySection.getParent() != null) {
                int summaryIndex = mPaymentContainerLayout.indexOfChild(mShippingSummarySection);
                mPaymentContainerLayout.removeView(mShippingSummarySection);

                mPaymentContainerLayout.addView(mShippingAddressSection, summaryIndex,
                        new LinearLayout.LayoutParams(
                                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
                mSectionSeparators.add(
                        new SectionSeparator(mPaymentContainerLayout, summaryIndex + 1));
                mPaymentContainerLayout.addView(mShippingOptionSection, summaryIndex + 2,
                        new LinearLayout.LayoutParams(
                                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            }

            // Expand all the dividers.
            for (int i = 0; i < mSectionSeparators.size(); i++) mSectionSeparators.get(i).expand();
            mPaymentContainerLayout.requestLayout();

            // Switch the 'edit' button to a 'cancel' button.
            mEditButton.setText(mContext.getString(R.string.payments_cancel_button));

            // Make the dialog take the whole screen.
            mDialog.getWindow().setLayout(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            mIsShowingEditDialog = true;
        }

        // Update the display status of each expandable section.
        mOrderSummarySection.setDisplayMode(section == mOrderSummarySection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mShippingAddressSection.setDisplayMode(section == mShippingAddressSection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mShippingOptionSection.setDisplayMode(section == mShippingOptionSection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mPaymentMethodSection.setDisplayMode(section == mPaymentMethodSection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);

        // Update the section contents when they're selected.
        mSelectedSection = section;
        assert mSelectedSection != mShippingSummarySection;
        if (mSelectedSection == mOrderSummarySection) {
            mClient.getLineItems(new Callback<List<LineItem>>() {
                @Override
                public void onResult(List<LineItem> result) {
                    updateOrderSummarySection(result);
                }
            });
        } else if (mSelectedSection == mShippingAddressSection) {
            mClient.getShippingAddresses(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    updateShippingAddressSection(result);
                }
            });
        } else if (mSelectedSection == mShippingOptionSection) {
            mClient.getShippingOptions(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    updateShippingOptionsSection(result);
                }
            });
        } else if (mSelectedSection == mPaymentMethodSection) {
            mClient.getPaymentMethods(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    updatePaymentMethodSection(result);
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
