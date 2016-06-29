// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.annotation.IntDef;
import android.support.v4.view.animation.FastOutLinearInInterpolator;
import android.support.v4.view.animation.LinearOutSlowInInterpolator;
import android.text.TextUtils.TruncateAt;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.ExtraTextSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.LineItemBreakdownSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.SectionSeparator;
import org.chromium.chrome.browser.widget.AlwaysDismissedDialog;
import org.chromium.chrome.browser.widget.DualControlLayout;
import org.chromium.chrome.browser.widget.animation.AnimatorProperties;
import org.chromium.chrome.browser.widget.animation.FocusAnimator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * The PaymentRequest UI.
 */
public class PaymentRequestUI implements DialogInterface.OnDismissListener, View.OnClickListener,
        PaymentRequestSection.SectionDelegate {
    public static final int TYPE_SHIPPING_ADDRESSES = 1;
    public static final int TYPE_SHIPPING_OPTIONS = 2;
    public static final int TYPE_CONTACT_DETAILS = 3;
    public static final int TYPE_PAYMENT_METHODS = 4;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        TYPE_SHIPPING_ADDRESSES,
        TYPE_SHIPPING_OPTIONS,
        TYPE_CONTACT_DETAILS,
        TYPE_PAYMENT_METHODS
    })
    public @interface DataType {}

    /**
     * The interface to be implemented by the consumer of the PaymentRequest UI.
     */
    public interface Client {
        /**
         * Asynchronously returns the default payment information.
         */
        void getDefaultPaymentInformation(Callback<PaymentInformation> callback);

        /**
         * Asynchronously returns the full bill. Includes the total price and its breakdown into
         * individual line items.
         */
        void getShoppingCart(Callback<ShoppingCart> callback);

        /**
         * Asynchronously returns the full list of options for the given type.
         *
         * @param optionType Data being updated.
         * @param callback   Callback to run when the data has been fetched.
         */
        void getSectionInformation(
                @DataType int optionType, Callback<SectionInformation> callback);

        /**
         * Called when the user changes one of their payment options.
         *
         * If this method returns true, then:
         * + The added option should be asynchronously verified.
         * + The section should be disabled and a progress spinny should be shown while the option
         *   is being verified.
         * + The checkedCallback will be invoked with the results of the check and updated
         *   information.
         *
         * For example, if the website needs a shipping address to calculate shipping options, then
         * calling onSectionOptionSelected(TYPE_SHIPPING_ADDRESS, option, checkedCallback) will
         * return true. When the website updates the shipping options, the checkedCallback will be
         * invoked.
         *
         * @param optionType        Data being updated.
         * @param option            Value of the data being updated.
         * @param checkedCallback   The callback after an asynchronous check has completed.
         * @return True if the option needs to be asynchronously checked.
         */
        boolean onSectionOptionSelected(@DataType int optionType, PaymentOption option,
                Callback<PaymentInformation> checkedCallback);

        /**
         * Called when the user clicks on the "Add" button for a section.
         *
         * If this method returns true, then:
         * + The added option should be asynchronously verified.
         * + The section should be disabled and a progress spinny should be shown while the option
         *   is being verified.
         * + The checkedCallback will be invoked with the results of the check and updated
         *   information.
         *
         * @param optionType      Data being updated.
         * @param checkedCallback The callback after an asynchronous check has completed.
         * @return True if the option needs to be asynchronously checked.
         */
        boolean onSectionAddOption(
                @DataType int optionType, Callback<PaymentInformation> checkedCallback);

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

        /**
         * Checks if the merchant needs a shipping address to provide the shipping options.
         * @return Whether or not the merchant needs a shipping address.
         */
        boolean merchantNeedsShippingAddress();
    }

    /**
     * A test-only observer for PaymentRequest UI.
     */
    public interface PaymentRequestObserverForTest {
        /**
         * Called when clicks on the UI are possible.
         */
        void onPaymentRequestReadyForInput(PaymentRequestUI ui);

        /**
         * Called when clicks on the X close button are possible.
         */
        void onPaymentRequestReadyToClose(PaymentRequestUI ui);

        /**
         * Called when clicks on the PAY button are possible.
         */
        void onPaymentRequestReadyToPay(PaymentRequestUI ui);

        /**
         * Called when edit dialog is showing.
         */
        void onPaymentRequestReadyToEdit();

        /**
         * Called when editor validation completes with error. This can happen, for example, when
         * user enters an invalid email address.
         */
        void onPaymentRequestEditorValidationError();

        /**
         * Called when an editor field text has changed.
         */
        void onPaymentRequestEditorTextUpdate();

        /**
         * Called when editor is dismissed.
         */
        void onPaymentRequestEditorDismissed();

        /**
         * Called when the result UI is showing.
         */
        void onPaymentRequestResultReady(PaymentRequestUI ui);

        /**
         * Called when the UI is gone.
         */
        void onPaymentRequestDismiss();
    }

    /** Length of the animation to either show the UI or expand it to full height. */
    private static final int DIALOG_ENTER_ANIMATION_MS = 225;

    /** Length of the animation to hide the bottom sheet UI. */
    private static final int DIALOG_EXIT_ANIMATION_MS = 195;

    private static PaymentRequestObserverForTest sObserverForTest;

    private final Context mContext;
    private final Client mClient;
    private final boolean mRequestShipping;
    private final boolean mRequestContactDetails;

    private final Dialog mDialog;
    private final EditorView mEditorView;
    private final ViewGroup mFullContainer;
    private final ViewGroup mRequestView;
    private final PaymentRequestUiErrorView mErrorView;
    private final Callback<PaymentInformation> mUpdateSectionsCallback;

    private ScrollView mPaymentContainer;
    private LinearLayout mPaymentContainerLayout;
    private DualControlLayout mButtonBar;
    private Button mEditButton;
    private Button mPayButton;
    private View mCloseButton;
    private View mSpinnyLayout;

    private LineItemBreakdownSection mOrderSummarySection;
    private ExtraTextSection mShippingSummarySection;
    private OptionSection mShippingAddressSection;
    private OptionSection mShippingOptionSection;
    private OptionSection mContactDetailsSection;
    private OptionSection mPaymentMethodSection;
    private List<SectionSeparator> mSectionSeparators;

    private ViewGroup mSelectedSection;
    private boolean mIsShowingEditDialog;
    private boolean mIsProcessingPayClicked;
    private boolean mIsClientClosing;
    private boolean mIsClientCheckingSelection;

    private ShoppingCart mShoppingCart;
    private SectionInformation mPaymentMethodSectionInformation;
    private SectionInformation mShippingAddressSectionInformation;
    private SectionInformation mShippingOptionsSectionInformation;
    private SectionInformation mContactDetailsSectionInformation;

    private Animator mSheetAnimator;
    private FocusAnimator mSectionAnimator;
    private int mAnimatorTranslation;
    private boolean mIsInitialLayoutComplete;

    /**
     * Builds and shows the UI for PaymentRequest.
     *
     * @param activity        The activity on top of which the UI should be displayed.
     * @param client          The consumer of the PaymentRequest UI.
     * @param requestShipping Whether the UI should show the shipping address and option selection.
     * @param requestContact  Whether the UI should show the email address and phone number
     *                        selection.
     * @param title           The title to show at the top of the UI. This can be, for example, the
     *                        &lt;title&gt; of the merchant website. If the string is too long for
     *                        UI, it elides at the end.
     * @param origin          The origin (part of URL) to show under the title. For example,
     *                        "https://shop.momandpop.com". If the origin is too long for the UI, it
     *                        should elide according to:
     * https://www.chromium.org/Home/chromium-security/enamel#TOC-Eliding-Origin-Names-And-Hostnames
     */
    public PaymentRequestUI(Activity activity, Client client, boolean requestShipping,
            boolean requestContactDetails, String title, String origin) {
        mContext = activity;
        mClient = client;
        mRequestShipping = requestShipping;
        mRequestContactDetails = requestContactDetails;
        mAnimatorTranslation = activity.getResources().getDimensionPixelSize(
                R.dimen.payments_ui_translation);

        mErrorView = (PaymentRequestUiErrorView) LayoutInflater.from(mContext).inflate(
                R.layout.payment_request_error, null);
        mErrorView.initialize(title, origin);

        // This callback will be fired if mIsClientCheckingSelection is true.
        mUpdateSectionsCallback = new Callback<PaymentInformation>() {
            @Override
            public void onResult(PaymentInformation result) {
                mIsClientCheckingSelection = false;
                updateOrderSummarySection(result.getShoppingCart());
                if (mRequestShipping) {
                    updateSection(TYPE_SHIPPING_ADDRESSES, result.getShippingAddresses());
                    updateSection(TYPE_SHIPPING_OPTIONS, result.getShippingOptions());
                }
                if (mRequestContactDetails) {
                    updateSection(TYPE_CONTACT_DETAILS, result.getContactDetails());
                }
                updateSection(TYPE_PAYMENT_METHODS, result.getPaymentMethods());
                if (mShippingAddressSectionInformation.getSelectedItem() == null) {
                    mShippingAddressSection.setDisplayMode(
                            PaymentRequestSection.DISPLAY_MODE_FOCUSED);
                } else {
                    expand(null);
                }
                updatePayButtonEnabled();
            }
        };

        mRequestView =
                (ViewGroup) LayoutInflater.from(mContext).inflate(R.layout.payment_request, null);
        prepareRequestView(activity, title, origin);

        // To handle the specced animations, the dialog is entirely contained within a translucent
        // FrameLayout.  This could eventually be converted to a real BottomSheetDialog, but that
        // requires exploration of how interactions would work when the dialog can be sent back and
        // forth between the peeking and expanded state.
        mFullContainer = new FrameLayout(mContext);
        mFullContainer.setBackgroundColor(
                ApiCompatibilityUtils.getColor(mContext.getResources(), R.color.payments_ui_scrim));
        FrameLayout.LayoutParams bottomSheetParams = new FrameLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        bottomSheetParams.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
        mFullContainer.addView(mRequestView, bottomSheetParams);

        mEditorView = new EditorView(activity, sObserverForTest);

        // Set up the dialog.
        mDialog = new AlwaysDismissedDialog(activity, R.style.DialogWhenLarge);
        mDialog.setOnDismissListener(this);
        mDialog.addContentView(mFullContainer,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        Window dialogWindow = mDialog.getWindow();
        dialogWindow.setGravity(Gravity.CENTER);
        dialogWindow.setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        dialogWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        mDialog.show();

        mClient.getDefaultPaymentInformation(new Callback<PaymentInformation>() {
            @Override
            public void onResult(PaymentInformation result) {
                updateOrderSummarySection(result.getShoppingCart());

                if (mRequestShipping) {
                    updateSection(TYPE_SHIPPING_ADDRESSES, result.getShippingAddresses());
                    updateSection(TYPE_SHIPPING_OPTIONS, result.getShippingOptions());

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

                if (mRequestContactDetails) {
                    updateSection(TYPE_CONTACT_DETAILS, result.getContactDetails());
                }

                updateSection(TYPE_PAYMENT_METHODS, result.getPaymentMethods());
                updatePayButtonEnabled();

                // Hide the loading indicators and show the real sections.
                mPaymentContainer.setVisibility(View.VISIBLE);
                mButtonBar.setVisibility(View.VISIBLE);
                mRequestView.removeView(mSpinnyLayout);
                mRequestView.addOnLayoutChangeListener(new SheetEnlargingAnimator(false));
            }
        });
    }

    /**
     * Prepares the PaymentRequestUI for initial display.
     *
     * TODO(dfalcantara): Ideally, everything related to the request and its views would just be put
     *                    into its own class but that'll require yanking out a lot of this class.
     *
     * @param activity Activity displaying the UI.
     * @param title    Title of the page.
     * @param origin   Host of the page.
     */
    private void prepareRequestView(Activity activity, String title, String origin) {
        mSpinnyLayout = mRequestView.findViewById(R.id.payment_request_spinny);

        // Indicate that we're preparing the dialog for display.
        TextView messageView = (TextView) mRequestView.findViewById(R.id.message);
        messageView.setText(R.string.payments_loading_message);

        ((TextView) mRequestView.findViewById(R.id.page_title)).setText(title);
        ((TextView) mRequestView.findViewById(R.id.hostname)).setText(origin);

        // Set up the buttons.
        mCloseButton = mRequestView.findViewById(R.id.close_button);
        mCloseButton.setOnClickListener(this);
        mPayButton = DualControlLayout.createButtonForLayout(
                activity, true, activity.getString(R.string.payments_pay_button), this);
        mEditButton = DualControlLayout.createButtonForLayout(
                activity, false, activity.getString(R.string.payments_edit_button), this);
        mButtonBar = (DualControlLayout) mRequestView.findViewById(R.id.button_bar);
        mButtonBar.setAlignment(DualControlLayout.ALIGN_END);
        mButtonBar.setStackedMargin(activity.getResources().getDimensionPixelSize(
                R.dimen.infobar_margin_between_stacked_buttons));
        mButtonBar.addView(mPayButton);
        mButtonBar.addView(mEditButton);

        // Create all the possible sections.
        mSectionSeparators = new ArrayList<SectionSeparator>();
        mPaymentContainer = (ScrollView) mRequestView.findViewById(R.id.option_container);
        mPaymentContainerLayout =
                (LinearLayout) mRequestView.findViewById(R.id.payment_container_layout);
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
        mContactDetailsSection = new OptionSection(activity,
                activity.getString(R.string.payments_contact_details_label),
                activity.getString(R.string.payments_select_contact_details_prompt), this);
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
        if (mRequestContactDetails) {
            // Contact details are optional, depending on the merchant website.
            mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout));
            mPaymentContainerLayout.addView(mContactDetailsSection, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }
        mRequestView.addOnLayoutChangeListener(new FadeInAnimator());
        mRequestView.addOnLayoutChangeListener(new PeekingAnimator());

        // Enabled in updatePayButtonEnabled() when the user has selected all payment options.
        mPayButton.setEnabled(false);
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
     * @param shouldCloseImmediately If true, this function will immediately dismiss the dialog
     *        without describing the error.
     * @param callback The callback to notify of finished animations.
     */
    public void close(boolean shouldCloseImmediately, final Runnable callback) {
        mIsClientClosing = true;

        Runnable dismissRunnable = new Runnable() {
            @Override
            public void run() {
                dismissDialog(false);
                if (callback != null) callback.run();
            }
        };

        if (shouldCloseImmediately || !mIsProcessingPayClicked) {
            // The shouldCloseImmediately boolean is true when the merchant calls
            // instrumentResponse.complete("success") or instrumentResponse.complete("")
            // in JavaScript.
            dismissRunnable.run();
        } else {
            // Animate the bottom sheet going away.
            new DisappearingAnimator(false);

            // Show the error dialog.
            mErrorView.show(mFullContainer, dismissRunnable);
        }

        if (sObserverForTest != null) sObserverForTest.onPaymentRequestResultReady(this);
    }

    /**
     * Sets the icon in the top left of the UI. This can be, for example, the favicon of the
     * merchant website. This is not a part of the constructor because favicon retrieval is
     * asynchronous.
     *
     * @param bitmap The bitmap to show next to the title.
     */
    public void setTitleBitmap(Bitmap bitmap) {
        ((ImageView) mRequestView.findViewById(R.id.icon_view)).setImageBitmap(bitmap);
        mErrorView.setBitmap(bitmap);
    }

    /**
     * Updates the line items in response to a changed shipping address or option.
     *
     * @param cart The shopping cart, including the line items and the total.
     */
    public void updateOrderSummarySection(ShoppingCart cart) {
        mShoppingCart = cart;

        if (cart == null || cart.getTotal() == null) {
            mOrderSummarySection.setVisibility(View.GONE);
        } else {
            mOrderSummarySection.setVisibility(View.VISIBLE);
            mOrderSummarySection.update(cart);
        }
    }

    /**
     * Updates the UI to account for changes in payment information.
     *
     * @param section The shipping options.
     */
    public void updateSection(@DataType int whichSection, SectionInformation section) {
        if (whichSection == TYPE_SHIPPING_ADDRESSES) {
            mShippingAddressSectionInformation = section;
            mShippingAddressSection.update(section);
        } else if (whichSection == TYPE_SHIPPING_OPTIONS) {
            mShippingOptionsSectionInformation = section;
            mShippingOptionSection.update(section);
        } else if (whichSection == TYPE_CONTACT_DETAILS) {
            mContactDetailsSectionInformation = section;
            mContactDetailsSection.update(section);
        } else if (whichSection == TYPE_PAYMENT_METHODS) {
            mPaymentMethodSectionInformation = section;
            mPaymentMethodSection.update(section);
        }
        updatePayButtonEnabled();
    }

    @Override
    public void onPaymentOptionChanged(final OptionSection section, PaymentOption option) {
        if (section == mShippingAddressSection
                && mShippingAddressSectionInformation.getSelectedItem() != option) {
            mShippingAddressSectionInformation.setSelectedItem(option);
            mIsClientCheckingSelection = mClient.onSectionOptionSelected(
                    TYPE_SHIPPING_ADDRESSES, option, mUpdateSectionsCallback);
        } else if (section == mShippingOptionSection) {
            mShippingOptionsSectionInformation.setSelectedItem(option);
            mClient.onSectionOptionSelected(TYPE_SHIPPING_OPTIONS, option, null);
        } else if (section == mContactDetailsSection) {
            mContactDetailsSectionInformation.setSelectedItem(option);
            mClient.onSectionOptionSelected(TYPE_CONTACT_DETAILS, option, null);
        } else if (section == mPaymentMethodSection) {
            mPaymentMethodSectionInformation.setSelectedItem(option);
            mClient.onSectionOptionSelected(TYPE_PAYMENT_METHODS, option, null);
        }

        if (mIsClientCheckingSelection) {
            startSectionResizeAnimation();
            section.setDisplayMode(PaymentRequestSection.DISPLAY_MODE_CHECKING);
        } else {
            expand(null);
        }
        updatePayButtonEnabled();
    }

    @Override
    public void onAddPaymentOption(OptionSection section) {
        assert section != mShippingOptionSection;

        if (section == mShippingAddressSection) {
            mIsClientCheckingSelection =
                    mClient.onSectionAddOption(TYPE_SHIPPING_ADDRESSES, mUpdateSectionsCallback);
        } else if (section == mContactDetailsSection) {
            mClient.onSectionAddOption(TYPE_CONTACT_DETAILS, null);
        } else if (section == mPaymentMethodSection) {
            mClient.onSectionAddOption(TYPE_PAYMENT_METHODS, null);
        }

        if (mIsClientCheckingSelection) {
            startSectionResizeAnimation();
            section.setDisplayMode(PaymentRequestSection.DISPLAY_MODE_CHECKING);
        }

        updatePayButtonEnabled();
    }

    /** @return The editor user interface. */
    public EditorView getEditorView() {
        return mEditorView;
    }

    /**
     * Called when user clicks anything in the dialog.
     */
    @Override
    public void onClick(View v) {
        if (!isAcceptingCloseButton()) return;

        if (v == mCloseButton) {
            dismissDialog(true);
            return;
        }

        if (!isAcceptingUserInput()) return;

        if (v == mOrderSummarySection) {
            expand(mOrderSummarySection);
        } else if (v == mShippingSummarySection || v == mShippingAddressSection) {
            expand(mShippingAddressSection);
        } else if (v == mShippingOptionSection) {
            expand(mShippingOptionSection);
        } else if (v == mContactDetailsSection) {
            expand(mContactDetailsSection);
        } else if (v == mPaymentMethodSection) {
            expand(mPaymentMethodSection);
        } else if (v == mPayButton) {
            processPayButton();
        } else if (v == mEditButton) {
            if (mIsShowingEditDialog) {
                dismissDialog(true);
            } else {
                expand(mOrderSummarySection);
            }
        }

        updatePayButtonEnabled();
    }

    /**
     * Dismiss the dialog.
     *
     * @param isAnimated If true, the dialog dismissal is animated.
     */
    private void dismissDialog(boolean isAnimated) {
        if (mDialog.isShowing()) {
            if (isAnimated) {
                new DisappearingAnimator(true);
            } else {
                mDialog.dismiss();
            }
        }
    }

    private void processPayButton() {
        mIsProcessingPayClicked = true;

        mRequestView.removeView(mPaymentContainer);
        mRequestView.removeView(mButtonBar);
        mRequestView.addView(mSpinnyLayout);

        // Turn the bottom sheet back into a collapsed bottom sheet showing only the spinner.
        // TODO(dfalcantara): Animate this: https://crbug.com/621955
        FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) mRequestView.getLayoutParams();
        params.width = LayoutParams.MATCH_PARENT;
        params.height = LayoutParams.WRAP_CONTENT;
        params.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
        mRequestView.requestLayout();

        mClient.onPayClicked(
                mShippingAddressSectionInformation == null
                        ? null : mShippingAddressSectionInformation.getSelectedItem(),
                mShippingOptionsSectionInformation == null
                        ? null : mShippingOptionsSectionInformation.getSelectedItem(),
                mPaymentMethodSectionInformation.getSelectedItem());
    }

    private void updatePayButtonEnabled() {
        boolean contactInfoOk = !mRequestContactDetails
                || (mContactDetailsSectionInformation != null
                           && mContactDetailsSectionInformation.getSelectedItem() != null);
        boolean shippingInfoOk = !mRequestShipping
                || (mShippingAddressSectionInformation != null
                           && mShippingAddressSectionInformation.getSelectedItem() != null
                           && mShippingOptionsSectionInformation != null
                           && mShippingOptionsSectionInformation.getSelectedItem() != null);
        mPayButton.setEnabled(contactInfoOk && shippingInfoOk
                && mPaymentMethodSectionInformation != null
                && mPaymentMethodSectionInformation.getSelectedItem() != null
                && !mIsClientCheckingSelection);
        notifyReadyToPay();
    }

    /** @return Whether or not the dialog can be closed via the X close button. */
    private boolean isAcceptingCloseButton() {
        return mSheetAnimator == null && mSectionAnimator == null && mIsInitialLayoutComplete;
    }

    /** @return Whether or not the dialog is accepting user input. */
    @Override
    public boolean isAcceptingUserInput() {
        return isAcceptingCloseButton() && mPaymentMethodSectionInformation != null
                && !mIsClientCheckingSelection;
    }

    private void expand(ViewGroup section) {
        if (!mIsShowingEditDialog) {
            // Container now takes the full height of the screen, animating towards it.
            mRequestView.getLayoutParams().height = LayoutParams.MATCH_PARENT;
            mRequestView.addOnLayoutChangeListener(new SheetEnlargingAnimator(true));

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

            // New separators appear at the top and bottom of the list.
            mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout, 0));
            mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout, -1));

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

        // Update the section contents when they're selected.
        mSelectedSection = section;
        assert mSelectedSection != mShippingSummarySection;
        if (mSelectedSection == mOrderSummarySection) {
            mClient.getShoppingCart(new Callback<ShoppingCart>() {
                @Override
                public void onResult(ShoppingCart result) {
                    updateOrderSummarySection(result);
                    updateSectionVisibility();
                }
            });
        } else if (mSelectedSection == mShippingAddressSection) {
            mClient.getSectionInformation(
                    TYPE_SHIPPING_ADDRESSES, createUpdateSectionCallback(TYPE_SHIPPING_ADDRESSES));
        } else if (mSelectedSection == mShippingOptionSection) {
            mClient.getSectionInformation(
                    TYPE_SHIPPING_OPTIONS, createUpdateSectionCallback(TYPE_SHIPPING_OPTIONS));
        } else if (mSelectedSection == mContactDetailsSection) {
            mClient.getSectionInformation(
                    TYPE_CONTACT_DETAILS, createUpdateSectionCallback(TYPE_CONTACT_DETAILS));
        } else if (mSelectedSection == mPaymentMethodSection) {
            mClient.getSectionInformation(
                    TYPE_PAYMENT_METHODS, createUpdateSectionCallback(TYPE_PAYMENT_METHODS));
        } else {
            updateSectionVisibility();
        }
    }

    private Callback<SectionInformation> createUpdateSectionCallback(@DataType final int type) {
        return new Callback<SectionInformation>() {
            @Override
            public void onResult(SectionInformation result) {
                updateSection(type, result);
                updateSectionVisibility();
            }
        };
    }

    /** Update the display status of each expandable section. */
    private void updateSectionVisibility() {
        startSectionResizeAnimation();

        mOrderSummarySection.setDisplayMode(mSelectedSection == mOrderSummarySection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mShippingAddressSection.setDisplayMode(mSelectedSection == mShippingAddressSection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mShippingOptionSection.setDisplayMode(mSelectedSection == mShippingOptionSection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mContactDetailsSection.setDisplayMode(mSelectedSection == mContactDetailsSection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mPaymentMethodSection.setDisplayMode(mSelectedSection == mPaymentMethodSection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
    }

    /**
     * Called when the dialog is dismissed. Can be caused by:
     * <ul>
     *  <li>User click on the "back" button on the phone.</li>
     *  <li>User click on the "X" button in the top-right corner of the dialog.</li>
     *  <li>User click on the "CANCEL" button on the bottom of the dialog.</li>
     *  <li>Successfully processing the payment.</li>
     *  <li>Failure to process the payment.</li>
     *  <li>The JavaScript calling the abort() method in PaymentRequest API.</li>
     *  <li>The PaymentRequest JavaScript object being destroyed.</li>
     * </ul>
     */
    @Override
    public void onDismiss(DialogInterface dialog) {
        if (sObserverForTest != null) sObserverForTest.onPaymentRequestDismiss();
        if (!mIsClientClosing) mClient.onDismiss();
    }

    @Override
    public String getAdditionalText(OptionSection section) {
        if (section == mShippingAddressSection) {
            int selectedItemIndex = mShippingAddressSectionInformation.getSelectedItemIndex();
            boolean isNecessary = mClient.merchantNeedsShippingAddress()
                    && (selectedItemIndex == SectionInformation.NO_SELECTION
                               || selectedItemIndex == SectionInformation.INVALID_SELECTION);
            return isNecessary
                    ? mContext.getString(selectedItemIndex == SectionInformation.NO_SELECTION
                            ? R.string.payments_select_shipping_address_for_shipping_methods
                            : R.string.payments_unsupported_shipping_address)
                    : null;
        }
        return null;
    }

    @Override
    public boolean isAdditionalTextDisplayingWarning(OptionSection section) {
        return section == mShippingAddressSection
                && mShippingAddressSectionInformation != null
                && mShippingAddressSectionInformation.getSelectedItemIndex()
                        == SectionInformation.INVALID_SELECTION;
    }

    /**
     * Animates the different sections of the dialog expanding and contracting into their final
     * positions.
     */
    private void startSectionResizeAnimation() {
        Runnable animationEndRunnable = new Runnable() {
            @Override
            public void run() {
                mSectionAnimator = null;
                notifyReadyToClose();
                notifyReadyForInput();
                notifyReadyToPay();
            }
        };

        mSectionAnimator =
                new FocusAnimator(mPaymentContainerLayout, mSelectedSection, animationEndRunnable);
    }

    /**
     * Animates the whole dialog fading in and darkening everything else on screen.
     * This particular animation is not tracked because it is not meant to be cancellable.
     */
    private class FadeInAnimator
            extends AnimatorListenerAdapter implements OnLayoutChangeListener {
        @Override
        public void onLayoutChange(View v, int left, int top, int right, int bottom,
                int oldLeft, int oldTop, int oldRight, int oldBottom) {
            mRequestView.removeOnLayoutChangeListener(this);

            Animator scrimFader = ObjectAnimator.ofInt(mFullContainer.getBackground(),
                    AnimatorProperties.DRAWABLE_ALPHA_PROPERTY, 0, 127);
            Animator alphaAnimator = ObjectAnimator.ofFloat(mFullContainer, View.ALPHA, 0f, 1f);

            AnimatorSet alphaSet = new AnimatorSet();
            alphaSet.playTogether(scrimFader, alphaAnimator);
            alphaSet.setDuration(DIALOG_ENTER_ANIMATION_MS);
            alphaSet.setInterpolator(new LinearOutSlowInInterpolator());
            alphaSet.start();
        }
    }

    /**
     * Animates the bottom sheet UI translating upwards from the bottom of the screen.
     * Can be canceled when a {@link SheetEnlargingAnimator} starts and expands the dialog.
     */
    private class PeekingAnimator implements OnLayoutChangeListener {
        @Override
        public void onLayoutChange(View v, int left, int top, int right, int bottom,
                int oldLeft, int oldTop, int oldRight, int oldBottom) {
            mRequestView.removeOnLayoutChangeListener(this);

            mSheetAnimator = ObjectAnimator.ofFloat(
                    mRequestView, View.TRANSLATION_Y, mAnimatorTranslation, 0);
            mSheetAnimator.setDuration(DIALOG_ENTER_ANIMATION_MS);
            mSheetAnimator.setInterpolator(new LinearOutSlowInInterpolator());
            mSheetAnimator.start();
        }
    }

    /** Animates the bottom sheet expanding to a larger sheet. */
    private class SheetEnlargingAnimator
            extends AnimatorListenerAdapter implements OnLayoutChangeListener {
        private final boolean mIsButtonBarLockedInPlace;
        private int mContainerHeightDifference;

        public SheetEnlargingAnimator(boolean isButtonBarLockedInPlace) {
            mIsButtonBarLockedInPlace = isButtonBarLockedInPlace;
        }

        /**
         * Updates the animation.
         *
         * @param progress How far along the animation is.  In the range [0,1], with 1 being done.
         */
        private void update(float progress) {
            // The dialog container initially starts off translated downward, gradually decreasing
            // the translation until it is in the right place on screen.
            float containerTranslation = mContainerHeightDifference * progress;
            mRequestView.setTranslationY(containerTranslation);

            if (mIsButtonBarLockedInPlace) {
                // The button bar is translated along the dialog so that is looks like it stays in
                // place at the bottom while the entire bottom sheet is translating upwards.
                mButtonBar.setTranslationY(-containerTranslation);

                // The payment container is sandwiched between the header and the button bar.
                // Expansion animates by changing where its "bottom" is, letting its shadows appear
                // and disappear as it changes size.
                int paymentContainerBottom = Math.min(
                        mPaymentContainer.getTop() + mPaymentContainer.getMeasuredHeight(),
                        mButtonBar.getTop());
                mPaymentContainer.setBottom(paymentContainerBottom);
            }
        }

        @Override
        public void onLayoutChange(View v, int left, int top, int right, int bottom,
                int oldLeft, int oldTop, int oldRight, int oldBottom) {
            if (mSheetAnimator != null) mSheetAnimator.cancel();

            mRequestView.removeOnLayoutChangeListener(this);
            mContainerHeightDifference = (bottom - top) - (oldBottom - oldTop);

            ValueAnimator containerAnimator = ValueAnimator.ofFloat(1f, 0f);
            containerAnimator.addUpdateListener(new AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    float alpha = (Float) animation.getAnimatedValue();
                    update(alpha);
                }
            });

            mSheetAnimator = containerAnimator;
            mSheetAnimator.setDuration(DIALOG_ENTER_ANIMATION_MS);
            mSheetAnimator.setInterpolator(new LinearOutSlowInInterpolator());
            mSheetAnimator.addListener(this);
            mSheetAnimator.start();
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            // Reset the layout so that everything is in the expected place.
            mRequestView.setTranslationY(0);
            mButtonBar.setTranslationY(0);
            mRequestView.requestLayout();

            // Indicate that the dialog is ready to use.
            mSheetAnimator = null;
            mIsInitialLayoutComplete = true;
            notifyReadyToClose();
            notifyReadyForInput();
            notifyReadyToPay();
        }
    }

    /** Animates the bottom sheet (and optionally, the scrim) disappearing off screen. */
    private class DisappearingAnimator extends AnimatorListenerAdapter {
        private final boolean mIsDialogClosing;

        public DisappearingAnimator(boolean removeDialog) {
            mIsDialogClosing = removeDialog;

            Animator sheetFader = ObjectAnimator.ofFloat(
                    mRequestView, View.ALPHA, mRequestView.getAlpha(), 0f);
            Animator sheetTranslator = ObjectAnimator.ofFloat(
                    mRequestView, View.TRANSLATION_Y, 0f, mAnimatorTranslation);

            AnimatorSet current = new AnimatorSet();
            current.setDuration(DIALOG_EXIT_ANIMATION_MS);
            current.setInterpolator(new FastOutLinearInInterpolator());
            if (mIsDialogClosing) {
                Animator scrimFader = ObjectAnimator.ofInt(mFullContainer.getBackground(),
                        AnimatorProperties.DRAWABLE_ALPHA_PROPERTY, 127, 0);
                current.playTogether(sheetFader, sheetTranslator, scrimFader);
            } else {
                current.playTogether(sheetFader, sheetTranslator);
            }

            mSheetAnimator = current;
            mSheetAnimator.addListener(this);
            mSheetAnimator.start();
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            mSheetAnimator = null;
            mFullContainer.removeView(mRequestView);
            if (mIsDialogClosing && mDialog.isShowing()) mDialog.dismiss();
        }
    }

    @VisibleForTesting
    public static void setObserverForTest(PaymentRequestObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }

    @VisibleForTesting
    public Dialog getDialogForTest() {
        return mDialog;
    }

    @VisibleForTesting
    public ViewGroup getShippingSummarySectionForTest() {
        return mShippingSummarySection;
    }

    @VisibleForTesting
    public ViewGroup getShippingAddressSectionForTest() {
        return mShippingAddressSection;
    }

    @VisibleForTesting
    public ViewGroup getContactDetailsSectionForTest() {
        return mContactDetailsSection;
    }

    private void notifyReadyForInput() {
        if (sObserverForTest != null && isAcceptingUserInput()) {
            sObserverForTest.onPaymentRequestReadyForInput(this);
        }
    }

    private void notifyReadyToPay() {
        if (sObserverForTest != null && isAcceptingUserInput() && mPayButton.isEnabled()) {
            sObserverForTest.onPaymentRequestReadyToPay(this);
        }
    }

    private void notifyReadyToClose() {
        if (sObserverForTest != null && isAcceptingCloseButton()) {
            sObserverForTest.onPaymentRequestReadyToClose(this);
        }
    }
}
