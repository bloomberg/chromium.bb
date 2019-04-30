// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.app.Activity;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.PersonalDataManagerObserver;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillPaymentApp;
import org.chromium.chrome.browser.payments.BasicCardUtils;
import org.chromium.chrome.browser.payments.CardEditor;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.payments.PaymentInstrument;
import org.chromium.chrome.browser.widget.FadingEdgeScrollView;
import org.chromium.chrome.browser.widget.prefeditor.Completable;
import org.chromium.chrome.browser.widget.prefeditor.EditorDialog;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

// TODO(crbug.com/806868): Use mCarouselCoordinator to show chips.

/**
 * Coordinator for the Payment Request.
 */
public class AssistantPaymentRequestCoordinator {
    private static final Comparator<Completable> COMPLETENESS_COMPARATOR =
            (a, b) -> (b.isComplete() ? 1 : 0) - (a.isComplete() ? 1 : 0);

    private static final String DIVIDER_TAG = "divider";
    private static final String SECTION_TAG = "section";
    private final Activity mActivity;
    private final LinearLayout mPaymentRequestUI;
    private final FadingEdgeScrollView mPaymentRequestScrollView;
    private final AssistantVerticalExpanderAccordion mPaymentRequestExpanderAccordion;
    private final int mPaymentRequestSectionPadding;

    private AssistantPaymentRequestContactDetailsSection mContactDetailsSection;
    private AssistantPaymentRequestPaymentMethodSection mPaymentMethodSection;
    private AssistantPaymentRequestShippingAddressSection mShippingAddressSection;
    private PersonalDataManagerObserver mPersonalDataManagerObserver;
    private Map<String, PaymentMethodData> mPaymentMethodsData;
    private boolean mVisible;
    private boolean mForceInvisible;

    public AssistantPaymentRequestCoordinator(
            Activity activity, AssistantPaymentRequestModel model) {
        mActivity = activity;
        mPaymentRequestSectionPadding = activity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_section_padding);

