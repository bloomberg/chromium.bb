// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import static android.widget.LinearLayout.VERTICAL;

import static org.chromium.chrome.browser.payments.ui.PaymentRequestSection.EDIT_BUTTON_GONE;

import android.app.Activity;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.payments.ShippingStrings;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.OptionSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection.SectionSeparator;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.DataType;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.SelectionResult;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.browser.widget.FadingEdgeScrollView;
import org.chromium.chrome.browser.widget.animation.FocusAnimator;
import org.chromium.chrome.browser.widget.prefeditor.EditableOption;
import org.chromium.chrome.browser.widget.prefeditor.EditorDialog;
import org.chromium.ui.text.SpanApplier;

import java.util.ArrayList;
import java.util.List;

/**
 * The AssistantPaymentRequestUI UI.
 *
 * TODO(crbug.com/806868: Further extract shared logic between PaymentRequestUI and
 * AssistantPaymentRequestUI.
 */
public class AssistantPaymentRequestUI
        implements View.OnClickListener, PaymentRequestSection.SectionDelegate {
    /**
     * The interface to be implemented by the consumer of the AssistantPaymentRequestUI UI.
     */
    public interface Client {
        /**
         * Returns the full list of options for the given type.
         *
         * @param optionType Data being updated.
         */
        @Nullable
        SectionInformation getSectionInformation(@DataType int optionType);

        /**
         * Called when the user changes one of their payment options.
         *
         * If this method returns {@link SelectionResult.EDITOR_LAUNCH}, then:
         * + Interaction with UI should be disabled until updateSection() is called.
         *
         * @param optionType Data being updated.
         * @param option     Value of the data being updated.
         * @return The result of the selection.
         */
        @SelectionResult
        int onSectionOptionSelected(@DataType int optionType, EditableOption option);

        /**
         * Called when the user clicks edit icon (pencil icon) on the payment option in a section.
         *
         * If this method returns {@link SelectionResult.EDITOR_LAUNCH}, then:
         * + Interaction with UI should be disabled until updateSection() is called.
         *
         * @param optionType Data being updated.
         * @param option     The option to be edited.
         * @return The result of the edit request.
         */
        @SelectionResult
        int onSectionEditOption(@DataType int optionType, EditableOption option);

        /**
         * Called when the user clicks on the "Add" button for a section.
         *
         * If this method returns {@link SelectionResult.EDITOR_LAUNCH}, then:
         * + Interaction with UI should be disabled until updateSection() is called.
         *
         * @param optionType Data being updated.
         * @return The result of the selection.
         */
        @SelectionResult
        int onSectionAddOption(@DataType int optionType);

        /**
         * Called when the user clicks on the radio button for accepting the website's terms and
         * conditions.
         *
         * @param checked The check status.
         */
        void onCheckAcceptTermsAndConditions(boolean checked);

        /**
         * Called when the user clicks on the radio button for reviewing the website's terms and
         * conditions.
         *
         * @param checked The check status.
         */
        void onCheckReviewTermsAndConditions(boolean checked);
    }

    private final Activity mActivity;
    private final Client mClient;

    private final EditorDialog mEditorDialog;
    private final EditorDialog mCardEditorDialog;

    private LinearLayout mRequestViewContainer;
    private FadingEdgeScrollView mRequestViewSrollView;
    private ViewGroup mRequestView;
    private LinearLayout mPaymentContainerLayout;

    private boolean mRequestShipping;
    private boolean mRequestPaymentMethod;
    private boolean mRequestContactDetails;
    private ShippingStrings mShippingStrings;
    private OptionSection mShippingAddressSection;
    private OptionSection mContactDetailsSection;
    private OptionSection mPaymentMethodSection;
    private List<SectionSeparator> mSectionSeparators;
    private PaymentRequestSection mSelectedSection;
    private SectionInformation mPaymentMethodSectionInformation;
    private SectionInformation mShippingAddressSectionInformation;
    private SectionInformation mContactDetailsSectionInformation;

    private FocusAnimator mSectionAnimator;

    /**
     * Constructor of AssistantPaymentRequestUI.
     *
     * @param activity The activity on top of which the UI should be displayed.
     * @param client   The AssistantPaymentRequestUI.Client.
     */
    public AssistantPaymentRequestUI(Activity activity, Client client) {
        mActivity = activity;
        mClient = client;

        mRequestViewContainer = new LinearLayout(activity, null);
        mRequestViewContainer.setOrientation(VERTICAL);
        mRequestViewContainer.setLayoutParams(
                new LinearLayout.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ 0, /* weight= */ 1));

        mRequestViewSrollView = new FadingEdgeScrollView(activity, null);
        mRequestViewContainer.addView(mRequestViewSrollView,
                new LinearLayout.LayoutParams(
                        /*width=*/LayoutParams.MATCH_PARENT, /*height=*/LayoutParams.WRAP_CONTENT));

        mEditorDialog = new EditorDialog(mActivity, null,
                /*deleteRunnable =*/null);
        mCardEditorDialog = new EditorDialog(mActivity, null,
                /*deleteRunnable =*/null);

        // Allow screenshots of the credit card number in Canary, Dev, and developer builds.
        if (ChromeVersionInfo.isBetaBuild() || ChromeVersionInfo.isStableBuild()) {
            mCardEditorDialog.disableScreenshots();
        }
    }

    /**
     * Gets the root view of AssistantPaymentRequestUI.
     */
    public View getView() {
        return mRequestViewContainer;
    }

    /**
     * Returns the scroll view of AssistantPaymentRequestUI.
     */
    public ScrollView getScrollView() {
        return mRequestViewSrollView;
    }

    /*
     * Shows the AssistantPaymentRequestUI. Should not be called multiple times before close is
     * called.
     *
     * @param origin          The origin of the information will be send to.
     * @param requestShipping Whether the UI should show the shipping address selection.
     * @param requestPaymentMethod Whether the UI should show the payment method selection.
     * @param requestContact  Whether the UI should show the payer name, email address and phone
     *                        number selection.
     * @param shippingStrings The string resource identifiers to use in the shipping sections.
     *
     */
    public void show(String origin, boolean requestShipping, boolean requestPaymentMethod,
            boolean requestContact, ShippingStrings shippingStrings) {
        mRequestShipping = requestShipping;
        mRequestPaymentMethod = requestPaymentMethod;
        mRequestContactDetails = requestContact;
        mShippingStrings = shippingStrings;

        assert mRequestView == null;
        mRequestView = (ViewGroup) LayoutInflater.from(mActivity).inflate(
                R.layout.autofill_assistant_payment_request, null);
        mRequestViewSrollView.addView(mRequestView);
        prepareRequestView(origin);
    }

    private void prepareRequestView(String origin) {
        // Set terms & conditions text.
        AssistantChoiceList thirdPartyTermsList =
                mRequestView.findViewById(R.id.third_party_terms_list);
        TextView acceptThirdPartyConditions =
                thirdPartyTermsList.findViewById(R.id.terms_checkbox_agree);
        TextView reviewThirdPartyConditions =
                thirdPartyTermsList.findViewById(R.id.terms_checkbox_review);
        StyleSpan boldSpan = new StyleSpan(android.graphics.Typeface.BOLD);
        acceptThirdPartyConditions.setText(SpanApplier.applySpans(
                mActivity.getString(R.string.autofill_assistant_3rd_party_terms_accept, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));
        reviewThirdPartyConditions.setText(SpanApplier.applySpans(
                mActivity.getString(R.string.autofill_assistant_3rd_party_terms_review, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));
        thirdPartyTermsList.setOnItemSelectedListener((view) -> {
            if (view == acceptThirdPartyConditions) {
                mClient.onCheckAcceptTermsAndConditions(true);
            } else if (view == reviewThirdPartyConditions) {
                mClient.onCheckReviewTermsAndConditions(true);
            }
        });

        // Set 3rd party privacy notice text.
        TextView thirdPartyPrivacyNotice =
                mRequestView.findViewById(R.id.payment_request_3rd_party_privacy_notice);
        thirdPartyPrivacyNotice.setText(SpanApplier.applySpans(
                mActivity.getString(R.string.autofill_assistant_3rd_party_privacy_notice, origin),
                new SpanApplier.SpanInfo("<b>", "</b>", boldSpan)));

        // Create all the possible sections.
        mSectionSeparators = new ArrayList<>();
        mPaymentContainerLayout =
                (LinearLayout) mRequestView.findViewById(R.id.payment_container_layout);
        mShippingAddressSection = new AssistantPaymentRequestSection(
                mActivity, mActivity.getString(mShippingStrings.getAddressLabel()), this);
        mContactDetailsSection = new AssistantPaymentRequestSection(mActivity,
                mActivity.getString(org.chromium.chrome.R.string.payments_contact_details_label),
                this);
        mPaymentMethodSection = new AssistantPaymentRequestSection(mActivity,
                mActivity.getString(org.chromium.chrome.R.string.payments_method_of_payment_label),
                this);

        // Display the summary of the selected address in multiple lines on bottom sheet.
        mShippingAddressSection.setDisplaySummaryInSingleLineInNormalMode(false);

        // Always allow adding payment methods.
        mPaymentMethodSection.setCanAddItems(true);

        // Add a separator at the beginning.
        mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout));

        // Add the necessary sections to the layout.
        if (mRequestContactDetails) {
            mPaymentContainerLayout.addView(mContactDetailsSection,
                    new LinearLayout.LayoutParams(
                            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        if (mRequestShipping) {
            if (mRequestContactDetails)
                mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout));
            // The shipping breakout sections are only added if they are needed.
            mPaymentContainerLayout.addView(mShippingAddressSection,
                    new LinearLayout.LayoutParams(
                            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        if (mRequestPaymentMethod) {
            if (mRequestContactDetails || mRequestShipping)
                mSectionSeparators.add(new SectionSeparator(mPaymentContainerLayout));
            mPaymentContainerLayout.addView(mPaymentMethodSection,
                    new LinearLayout.LayoutParams(
                            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        // Always expand separators to make them align with the rest of the UI.
        for (int i = 0; i < mSectionSeparators.size(); i++) {
            mSectionSeparators.get(i).expand();
        }

        if (mRequestContactDetails) {
            updateSection(DataType.CONTACT_DETAILS,
                    mClient.getSectionInformation(DataType.CONTACT_DETAILS));
        }
        if (mRequestShipping) {
            updateSection(DataType.SHIPPING_ADDRESSES,
                    mClient.getSectionInformation(DataType.SHIPPING_ADDRESSES));
        }
        if (mRequestPaymentMethod) {
            updateSection(DataType.PAYMENT_METHODS,
                    mClient.getSectionInformation(DataType.PAYMENT_METHODS));
        }

        // Force the initial appearance of edit chevrons next to all sections.
        updateSectionVisibility();
    }

    /**
     * Closes the UI.
     *
     * Note that it clears dynamically added sections and separators. The caller is responsible for
     * hiding this UI.
     */
    public void close() {
        if (mEditorDialog.isShowing()) mEditorDialog.dismiss();
        if (mCardEditorDialog.isShowing()) mCardEditorDialog.dismiss();
        if (mRequestViewSrollView != null) mRequestViewSrollView.removeAllViews();
        if (mRequestView != null) mRequestView.removeAllViews();
        mRequestView = null;
        mShippingAddressSection = null;
        mContactDetailsSection = null;
        mPaymentMethodSection = null;
        mShippingAddressSectionInformation = null;
        mSectionSeparators = null;
    }

    /**
     * Gets whether the UI is closed.
     */
    public boolean isClosed() {
        return mRequestView == null;
    }

    /**
     * Updates the UI to account for changes in section information.
     *
     * @param whichSection Which section to update.
     * @param section      The new section information.
     */
    public void updateSection(@DataType int whichSection, SectionInformation section) {
        if (whichSection == DataType.SHIPPING_ADDRESSES) {
            mShippingAddressSectionInformation = section;
            mShippingAddressSection.update(section);
        } else if (whichSection == DataType.CONTACT_DETAILS) {
            mContactDetailsSectionInformation = section;
            mContactDetailsSection.update(section);
        } else if (whichSection == DataType.PAYMENT_METHODS) {
            mPaymentMethodSectionInformation = section;
            mPaymentMethodSection.update(section);
        } else {
            // Only support the above sections.
            assert false : "Unsupported section type.";
        }

        updateSectionButtons();
    }

    @Override
    public void onEditableOptionChanged(
            final PaymentRequestSection section, EditableOption option) {
        @SelectionResult
        int result = SelectionResult.NONE;
        if (section == mShippingAddressSection) {
            mShippingAddressSectionInformation.setSelectedItem(option);
            result = mClient.onSectionOptionSelected(DataType.SHIPPING_ADDRESSES, option);
        } else if (section == mContactDetailsSection) {
            mContactDetailsSectionInformation.setSelectedItem(option);
            result = mClient.onSectionOptionSelected(DataType.CONTACT_DETAILS, option);
        } else if (section == mPaymentMethodSection) {
            mPaymentMethodSectionInformation.setSelectedItem(option);
            result = mClient.onSectionOptionSelected(DataType.PAYMENT_METHODS, option);
        } else {
            // Only support the above sections.
            assert false : "Unsupported section type.";
        }

        expand(null);
    }

    @Override
    public void onEditEditableOption(final PaymentRequestSection section, EditableOption option) {
        @SelectionResult
        int result = SelectionResult.NONE;

        if (section == mShippingAddressSection) {
            assert mShippingAddressSectionInformation.getSelectedItem() == option;
            result = mClient.onSectionEditOption(DataType.SHIPPING_ADDRESSES, option);
        } else if (section == mContactDetailsSection) {
            assert mContactDetailsSectionInformation.getSelectedItem() == option;
            result = mClient.onSectionEditOption(DataType.CONTACT_DETAILS, option);
        } else if (section == mPaymentMethodSection) {
            assert mPaymentMethodSectionInformation.getSelectedItem() == option;
            result = mClient.onSectionEditOption(DataType.PAYMENT_METHODS, option);
        } else {
            // Only support the above sections.
            assert false : "Unsupported section type.";
        }

        expand(null);
    }

    @Override
    public void onAddEditableOption(PaymentRequestSection section) {
        @SelectionResult
        int result = SelectionResult.NONE;
        if (section == mShippingAddressSection) {
            result = mClient.onSectionAddOption(DataType.SHIPPING_ADDRESSES);
        } else if (section == mContactDetailsSection) {
            result = mClient.onSectionAddOption(DataType.CONTACT_DETAILS);
        } else if (section == mPaymentMethodSection) {
            result = mClient.onSectionAddOption(DataType.PAYMENT_METHODS);
        } else {
            // Only support the above sections.
            assert false : "Unsupported section type.";
        }

        expand(null);
    }

    @Override
    public boolean isBoldLabelNeeded(PaymentRequestSection section) {
        return section == mShippingAddressSection;
    }

    /** @return The common editor user interface. */
    public EditorDialog getEditorDialog() {
        return mEditorDialog;
    }

    /**
     * @return The card editor user interface. Distinct from the common editor user interface,
     * because the credit card editor can launch the address editor.
     */
    public EditorDialog getCardEditorDialog() {
        return mCardEditorDialog;
    }

    /**
     * Called when user clicks anything.
     */
    @Override
    public void onClick(View v) {
        // Users can only expand incomplete sections by clicking on their edit buttons.
        if (v instanceof PaymentRequestSection) {
            PaymentRequestSection section = (PaymentRequestSection) v;
            if (section.getEditButtonState() != EDIT_BUTTON_GONE) return;
        }

        if (v == mShippingAddressSection) {
            expand(mShippingAddressSection);
        } else if (v == mContactDetailsSection) {
            expand(mContactDetailsSection);
        } else if (v == mPaymentMethodSection) {
            expand(mPaymentMethodSection);
        }
    }

    /** @return Whether or not the UI is accepting user input. */
    @Override
    public boolean isAcceptingUserInput() {
        return mSectionAnimator == null;
    }

    private void expand(PaymentRequestSection section) {
        // Update the section contents when they're selected.
        mSelectedSection = section;
        if (mSelectedSection == mShippingAddressSection) {
            updateSection(DataType.SHIPPING_ADDRESSES,
                    mClient.getSectionInformation(DataType.SHIPPING_ADDRESSES));
        } else if (mSelectedSection == mContactDetailsSection) {
            updateSection(DataType.CONTACT_DETAILS,
                    mClient.getSectionInformation(DataType.CONTACT_DETAILS));
        } else if (mSelectedSection == mPaymentMethodSection) {
            updateSection(DataType.PAYMENT_METHODS,
                    mClient.getSectionInformation(DataType.PAYMENT_METHODS));
        }

        updateSectionVisibility();
    }

    /** Update the display status of each expandable section. */
    private void updateSectionVisibility() {
        startSectionResizeAnimation();
        mShippingAddressSection.focusSection(mSelectedSection == mShippingAddressSection);
        mContactDetailsSection.focusSection(mSelectedSection == mContactDetailsSection);
        mPaymentMethodSection.focusSection(mSelectedSection == mPaymentMethodSection);
        updateSectionButtons();
    }

    /**
     * Updates the enabled/disabled state of each section's edit button.
     *
     * Only the top-most button is enabled -- the others are disabled so the user is directed
     * through the form from top to bottom.
     */
    private void updateSectionButtons() {
        boolean mayEnableButton = true;
        for (int i = 0; i < mPaymentContainerLayout.getChildCount(); i++) {
            View child = mPaymentContainerLayout.getChildAt(i);
            if (!(child instanceof PaymentRequestSection)) continue;

            PaymentRequestSection section = (PaymentRequestSection) child;
            section.setIsEditButtonEnabled(mayEnableButton);
            if (section.getEditButtonState() != EDIT_BUTTON_GONE) mayEnableButton = false;
        }
    }

    @Nullable
    @Override
    public String getAdditionalText(PaymentRequestSection section) {
        if (section == mShippingAddressSection) {
            int selectedItemIndex = mShippingAddressSectionInformation.getSelectedItemIndex();
            if (selectedItemIndex != SectionInformation.NO_SELECTION
                    && selectedItemIndex != SectionInformation.INVALID_SELECTION) {
                return null;
            }

            String customErrorMessage = mShippingAddressSectionInformation.getErrorMessage();
            if (selectedItemIndex == SectionInformation.INVALID_SELECTION
                    && !TextUtils.isEmpty(customErrorMessage)) {
                return customErrorMessage;
            }

            return mActivity.getString(selectedItemIndex == SectionInformation.NO_SELECTION
                            ? mShippingStrings.getSelectPrompt()
                            : mShippingStrings.getUnsupported());
        } else if (section == mPaymentMethodSection) {
            return mPaymentMethodSectionInformation.getAdditionalText();
        } else {
            return null;
        }
    }

    @Override
    public boolean isAdditionalTextDisplayingWarning(PaymentRequestSection section) {
        return section == mShippingAddressSection && mShippingAddressSectionInformation != null
                && mShippingAddressSectionInformation.getSelectedItemIndex()
                == SectionInformation.INVALID_SELECTION;
    }

    @Override
    public void onSectionClicked(PaymentRequestSection section) {
        expand(section);
    }

    /**
     * Animates the different sections of the sheet expanding and contracting into their final
     * positions.
     */
    private void startSectionResizeAnimation() {
        Runnable animationEndRunnable = new Runnable() {
            @Override
            public void run() {
                mSectionAnimator = null;
            }
        };

        mSectionAnimator =
                new FocusAnimator(mPaymentContainerLayout, mSelectedSection, animationEndRunnable);
    }
}
