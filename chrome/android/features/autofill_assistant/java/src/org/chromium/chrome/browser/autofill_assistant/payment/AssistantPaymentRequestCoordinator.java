// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.app.Activity;
import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v4.util.ArrayMap;
import android.text.TextUtils;
import android.view.View;
import android.widget.ScrollView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentApp;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.payments.BasicCardUtils;
import org.chromium.chrome.browser.payments.CardEditor;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.payments.ShippingStrings;
import org.chromium.chrome.browser.payments.ui.ContactDetailsSection;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.DataType;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.SelectionResult;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.browser.widget.prefeditor.Completable;
import org.chromium.chrome.browser.widget.prefeditor.EditableOption;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentShippingType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

// TODO(crbug.com/806868): Use mCarouselCoordinator to show chips.
// TODO(crbug.com/806868): Refact each payment request section into a MVC component.

/**
 * Coordinator for the Payment Request.
 */
public class AssistantPaymentRequestCoordinator implements AssistantPaymentRequestUI.Client {
    private static final Comparator<Completable> COMPLETENESS_COMPARATOR =
            (a, b) -> (b.isComplete() ? 1 : 0) - (a.isComplete() ? 1 : 0);

    private final Context mContext;
    private final AssistantPaymentRequestModel mModel;
    private final AssistantPaymentRequestUI mPaymentRequestUI;

    private CardEditor mCardEditor;
    private AddressEditor mAddressEditor;
    private ContactEditor mContactEditor;
    private SectionInformation mPaymentMethodsSection;
    private SectionInformation mShippingAddressesSection;
    private ContactDetailsSection mContactSection;

    private boolean mVisible;
    private boolean mForceInvisible;

    public AssistantPaymentRequestCoordinator(Context context, AssistantPaymentRequestModel model) {
        mContext = context;
        mModel = model;

        // TODO(crbug.com/806868): Pass in activity explicitly.
        mPaymentRequestUI = new AssistantPaymentRequestUI((Activity) context, this);

        // Payment request is initially hidden.
        setVisible(false);

        // Listen for model changes.
        model.addObserver((source, propertyKey)
                                  -> resetPaymentRequestUI(
                                          model.get(AssistantPaymentRequestModel.WEB_CONTENTS),
                                          model.get(AssistantPaymentRequestModel.OPTIONS),
                                          model.get(AssistantPaymentRequestModel.DELEGATE)));
    }

    public ScrollView getView() {
        return mPaymentRequestUI.getView();
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

    private void resetPaymentRequestUI(@Nullable WebContents webContents,
            @Nullable AssistantPaymentRequestOptions options,
            @Nullable AssistantPaymentRequestDelegate delegate) {
        mPaymentRequestUI.close();
        if (options == null || webContents == null || delegate == null) {
            setVisible(false);
            return;
        }

        buildPaymentRequestUI(webContents, options);
        mPaymentRequestUI.show(UrlFormatter.formatUrlForSecurityDisplayOmitScheme(
                                       webContents.getLastCommittedUrl()),
                options.mRequestShipping, options.mRequestPaymentMethod,
                options.mRequestPayerName || options.mRequestPayerPhone
                        || options.mRequestPayerEmail,
                new ShippingStrings(PaymentShippingType.SHIPPING));
        setVisible(true);

        // Notify initial selections.
        if (mShippingAddressesSection != null) notifySelectedShippingAddressChanged();
        if (mContactSection != null) notifySelectedContactInfoChanged();
        notifyCreditCardChanged();
    }

    private void buildPaymentRequestUI(
            WebContents webContents, AssistantPaymentRequestOptions options) {
        assert !webContents.isIncognito();
        mAddressEditor = new AddressEditor(AddressEditor.Purpose.AUTOFILL_ASSISTANT,
                /* saveToDisk= */ !webContents.isIncognito());
        mCardEditor = new CardEditor(webContents, mAddressEditor, /* includeOrgLabel= */
                ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_COMPANY_NAME),
                /* observerForTest= */ null);

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

        buildPaymentRequestUISectionInformation(webContents, options, methodData);

        mAddressEditor.setEditorDialog(mPaymentRequestUI.getEditorDialog());
        mCardEditor.addAcceptedPaymentMethodIfRecognized(methodData);
        mCardEditor.setEditorDialog(mPaymentRequestUI.getCardEditorDialog());
        if (mContactEditor != null)
            mContactEditor.setEditorDialog(mPaymentRequestUI.getEditorDialog());
    }

