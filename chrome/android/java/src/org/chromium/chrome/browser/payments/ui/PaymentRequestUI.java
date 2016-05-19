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

import java.util.ArrayList;
import java.util.List;

/**
 * The PaymentRequest UI.
 */
public class PaymentRequestUI implements DialogInterface.OnDismissListener, View.OnClickListener,
        PaymentRequestSection.PaymentsSectionDelegate {
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
    private final PaymentRequestObserverForTest mObserverForTest;
    private final boolean mRequestShipping;

    private final Dialog mDialog;
    private final ViewGroup mFullContainer;
    private final ViewGroup mBottomSheetContainer;
    private final PaymentResultUI mResultUI;

    private final ScrollView mPaymentContainer;
    private final LinearLayout mPaymentContainerLayout;
    private final DualControlLayout mButtonBar;
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
    private boolean mIsClientClosing;

    private List<LineItem> mLineItems;
    private SectionInformation mPaymentMethodSectionInformation;
    private SectionInformation mShippingAddressSectionInformation;
    private SectionInformation mShippingOptionsSectionInformation;

    private Animator mSheetAnimator;
    private FocusAnimator mSectionAnimator;
    private int mAnimatorTranslation;
    private boolean mIsInitialLayoutComplete;

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
     * @return The UI for PaymentRequest.
     */
    public static PaymentRequestUI show(Activity activity, Client client, boolean requestShipping,
            String title, String origin) {
        PaymentRequestUI ui = new PaymentRequestUI(activity, client, requestShipping, title, origin,
                sObserverForTest);
        sObserverForTest = null;
        return ui;
    }

    private PaymentRequestUI(Activity activity, Client client, boolean requestShipping,
            String title, String origin, PaymentRequestObserverForTest observerForTest) {
        mContext = activity;
        mClient = client;
        mObserverForTest = observerForTest;
        mRequestShipping = requestShipping;
        mAnimatorTranslation = activity.getResources().getDimensionPixelSize(
                R.dimen.payments_ui_translation);

        // Inflate the layout.
        mFullContainer = new FrameLayout(mContext);
        mFullContainer.setBackgroundColor(
                ApiCompatibilityUtils.getColor(mContext.getResources(), R.color.payments_ui_scrim));
        LayoutInflater.from(mContext).inflate(R.layout.payment_request, mFullContainer);

        mBottomSheetContainer =
                (ViewGroup) mFullContainer.findViewById(R.id.payment_request_layout);
        mResultUI = new PaymentResultUI(mContext, title, origin);

        mPaymentContainer = (ScrollView) mBottomSheetContainer.findViewById(R.id.paymentContainer);
        ((TextView) mBottomSheetContainer.findViewById(R.id.pageTitle)).setText(title);
        ((TextView) mBottomSheetContainer.findViewById(R.id.hostname)).setText(origin);

        // Set up the buttons.
        mCloseButton = mBottomSheetContainer.findViewById(R.id.close_button);
        mCloseButton.setOnClickListener(this);
        mPayButton = DualControlLayout.createButtonForLayout(
                activity, true, activity.getString(R.string.payments_pay_button), this);
        mEditButton = DualControlLayout.createButtonForLayout(
                activity, false, activity.getString(R.string.payments_edit_button), this);
        mButtonBar = (DualControlLayout) mBottomSheetContainer.findViewById(R.id.buttonBar);
        mButtonBar.setAlignment(DualControlLayout.ALIGN_END);
        mButtonBar.setStackedMargin(activity.getResources().getDimensionPixelSize(
                R.dimen.infobar_margin_between_stacked_buttons));
        mButtonBar.addView(mPayButton);
        mButtonBar.addView(mEditButton);

        // Create all the possible sections.
        mSectionSeparators = new ArrayList<SectionSeparator>();
        mPaymentContainerLayout =
                (LinearLayout) mBottomSheetContainer.findViewById(R.id.paymentContainerLayout);
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
        mBottomSheetContainer.addOnLayoutChangeListener(new FadeInAnimator());
        mBottomSheetContainer.addOnLayoutChangeListener(new PeekingAnimator());

        // Enabled in updatePayButtonEnabled() when the user has selected all payment options.
        mPayButton.setEnabled(false);

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

                // Hide the loading indicators and show the real sections.
                mPaymentContainer.setVisibility(View.VISIBLE);
                mButtonBar.setVisibility(View.VISIBLE);
                mBottomSheetContainer.removeView(
                        mBottomSheetContainer.findViewById(R.id.waiting_progress));
                mBottomSheetContainer.removeView(
                        mBottomSheetContainer.findViewById(R.id.waiting_message));
                mBottomSheetContainer.addOnLayoutChangeListener(new SheetEnlargingAnimator(false));
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
        mIsClientClosing = true;
        mResultUI.update(paymentSuccess, new Runnable() {
            @Override
            public void run() {
                dismissDialog(false);
                if (callback != null) callback.run();
            }
        });
        if (mObserverForTest != null) mObserverForTest.onPaymentRequestResultReady(this);
    }

    /**
     * Sets the icon in the top left of the UI. This can be, for example, the favicon of the
     * merchant website. This is not a part of the constructor because favicon retrieval is
     * asynchronous.
     *
     * @param bitmap The bitmap to show next to the title.
     */
    public void setTitleBitmap(Bitmap bitmap) {
        ((ImageView) mBottomSheetContainer.findViewById(R.id.pageFavIcon)).setImageBitmap(bitmap);
        mResultUI.setBitmap(bitmap);
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

    private void updateShippingAddressSection(SectionInformation section) {
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
        updatePayButtonEnabled();
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

        // Collapse all sections after an option is selected.
        expand(null);

        updatePayButtonEnabled();
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
        } else if (v == mPaymentMethodSection) {
            expand(mPaymentMethodSection);
        } else if (v == mPayButton) {
            showResultDialog();

            mClient.onPayClicked(
                    mShippingAddressSectionInformation == null
                            ? null : mShippingAddressSectionInformation.getSelectedItem(),
                    mShippingOptionsSectionInformation == null
                            ? null : mShippingOptionsSectionInformation.getSelectedItem(),
                    mPaymentMethodSectionInformation.getSelectedItem());
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

    private void showResultDialog() {
        // Animate the bottom sheet going away, but keep the scrim visible.
        new DisappearingAnimator(false);

        int floatingDialogWidth = PaymentResultUI.computeMaxWidth(
                mContext, mFullContainer.getMeasuredWidth(), mFullContainer.getMeasuredHeight());
        FrameLayout.LayoutParams overlayParams =
                new FrameLayout.LayoutParams(floatingDialogWidth, LayoutParams.WRAP_CONTENT);
        overlayParams.gravity = Gravity.CENTER;
        mFullContainer.addView(mResultUI.getView(), overlayParams);
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

        notifyReadyToPay();
    }

    /** @return Whether or not the dialog can be closed via the X close button. */
    private boolean isAcceptingCloseButton() {
        return mSheetAnimator == null && mSectionAnimator == null && mIsInitialLayoutComplete;
    }

    /** @return Whether or not the dialog is accepting user input. */
    @Override
    public boolean isAcceptingUserInput() {
        return isAcceptingCloseButton() && mPaymentMethodSectionInformation != null;
    }

    private void expand(ViewGroup section) {
        if (!mIsShowingEditDialog) {
            // Container now takes the full height of the screen, animating towards it.
            mBottomSheetContainer.getLayoutParams().height = LayoutParams.MATCH_PARENT;
            mBottomSheetContainer.addOnLayoutChangeListener(new SheetEnlargingAnimator(true));

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
            mClient.getLineItems(new Callback<List<LineItem>>() {
                @Override
                public void onResult(List<LineItem> result) {
                    updateOrderSummarySection(result);
                    updateSectionVisibility();
                }
            });
        } else if (mSelectedSection == mShippingAddressSection) {
            mClient.getShippingAddresses(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    updateShippingAddressSection(result);
                    updateSectionVisibility();
                }
            });
        } else if (mSelectedSection == mShippingOptionSection) {
            mClient.getShippingOptions(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    updateShippingOptionsSection(result);
                    updateSectionVisibility();
                }
            });
        } else if (mSelectedSection == mPaymentMethodSection) {
            mClient.getPaymentMethods(new Callback<SectionInformation>() {
                @Override
                public void onResult(SectionInformation result) {
                    updatePaymentMethodSection(result);
                    updateSectionVisibility();
                }
            });
        } else {
            updateSectionVisibility();
        }
    }

    /** Update the display status of each expandable section. */
    private void updateSectionVisibility() {
        Runnable animationEndRunnable = new Runnable() {
            @Override
            public void run() {
                mSectionAnimator = null;
                notifyReadyToClose();
                notifyReadyForInput();
                notifyReadyToPay();
            }
        };
        mSectionAnimator = new FocusAnimator(
                mPaymentContainerLayout, mSelectedSection, animationEndRunnable);

        mOrderSummarySection.setDisplayMode(mSelectedSection == mOrderSummarySection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mShippingAddressSection.setDisplayMode(mSelectedSection == mShippingAddressSection
                ? PaymentRequestSection.DISPLAY_MODE_FOCUSED
                : PaymentRequestSection.DISPLAY_MODE_EXPANDABLE);
        mShippingOptionSection.setDisplayMode(mSelectedSection == mShippingOptionSection
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
        if (mObserverForTest != null) mObserverForTest.onPaymentRequestDismiss();
        if (!mIsClientClosing) mClient.onDismiss();
    }

    /**
     * Animates the whole dialog fading in and darkening everything else on screen.
     * This particular animation is not tracked because it is not meant to be cancelable.
     */
    private class FadeInAnimator
            extends AnimatorListenerAdapter implements OnLayoutChangeListener {
        @Override
        public void onLayoutChange(View v, int left, int top, int right, int bottom,
                int oldLeft, int oldTop, int oldRight, int oldBottom) {
            mBottomSheetContainer.removeOnLayoutChangeListener(this);

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
            mBottomSheetContainer.removeOnLayoutChangeListener(this);

            mSheetAnimator = ObjectAnimator.ofFloat(
                    mBottomSheetContainer, View.TRANSLATION_Y, mAnimatorTranslation, 0);
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
            mBottomSheetContainer.setTranslationY(containerTranslation);

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

            mBottomSheetContainer.removeOnLayoutChangeListener(this);
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
            mBottomSheetContainer.setTranslationY(0);
            mButtonBar.setTranslationY(0);
            mBottomSheetContainer.requestLayout();

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
                    mBottomSheetContainer, View.ALPHA, mBottomSheetContainer.getAlpha(), 0f);
            Animator sheetTranslator = ObjectAnimator.ofFloat(
                    mBottomSheetContainer, View.TRANSLATION_Y, 0f, mAnimatorTranslation);

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
            mFullContainer.removeView(mBottomSheetContainer);
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

    private void notifyReadyForInput() {
        if (mObserverForTest != null && isAcceptingUserInput()) {
            mObserverForTest.onPaymentRequestReadyForInput(this);
        }
    }

    private void notifyReadyToPay() {
        if (mObserverForTest != null && isAcceptingUserInput() && mPayButton.isEnabled()) {
            mObserverForTest.onPaymentRequestReadyToPay(this);
        }
    }

    private void notifyReadyToClose() {
        if (mObserverForTest != null && isAcceptingCloseButton()) {
            mObserverForTest.onPaymentRequestReadyToClose(this);
        }
    }
}