        mPaymentRequestUI = new LinearLayout(mActivity);
        mPaymentRequestUI.setOrientation(LinearLayout.VERTICAL);
        mPaymentRequestUI.setLayoutParams(
                new LinearLayout.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ 0, /* weight= */ 1));

        mPaymentRequestExpanderAccordion = new AssistantVerticalExpanderAccordion(mActivity, null);
        mPaymentRequestExpanderAccordion.setOrientation(LinearLayout.VERTICAL);
        mPaymentRequestExpanderAccordion.setLayoutParams(
                new LinearLayout.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ 0, /* weight= */ 1));
        mPaymentRequestExpanderAccordion.setOnExpandedViewChangedListener(
                expander -> updateSectionPaddings());

        mPaymentRequestScrollView = new FadingEdgeScrollView(mActivity, null);
        mPaymentRequestScrollView.addView(mPaymentRequestExpanderAccordion,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mPaymentRequestUI.addView(mPaymentRequestScrollView,
                new LinearLayout.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ 0, /* weight= */ 1));

        // Payment request is initially hidden.
        setVisible(false);

        // Listen for model changes.
        model.addObserver((source, propertyKey)
                                  -> resetUI(model.get(AssistantPaymentRequestModel.WEB_CONTENTS),
                                          model.get(AssistantPaymentRequestModel.OPTIONS),
                                          model.get(AssistantPaymentRequestModel.DELEGATE)));
    }

    public View getView() {
        return mPaymentRequestUI;
    }

    public ScrollView getScrollView() {
        return mPaymentRequestScrollView;
    }

    private void setVisible(boolean visible) {
        mVisible = visible;
        updateVisibility();
    }

    private void updateVisibility() {
        int visibility = mVisible && !mForceInvisible ? View.VISIBLE : View.GONE;
        if (getView().getVisibility() != visibility) {
            getView().setVisibility(visibility);
        }
    }

    /** Force the view of this coordinator to be invisible. */
    public void setForceInvisible(boolean forceInvisible) {
        if (mForceInvisible == forceInvisible) return;

        mForceInvisible = forceInvisible;
        updateVisibility();
    }

    private void resetUI(@Nullable WebContents webContents,
            @Nullable AssistantPaymentRequestOptions options,
            @Nullable AssistantPaymentRequestDelegate delegate) {
        if (mPersonalDataManagerObserver != null) {
            PersonalDataManager.getInstance().unregisterDataObserver(mPersonalDataManagerObserver);
            mPersonalDataManagerObserver = null;
        }
        if (options == null || webContents == null || delegate == null) {
            setVisible(false);
            return;
        }
        // Remove dynamically created UI elements.
        mPaymentRequestExpanderAccordion.removeAllViews();

        boolean showContactDetailsSection = options.mRequestPayerName || options.mRequestPayerPhone
                || options.mRequestPayerEmail;
        if (showContactDetailsSection || options.mRequestShipping) {
            // Listen to changes to profile data and update sections when necessary.
            mPersonalDataManagerObserver = ()
                    -> AssistantPaymentRequestCoordinator.this.updateSections(webContents, options);
            PersonalDataManager.getInstance().registerDataObserver(mPersonalDataManagerObserver);
        }

        mContactDetailsSection = null;
        if (showContactDetailsSection) {
            mContactDetailsSection = createContactDetailsSection(webContents, options, delegate);
            createSeparator();
        }

        mPaymentMethodSection = null;
        mPaymentMethodsData = null;
        if (options.mRequestPaymentMethod) {
            mPaymentMethodSection = createPaymentMethodsSection(webContents, options, delegate);
            createSeparator();
        }

        mShippingAddressSection = null;
        if (options.mRequestShipping) {
            mShippingAddressSection = createShippingAddressSection(webContents, options, delegate);
            createSeparator();
        }
        createTermsAndConditionsSection(webContents, options, delegate);
        updateSections(webContents, options);
        setVisible(true);
    }

    /**
     * Adjusts the paddings between sections and dividers.
     */
    private void updateSectionPaddings() {
        boolean prevChildWasDivider = false;
        boolean prevChildWasSection = false;
        for (int i = 0; i < mPaymentRequestExpanderAccordion.getChildCount(); i++) {
            View child = mPaymentRequestExpanderAccordion.getChildAt(i);
            if (child.getTag() == null) {
                continue;
            }
            boolean isDivider = child.getTag().equals(DIVIDER_TAG);
            boolean isSection = child.getTag().equals(SECTION_TAG);
            if (isSection && prevChildWasDivider) {
                child.setPadding(child.getPaddingLeft(), mPaymentRequestSectionPadding,
                        child.getPaddingRight(), child.getPaddingBottom());
            } else if (isDivider && prevChildWasSection) {
                View prevChild = mPaymentRequestExpanderAccordion.getChildAt(i - 1);
                if (prevChild instanceof AssistantVerticalExpander) {
                    // For expanders, the padding must be set for all views separately to
                    // effectively enlarge their background (if any).
                    AssistantVerticalExpander expander = (AssistantVerticalExpander) prevChild;
                    View titleView = expander.getTitleView();
                    View collapsedView = expander.getCollapsedView();
                    View expandedView = expander.getExpandedView();
                    expander.setPadding(expander.getPaddingLeft(), expander.getPaddingTop(),
                            expander.getPaddingRight(), 0);
                    collapsedView.setPadding(collapsedView.getPaddingLeft(),
                            collapsedView.getPaddingTop(), collapsedView.getPaddingRight(),
                            mPaymentRequestSectionPadding);
                    expandedView.setPadding(expandedView.getPaddingLeft(),
                            expandedView.getPaddingTop(), expandedView.getPaddingRight(),
                            mPaymentRequestSectionPadding);

                    // Dividers need additional margin to fixed expanders.
                    ViewGroup.MarginLayoutParams lp =
                            (ViewGroup.MarginLayoutParams) child.getLayoutParams();
                    lp.topMargin = expander.isFixed() ? mPaymentRequestSectionPadding : 0;
                    child.setLayoutParams(lp);

                    // Hide divider if corresponding section is expanded.
                    child.setVisibility(expander.isExpanded() ? View.GONE : View.VISIBLE);
                } else {
                    prevChild.setPadding(prevChild.getPaddingLeft(), prevChild.getPaddingTop(),
                            prevChild.getPaddingRight(), mPaymentRequestSectionPadding);
                }
            }

            prevChildWasDivider = isDivider;
            prevChildWasSection = isSection;
        }
    }

    private void createSeparator() {
        View divider = LayoutInflater.from(mActivity).inflate(
                R.layout.autofill_assistant_payment_request_section_divider,
                mPaymentRequestExpanderAccordion, false);
        divider.setTag(DIVIDER_TAG);
        mPaymentRequestExpanderAccordion.addView(divider);
    }

    private void createTermsAndConditionsSection(WebContents webContents,
            AssistantPaymentRequestOptions options, AssistantPaymentRequestDelegate delegate) {
        AssistantPaymentRequestTermsSection termsSection =
                new AssistantPaymentRequestTermsSection(mActivity, mPaymentRequestExpanderAccordion,
                        UrlFormatter.formatUrlForSecurityDisplayOmitScheme(
                                webContents.getLastCommittedUrl()),
                        delegate::onTermsAndConditionsChanged);
        termsSection.getView().setTag(SECTION_TAG);
    }

    private AssistantPaymentRequestShippingAddressSection createShippingAddressSection(
            WebContents webContents, AssistantPaymentRequestOptions options,
            AssistantPaymentRequestDelegate delegate) {
        AddressEditor editor = new AddressEditor(AddressEditor.Purpose.PAYMENT_REQUEST,
                /* saveToDisk= */ !webContents.isIncognito());
        editor.setEditorDialog(new EditorDialog(mActivity, null,
                /*deleteRunnable =*/null));

        AssistantPaymentRequestShippingAddressSection shippingAddressSection =
                new AssistantPaymentRequestShippingAddressSection(
                        mActivity, mPaymentRequestExpanderAccordion);
        shippingAddressSection.setListener(address
                -> delegate.onShippingAddressChanged(
                        address != null ? address.getProfile() : null));
        shippingAddressSection.setEditor(editor);
        shippingAddressSection.getView().setTag(SECTION_TAG);
        return shippingAddressSection;
    }

    private AssistantPaymentRequestContactDetailsSection createContactDetailsSection(
            WebContents webContents, AssistantPaymentRequestOptions options,
            AssistantPaymentRequestDelegate delegate) {
        ContactEditor editor = new ContactEditor(options.mRequestPayerName,
                options.mRequestPayerPhone, options.mRequestPayerEmail, !webContents.isIncognito());
        editor.setEditorDialog(new EditorDialog(mActivity, null,
                /*deleteRunnable =*/null));

        AssistantPaymentRequestContactDetailsSection contactDetailsSection =
                new AssistantPaymentRequestContactDetailsSection(
                        mActivity, mPaymentRequestExpanderAccordion);

        contactDetailsSection.setListener(delegate::onContactInfoChanged);
        contactDetailsSection.setEditor(editor);
        contactDetailsSection.getView().setTag(SECTION_TAG);
        return contactDetailsSection;
    }

    private AssistantPaymentRequestPaymentMethodSection createPaymentMethodsSection(
            WebContents webContents, AssistantPaymentRequestOptions options,
            AssistantPaymentRequestDelegate delegate) {
        // Only enable 'basic-card' payment method.
        PaymentMethodData methodData = new PaymentMethodData();
        methodData.supportedMethod = BasicCardUtils.BASIC_CARD_METHOD_NAME;

        // Apply basic-card filter if specified
        String[] supportedBasicCardNetworks = options.mSupportedBasicCardNetworks;
        if (supportedBasicCardNetworks.length > 0) {
            ArrayList<Integer> filteredNetworks = new ArrayList<>();
            Map<String, Integer> networks = BasicCardUtils.getNetworkIdentifiers();
            for (int i = 0; i < supportedBasicCardNetworks.length; i++) {
                assert networks.containsKey(supportedBasicCardNetworks[i]);
                filteredNetworks.add(networks.get(supportedBasicCardNetworks[i]));
            }

            methodData.supportedNetworks = new int[filteredNetworks.size()];
            for (int i = 0; i < filteredNetworks.size(); i++) {
                methodData.supportedNetworks[i] = filteredNetworks.get(i);
            }
        }

        mPaymentMethodsData = new ArrayMap<>();
        mPaymentMethodsData.put(BasicCardUtils.BASIC_CARD_METHOD_NAME, methodData);
        AssistantPaymentRequestPaymentMethodSection paymentMethodsSection =
                new AssistantPaymentRequestPaymentMethodSection(
                        mActivity, mPaymentRequestExpanderAccordion);
        paymentMethodsSection.setListener(
                method -> delegate.onCreditCardChanged(method != null ? method.getCard() : null));

        paymentMethodsSection.getView().setTag(SECTION_TAG);
        return paymentMethodsSection;
    }

    private int countCompleteFields(
            AssistantPaymentRequestOptions options, AutofillProfile profile) {
        int completeFields = 0;
        if (options.mRequestShipping && !TextUtils.isEmpty(profile.getStreetAddress())) {
            completeFields++;
        }

        if (options.mRequestPayerEmail && !TextUtils.isEmpty(profile.getEmailAddress())) {
            completeFields++;
        }

        if (options.mRequestPayerName && !TextUtils.isEmpty((profile.getFullName()))) {
            completeFields++;
        }

        if (options.mRequestPayerPhone && !TextUtils.isEmpty(profile.getPhoneNumber())) {
            completeFields++;
        }

        return completeFields;
    }

    /**
     * Populates or updates items in all sections that are currently displayed.
     */
    private void updateSections(WebContents webContents, AssistantPaymentRequestOptions options) {
        List<AutofillProfile> autofillProfiles =
                PersonalDataManager.getInstance().getProfilesToSuggest(
                        /* includeNameInLabel= */ false);

        // Suggest complete profiles first.
        Collections.sort(autofillProfiles,
                (a, b) -> countCompleteFields(options, a) - countCompleteFields(options, b));

        if (mContactDetailsSection != null) {
            mContactDetailsSection.onProfilesChanged(autofillProfiles);
        }
        if (mShippingAddressSection != null) {
            mShippingAddressSection.onProfilesChanged(autofillProfiles);
        }
        if (mPaymentMethodSection != null) {
            // TODO(crbug.com/806868): It is currently not possible to clear the billing address
            // dropdown of |CardEditor|. As a workaround, we create a new instance here.
            AddressEditor addressEditor = new AddressEditor(AddressEditor.Purpose.PAYMENT_REQUEST,
                    /* saveToDisk= */ !webContents.isIncognito());
            CardEditor cardEditor = new CardEditor(webContents, addressEditor,
                    /* includeOrgLabel= */ false, /* observerForTest= */ null);
            for (Map.Entry<String, PaymentMethodData> entry : mPaymentMethodsData.entrySet()) {
                cardEditor.addAcceptedPaymentMethodIfRecognized(entry.getValue());
            }

            addressEditor.setEditorDialog(new EditorDialog(mActivity, null,
                    /*deleteRunnable =*/null));
            EditorDialog cardEditorDialog = new EditorDialog(mActivity, null,
                    /*deleteRunnable =*/null);
            if (ChromeVersionInfo.isBetaBuild() || ChromeVersionInfo.isStableBuild()) {
                cardEditorDialog.disableScreenshots();
            }
            cardEditor.setEditorDialog(cardEditorDialog);
            mPaymentMethodSection.setEditor(cardEditor);
            mPaymentMethodSection.onProfilesChanged(autofillProfiles);

            // TODO(crbug.com/806868): Change to asynchronous |getInstruments|.
            // TODO(crbug.com/806868): Sort payment methods, those matching autofillProfiles first.
            AutofillPaymentApp autofillPaymentApp = new AutofillPaymentApp(webContents);
            List<PaymentInstrument> paymentMethods = autofillPaymentApp.getInstruments(
                    mPaymentMethodsData, /*forceReturnServerCards=*/true);
            mPaymentMethodSection.onPaymentMethodsChanged(paymentMethods);
        }
        updateSectionPaddings();
    }
}