    private void buildPaymentRequestUISectionInformation(WebContents webContents,
            AssistantPaymentRequestOptions options, PaymentMethodData methodData) {
        Map<String, PaymentMethodData> methodsData = new ArrayMap<>();
        methodsData.put(BasicCardUtils.BASIC_CARD_METHOD_NAME, methodData);
        mPaymentMethodsSection =
                new SectionInformation(DataType.PAYMENT_METHODS, SectionInformation.NO_SELECTION,
                        (new AutofillPaymentApp(webContents))
                                .getInstruments(methodsData, /*forceReturnServerCards=*/true));
        if (!mPaymentMethodsSection.isEmpty() && mPaymentMethodsSection.getItem(0).isComplete()) {
            mPaymentMethodsSection.setSelectedItemIndex(0);
        }

        List<AutofillProfile> profiles = null;
        if (options.mRequestShipping || options.mRequestPayerName || options.mRequestPayerPhone
                || options.mRequestPayerEmail) {
            profiles = PersonalDataManager.getInstance().getProfilesToSuggest(
                    /* includeNameInLabel= */ false);
            String defaultEmail = options.mDefaultEmail;
            if (defaultEmail != null && profiles != null) {
                // The profile with default email should be shown as first. Following profiles are
                // sorted in an alphabetic order.
                Collections.sort(profiles, (a, b) -> {
                    int compareResult = ApiCompatibilityUtils.compareBoolean(
                            defaultEmail.equals(b.getEmailAddress()),
                            defaultEmail.equals(a.getEmailAddress()));
                    if (compareResult != 0) return compareResult;
                    return b.getEmailAddress().compareTo(a.getEmailAddress());
                });
            }
        }

        if (options.mRequestShipping) {
            createShippingSection(mContext, Collections.unmodifiableList(profiles));
        }

        if (options.mRequestPayerName || options.mRequestPayerPhone || options.mRequestPayerEmail) {
            mContactEditor =
                    new ContactEditor(options.mRequestPayerName, options.mRequestPayerPhone,
                            options.mRequestPayerEmail, !webContents.isIncognito());
            mContactSection = new ContactDetailsSection(mContext,
                    Collections.unmodifiableList(profiles), mContactEditor,
                    /* journeyLogger= */ null);
        }
    }

    private void createShippingSection(
            Context context, List<AutofillProfile> unmodifiableProfiles) {
        List<AutofillAddress> addresses = new ArrayList<>();

        for (int i = 0; i < unmodifiableProfiles.size(); i++) {
            AutofillProfile profile = unmodifiableProfiles.get(i);
            mAddressEditor.addPhoneNumberIfValid(profile.getPhoneNumber());

            // Only suggest addresses that have a street address.
            if (!TextUtils.isEmpty(profile.getStreetAddress())) {
                addresses.add(new AutofillAddress(context, profile));
            }
        }

        // Suggest complete addresses first.
        Collections.sort(addresses, COMPLETENESS_COMPARATOR);

        // Automatically select the first address if one is complete.
        int firstCompleteAddressIndex = SectionInformation.NO_SELECTION;
        if (!addresses.isEmpty() && addresses.get(0).isComplete()) {
            firstCompleteAddressIndex = 0;

            // The initial label for the selected shipping address should not include the
            // country.
            addresses.get(firstCompleteAddressIndex).setShippingAddressLabelWithoutCountry();
        }

        mShippingAddressesSection = new SectionInformation(
                DataType.SHIPPING_ADDRESSES, firstCompleteAddressIndex, addresses);
    }

    @Override
    @Nullable
    public SectionInformation getSectionInformation(@DataType int optionType) {
        if (optionType == DataType.SHIPPING_ADDRESSES) {
            return mShippingAddressesSection;
        }

        if (optionType == DataType.CONTACT_DETAILS) {
            return mContactSection;
        }

        if (optionType == DataType.PAYMENT_METHODS) {
            return mPaymentMethodsSection;
        }

        // Only support the above sections.
        assert false : "Unsupported section type.";
        return null;
    }

    @Override
    @SelectionResult
    public int onSectionOptionSelected(@DataType int optionType, EditableOption option) {
        if (optionType == DataType.SHIPPING_ADDRESSES) {
            AutofillAddress address = (AutofillAddress) option;
            if (address.isComplete()) {
                mShippingAddressesSection.setSelectedItem(option);
                notifySelectedShippingAddressChanged();
            } else {
                editAddress(address);
                return SelectionResult.EDITOR_LAUNCH;
            }
        } else if (optionType == DataType.CONTACT_DETAILS) {
            AutofillContact contact = (AutofillContact) option;
            if (contact.isComplete()) {
                mContactSection.setSelectedItem(option);
                notifySelectedContactInfoChanged();
            } else {
                editContact(contact);
                return SelectionResult.EDITOR_LAUNCH;
            }
        } else if (optionType == DataType.PAYMENT_METHODS) {
            AutofillPaymentInstrument card = (AutofillPaymentInstrument) option;
            if (card.isComplete()) {
                mPaymentMethodsSection.setSelectedItem(option);
                notifyCreditCardChanged();
            } else {
                editCard(card);
                return SelectionResult.EDITOR_LAUNCH;
            }
        } else {
            // Only support the above sections.
            assert false : "Unsupported section type.";
        }

        return SelectionResult.NONE;
    }

    @Override
    @SelectionResult
    public int onSectionEditOption(@DataType int optionType, EditableOption option) {
        if (optionType == DataType.SHIPPING_ADDRESSES) {
            editAddress((AutofillAddress) option);
            return SelectionResult.EDITOR_LAUNCH;
        }

        if (optionType == DataType.CONTACT_DETAILS) {
            editContact((AutofillContact) option);
            return SelectionResult.EDITOR_LAUNCH;
        }

        if (optionType == DataType.PAYMENT_METHODS) {
            editCard((AutofillPaymentInstrument) option);
            return SelectionResult.EDITOR_LAUNCH;
        }

        // Only support the above sections.
        assert false : "Unsupported section type.";
        return SelectionResult.NONE;
    }

    @Override
    @SelectionResult
    public int onSectionAddOption(@DataType int optionType) {
        if (optionType == DataType.SHIPPING_ADDRESSES) {
            editAddress(null);
            return SelectionResult.EDITOR_LAUNCH;
        }

        if (optionType == DataType.CONTACT_DETAILS) {
            editContact(null);
            return SelectionResult.EDITOR_LAUNCH;
        }

        if (optionType == DataType.PAYMENT_METHODS) {
            editCard(null);
            return SelectionResult.EDITOR_LAUNCH;
        }

        // Only support the above sections.
        assert false : "Unsupported section type.";
        return SelectionResult.NONE;
    }

    private void editAddress(final AutofillAddress toEdit) {
        mAddressEditor.edit(toEdit, new Callback<AutofillAddress>() {
            @Override
            public void onResult(AutofillAddress editedAddress) {
                if (mPaymentRequestUI.isClosed()) return;

                if (editedAddress != null) {
                    // Sets or updates the shipping address label.
                    editedAddress.setShippingAddressLabelWithCountry();

                    mCardEditor.updateBillingAddressIfComplete(editedAddress);

                    // A partial or complete address came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedAddress.isComplete()) {
                        // If the address is not complete, deselect it (editor can return incomplete
                        // information when cancelled).
                        mShippingAddressesSection.setSelectedItemIndex(
                                SectionInformation.NO_SELECTION);
                    } else {
                        if (toEdit == null) {
                            // Address is complete and user was in the "Add flow": add an item to
                            // the list.
                            mShippingAddressesSection.addAndSelectItem(editedAddress);
                        }

                        if (mContactSection != null) {
                            // Update |mContactSection| with the new/edited address, which will
                            // update an existing item or add a new one to the end of the list.
                            mContactSection.addOrUpdateWithAutofillAddress(editedAddress);
                            notifySelectedContactInfoChanged();
                            mPaymentRequestUI.updateSection(
                                    DataType.CONTACT_DETAILS, mContactSection);
                        }
                    }
                }

                notifySelectedShippingAddressChanged();
                mPaymentRequestUI.updateSection(
                        DataType.SHIPPING_ADDRESSES, mShippingAddressesSection);
            }
        });
    }

    private void editContact(final AutofillContact toEdit) {
        mContactEditor.edit(toEdit, new Callback<AutofillContact>() {
            @Override
            public void onResult(AutofillContact editedContact) {
                if (mPaymentRequestUI.isClosed()) return;

                if (editedContact != null) {
                    // A partial or complete contact came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedContact.isComplete()) {
                        // If the contact is not complete according to the requirements of the flow,
                        // deselect it (editor can return incomplete information when cancelled).
                        mContactSection.setSelectedItemIndex(SectionInformation.NO_SELECTION);
                    } else if (toEdit == null) {
                        // Contact is complete and we were in the "Add flow": add an item to the
                        // list.
                        mContactSection.addAndSelectItem(editedContact);
                    }
                    // If contact is complete and (toEdit != null), no action needed: the contact
                    // was already selected in the UI.
                }
                // If |editedContact| is null, the user has cancelled out of the "Add flow". No
                // action to take (if a contact was selected in the UI, it will stay selected).

                notifySelectedContactInfoChanged();
                mPaymentRequestUI.updateSection(DataType.CONTACT_DETAILS, mContactSection);
            }
        });
    }

    private void editCard(final AutofillPaymentInstrument toEdit) {
        mCardEditor.edit(toEdit, new Callback<AutofillPaymentInstrument>() {
            @Override
            public void onResult(AutofillPaymentInstrument editedCard) {
                if (mPaymentRequestUI.isClosed()) return;

                if (editedCard != null) {
                    // A partial or complete card came back from the editor (could have been from
                    // adding/editing or cancelling out of the edit flow).
                    if (!editedCard.isComplete()) {
                        // If the card is not complete, deselect it (editor can return incomplete
                        // information when cancelled).
                        mPaymentMethodsSection.setSelectedItemIndex(
                                SectionInformation.NO_SELECTION);
                    } else if (toEdit == null) {
                        // Card is complete and we were in the "Add flow": add an item to the list.
                        mPaymentMethodsSection.addAndSelectItem(editedCard);
                    }
                    // If card is complete and (toEdit != null), no action needed: the card was
                    // already selected in the UI.
                }
                // If |editedCard| is null, the user has cancelled out of the "Add flow". No action
                // to take (if another card was selected prior to the add flow, it will stay
                // selected).

                notifyCreditCardChanged();
                mPaymentRequestUI.updateSection(DataType.PAYMENT_METHODS, mPaymentMethodsSection);
            }
        });
    }

    private void notifySelectedShippingAddressChanged() {
        AssistantPaymentRequestDelegate delegate =
                mModel.get(AssistantPaymentRequestModel.DELEGATE);
        assert delegate != null;

        AutofillAddress address = (AutofillAddress) mShippingAddressesSection.getSelectedItem();
        delegate.onShippingAddressChanged(address == null ? null : address.getProfile());
    }

    private void notifySelectedContactInfoChanged() {
        AssistantPaymentRequestDelegate delegate =
                mModel.get(AssistantPaymentRequestModel.DELEGATE);
        assert delegate != null;

        delegate.onContactInfoChanged((AutofillContact) mContactSection.getSelectedItem());
    }

    private void notifyCreditCardChanged() {
        AssistantPaymentRequestDelegate delegate =
                mModel.get(AssistantPaymentRequestModel.DELEGATE);
        assert delegate != null;

        AutofillPaymentInstrument cardInstrument =
                (AutofillPaymentInstrument) mPaymentMethodsSection.getSelectedItem();
        delegate.onCreditCardChanged(cardInstrument == null ? null : cardInstrument.getCard());
    }

    @Override
    public void onCheckAcceptTermsAndConditions(boolean checked) {
        assert checked;

        AssistantPaymentRequestDelegate delegate =
                mModel.get(AssistantPaymentRequestModel.DELEGATE);
        assert delegate != null;
        delegate.onTermsAndConditionsChanged(AssistantTermsAndConditionsState.ACCEPTED);
    }

    @Override
    public void onCheckReviewTermsAndConditions(boolean checked) {
        assert checked;

        AssistantPaymentRequestDelegate delegate =
                mModel.get(AssistantPaymentRequestModel.DELEGATE);
        assert delegate != null;
        delegate.onTermsAndConditionsChanged(AssistantTermsAndConditionsState.REQUIRES_REVIEW);
    }
}
