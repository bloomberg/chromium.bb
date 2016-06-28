// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Handler;
import android.telephony.PhoneNumberUtils;
import android.text.TextUtils;
import android.util.Patterns;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.payments.ui.EditorFieldModel;
import org.chromium.chrome.browser.payments.ui.EditorFieldModel.EditorFieldValidator;
import org.chromium.chrome.browser.payments.ui.EditorModel;
import org.chromium.chrome.browser.payments.ui.LineItem;
import org.chromium.chrome.browser.payments.ui.PaymentInformation;
import org.chromium.chrome.browser.payments.ui.PaymentOption;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.browser.payments.ui.ShoppingCart;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.autofill.AutofillCreditCardEditor;
import org.chromium.chrome.browser.preferences.autofill.AutofillProfileEditor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.safejson.JsonSanitizer;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojom.payments.PaymentComplete;
import org.chromium.mojom.payments.PaymentDetails;
import org.chromium.mojom.payments.PaymentItem;
import org.chromium.mojom.payments.PaymentMethodData;
import org.chromium.mojom.payments.PaymentOptions;
import org.chromium.mojom.payments.PaymentRequest;
import org.chromium.mojom.payments.PaymentRequestClient;
import org.chromium.mojom.payments.PaymentResponse;
import org.chromium.mojom.payments.PaymentShippingOption;
import org.chromium.ui.base.WindowAndroid;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Android implementation of the PaymentRequest service defined in
 * third_party/WebKit/public/platform/modules/payments/payment_request.mojom.
 */
public class PaymentRequestImpl implements PaymentRequest, PaymentRequestUI.Client,
        PaymentApp.InstrumentsCallback, PaymentInstrument.DetailsCallback {

    /**
     * A test-only observer for the PaymentRequest service implementation.
     */
    public interface PaymentRequestServiceObserverForTest {
        /**
         * Called when an abort request was denied.
         */
        void onPaymentRequestServiceUnableToAbort();
    }

    /**
     * The size for the favicon in density-independent pixels.
     */
    private static final int FAVICON_SIZE_DP = 24;

    private static final String TAG = "cr_PaymentRequest";

    private static PaymentRequestServiceObserverForTest sObserverForTest;

    private final Handler mHandler = new Handler();

    private Activity mContext;
    private String mMerchantName;
    private String mOrigin;
    private Bitmap mFavicon;
    private List<PaymentApp> mApps;
    private PaymentRequestClient mClient;

    /**
     * The raw total amount being charged, as it was received from the website. This data is passed
     * to the payment app.
     */
    private PaymentItem mRawTotal;

    /**
     * The raw items in the shopping cart, as they were received from the website. This data is
     * passed to the payment app.
     */
    private List<PaymentItem> mRawLineItems;

    /**
     * The UI model of the shopping cart, including the total. Each item includes a label and a
     * price string. This data is passed to the UI.
     */
    private ShoppingCart mUiShoppingCart;

    /**
     * The raw shipping options, as they were received from the website. This data is compared to
     * updated payment options from the website to determine whether shipping options have changed
     * due to user selecting a shipping address.
     */
    private List<PaymentShippingOption> mRawShippingOptions;

    /**
     * The UI model for the shipping options. Includes the label and sublabel for each shipping
     * option. Also keeps track of the selected shipping option. This data is passed to the UI.
     */
    private SectionInformation mUiShippingOptions;

    private HashMap<String, JSONObject> mMethodData;
    private SectionInformation mShippingAddressesSection;
    private SectionInformation mContactSection;
    private List<PaymentApp> mPendingApps;
    private List<PaymentInstrument> mPendingInstruments;
    private SectionInformation mPaymentMethodsSection;
    private PaymentRequestUI mUI;
    private Callback<PaymentInformation> mPaymentInformationCallback;
    private Pattern mRegionCodePattern;
    private boolean mMerchantNeedsShippingAddress;
    private boolean mPaymentAppRunning;
    private boolean mRequestPayerPhone;
    private boolean mRequestPayerEmail;
    private List<CharSequence> mAllPhoneNumbers;
    private List<CharSequence> mAllEmailAddresses;
    private EditorFieldValidator mPhoneValidator;
    private EditorFieldValidator mEmailValidator;

    /**
     * Builds the PaymentRequest service implementation.
     *
     * @param webContents The web contents that have invoked the PaymentRequest API.
     */
    public PaymentRequestImpl(WebContents webContents) {
        if (webContents == null) return;

        ContentViewCore contentViewCore = ContentViewCore.fromWebContents(webContents);
        if (contentViewCore == null) return;

        WindowAndroid window = contentViewCore.getWindowAndroid();
        if (window == null) return;

        mContext = window.getActivity().get();
        if (mContext == null) return;

        mMerchantName = webContents.getTitle();
        // The feature is available only in secure context, so it's OK to not show HTTPS.
        mOrigin = UrlUtilities.formatUrlForSecurityDisplay(webContents.getVisibleUrl(), false);

        final FaviconHelper faviconHelper = new FaviconHelper();
        float scale = mContext.getResources().getDisplayMetrics().density;
        faviconHelper.getLocalFaviconImageForURL(Profile.getLastUsedProfile(),
                webContents.getVisibleUrl(), (int) (FAVICON_SIZE_DP * scale + 0.5f),
                new FaviconHelper.FaviconImageCallback() {
                    @Override
                    public void onFaviconAvailable(Bitmap bitmap, String iconUrl) {
                        faviconHelper.destroy();
                        if (bitmap == null) return;
                        if (mUI == null) {
                            mFavicon = bitmap;
                            return;
                        }
                        mUI.setTitleBitmap(bitmap);
                    }
                });

        mApps = PaymentAppFactory.create(webContents);
        mRegionCodePattern = Pattern.compile(AutofillAddress.REGION_CODE_PATTERN);
    }

    /**
     * Called by the renderer to provide an endpoint for callbacks.
     */
    @Override
    public void setClient(PaymentRequestClient client) {
        assert mClient == null;

        mClient = client;

        if (mClient == null) return;

        if (mContext == null) {
            disconnectFromClientWithDebugMessage("Web contents don't have associated activity");
        }
    }

    /**
     * Called by the merchant website to show the payment request to the user.
     */
    @Override
    public void show(PaymentMethodData[] methodData, PaymentDetails details,
            PaymentOptions options) {
        if (mClient == null) return;

        if (mMethodData != null) {
            disconnectFromClientWithDebugMessage("PaymentRequest.show() called more than once.");
            return;
        }

        mMethodData = getValidatedMethodData(methodData);
        if (mMethodData == null) {
            disconnectFromClientWithDebugMessage("Invalid payment methods or data");
            return;
        }

        if (!parseAndValidateDetailsOrDisconnectFromClient(details)) return;

        // If the merchant requests shipping and does not provide a selected shipping option, then
        // the merchant needs the shipping address to calculate the shipping price and availability.
        boolean requestShipping = options != null && options.requestShipping;
        mMerchantNeedsShippingAddress =
                requestShipping && mUiShippingOptions.getSelectedItem() == null;

        mRequestPayerPhone = options != null && options.requestPayerPhone;
        mRequestPayerEmail = options != null && options.requestPayerEmail;

        if (requestShipping || mRequestPayerPhone || mRequestPayerEmail) {
            List<AutofillProfile> profiles =
                    PersonalDataManager.getInstance().getProfilesToSuggest();
            List<AutofillContact> contacts = new ArrayList<>();
            List<AutofillAddress> addresses = new ArrayList<>();
            mAllPhoneNumbers = new ArrayList<>();
            mAllEmailAddresses = new ArrayList<>();
            int firstCompleteContactIndex = SectionInformation.NO_SELECTION;
            for (int i = 0; i < profiles.size(); i++) {
                AutofillProfile profile = profiles.get(i);

                String phone = mRequestPayerPhone && !TextUtils.isEmpty(profile.getPhoneNumber())
                        ? profile.getPhoneNumber() : null;
                String email = mRequestPayerEmail && !TextUtils.isEmpty(profile.getEmailAddress())
                        ? profile.getEmailAddress() : null;
                if (phone != null || email != null) {
                    boolean isComplete = isContactInformationComplete(phone, email);
                    contacts.add(new AutofillContact(profile, phone, email, isComplete));
                    if (isComplete && firstCompleteContactIndex < 0) firstCompleteContactIndex = i;
                    if (getPhoneValidator().isValid(phone)) mAllPhoneNumbers.add(phone);
                    if (getEmailValidator().isValid(email)) mAllEmailAddresses.add(email);
                }

                if (canUseAddress(profile, requestShipping)) {
                    addresses.add(new AutofillAddress(profile));
                }
            }

            if (requestShipping) {
                int selectedIndex = SectionInformation.NO_SELECTION;
                if (!addresses.isEmpty() && mUiShippingOptions.getSelectedItem() != null) {
                    selectedIndex = 0;
                }
                mShippingAddressesSection = new SectionInformation(
                        PaymentRequestUI.TYPE_SHIPPING_ADDRESSES, selectedIndex, addresses);
            }

            // The contact section automatically selects the first complete entry.
            if (mRequestPayerPhone || mRequestPayerEmail) {
                mContactSection = new SectionInformation(
                        PaymentRequestUI.TYPE_CONTACT_DETAILS, firstCompleteContactIndex, contacts);
            }
        }

        mPendingApps = new ArrayList<>(mApps);
        mPendingInstruments = new ArrayList<>();
        boolean isGettingInstruments = false;

        for (int i = 0; i < mApps.size(); i++) {
            PaymentApp app = mApps.get(i);
            Set<String> appMethods = app.getSupportedMethodNames();
            appMethods.retainAll(mMethodData.keySet());
            if (appMethods.isEmpty()) {
                mPendingApps.remove(app);
            } else {
                isGettingInstruments = true;
                app.getInstruments(mMethodData.get(appMethods.iterator().next()), this);
            }
        }

        if (!isGettingInstruments) {
            mPaymentMethodsSection = new SectionInformation(PaymentRequestUI.TYPE_PAYMENT_METHODS);
        }

        mUI = new PaymentRequestUI(mContext, this, requestShipping,
                mRequestPayerPhone || mRequestPayerEmail, mMerchantName, mOrigin);
        if (mFavicon != null) mUI.setTitleBitmap(mFavicon);
        mFavicon = null;
    }

    private static HashMap<String, JSONObject> getValidatedMethodData(
            PaymentMethodData[] methodData) {
        // Payment methodData are required.
        if (methodData == null || methodData.length == 0) return null;
        HashMap<String, JSONObject> result = new HashMap<>();
        for (int i = 0; i < methodData.length; i++) {
            JSONObject data = null;
            if (!TextUtils.isEmpty(methodData[i].stringifiedData)) {
                try {
                    data = new JSONObject(JsonSanitizer.sanitize(methodData[i].stringifiedData));
                } catch (JSONException | IOException | IllegalStateException e) {
                    // Payment method specific data should be a JSON object.
                    // According to the payment request spec[1], for each method data,
                    // if the data field is supplied but is not a JSON-serializable object,
                    // then should throw a TypeError. So, we should return null here even if
                    // only one is bad.
                    // [1] https://w3c.github.io/browser-payment-api/specs/paymentrequest.html
                    return null;
                }
            }

            String[] methods = methodData[i].supportedMethods;

            // Payment methods are required.
            if (methods == null || methods.length == 0) return null;

            for (int j = 0; j < methods.length; j++) {
                // Payment methods should be non-empty.
                if (TextUtils.isEmpty(methods[j])) return null;
                result.put(methods[j], data);
            }
        }
        return result;
    }

    private boolean isContactInformationComplete(String phone, String email) {
        return (!mRequestPayerPhone || getPhoneValidator().isValid(phone))
                && (!mRequestPayerEmail || getEmailValidator().isValid(email));
    }

    private boolean canUseAddress(AutofillProfile profile, boolean requestShipping) {
        return requestShipping && profile.getCountryCode() != null
                && mRegionCodePattern.matcher(profile.getCountryCode()).matches()
                && profile.getStreetAddress() != null && profile.getRegion() != null
                && profile.getLocality() != null && profile.getDependentLocality() != null
                && profile.getPostalCode() != null && profile.getSortingCode() != null
                && profile.getCompanyName() != null && profile.getFullName() != null
                && profile.getPhoneNumber() != null;
    }

    /**
     * Called by merchant to update the shipping options and line items after the user has selected
     * their shipping address or shipping option.
     */
    @Override
    public void updateWith(PaymentDetails details) {
        if (mClient == null) return;

        if (mUI == null) {
            disconnectFromClientWithDebugMessage(
                    "PaymentRequestUpdateEvent.updateWith() called without PaymentRequest.show()");
            return;
        }

        if (!parseAndValidateDetailsOrDisconnectFromClient(details)) return;

        // Empty shipping options means the merchant cannot ship to the user's selected shipping
        // address.
        if (mUiShippingOptions.isEmpty() && !mMerchantNeedsShippingAddress) {
            disconnectFromClientWithDebugMessage("Merchant indicates inability to ship although "
                    + "originally indicated that can ship anywhere");
            return;
        }

        if (mUiShippingOptions.isEmpty() && mShippingAddressesSection.getSelectedItem() != null) {
            mShippingAddressesSection.getSelectedItem().setInvalid();
            mShippingAddressesSection.setSelectedItemIndex(SectionInformation.INVALID_SELECTION);
        }

        if (mPaymentInformationCallback != null) {
            providePaymentInformation();
        } else {
            mUI.updateOrderSummarySection(mUiShoppingCart);
            mUI.updateSection(PaymentRequestUI.TYPE_SHIPPING_OPTIONS, mUiShippingOptions);
        }
    }

    /**
     * Sets the total, display line items, and shipping options based on input and returns the
     * status boolean. That status is true for valid data, false for invalid data. If the input is
     * invalid, disconnects from the client. Both raw and UI versions of data are updated.
     *
     * @param details The total, line items, and shipping options to parse, validate, and save in
     *                member variables.
     * @return True if the data is valid. False if the data is invalid.
     */
    private boolean parseAndValidateDetailsOrDisconnectFromClient(PaymentDetails details) {
        if (details == null) {
            disconnectFromClientWithDebugMessage("Payment details required");
            return false;
        }

        if (!hasAllPaymentItemFields(details.total)) {
            disconnectFromClientWithDebugMessage("Invalid total");
            return false;
        }

        String totalCurrency = details.total.amount.currency;
        CurrencyStringFormatter formatter =
                new CurrencyStringFormatter(totalCurrency, Locale.getDefault());

        if (!formatter.isValidAmountCurrencyCode(details.total.amount.currency)) {
            disconnectFromClientWithDebugMessage("Invalid total amount currency");
            return false;
        }

        if (!formatter.isValidAmountValue(details.total.amount.value)
                || details.total.amount.value.startsWith("-")) {
            disconnectFromClientWithDebugMessage("Invalid total amount value");
            return false;
        }

        LineItem uiTotal = new LineItem(
                details.total.label, totalCurrency, formatter.format(details.total.amount.value));

        List<LineItem> uiLineItems = getValidatedLineItems(details.displayItems, totalCurrency,
                formatter);
        if (uiLineItems == null) {
            disconnectFromClientWithDebugMessage("Invalid line items");
            return false;
        }

        mUiShoppingCart = new ShoppingCart(uiTotal, uiLineItems);
        mRawTotal = details.total;
        mRawLineItems = Arrays.asList(details.displayItems);

        mUiShippingOptions = getValidatedShippingOptions(details.shippingOptions, totalCurrency,
                formatter);
        if (mUiShippingOptions == null) {
            disconnectFromClientWithDebugMessage("Invalid shipping options");
            return false;
        }

        mRawShippingOptions = Arrays.asList(details.shippingOptions);

        return true;
    }

    /**
     * Returns true if all fields in the payment item are non-null and non-empty.
     *
     * @param item The payment item to examine.
     * @return True if all fields are present and non-empty.
     */
    private static boolean hasAllPaymentItemFields(PaymentItem item) {
        // "label", "currency", and "value" should be non-empty.
        return item != null && !TextUtils.isEmpty(item.label) && item.amount != null
                && !TextUtils.isEmpty(item.amount.currency)
                && !TextUtils.isEmpty(item.amount.value);
    }

    /**
     * Validates a list of payment items and returns their parsed representation or null if invalid.
     *
     * @param items The payment items to parse and validate.
     * @param totalCurrency The currency code for the total amount of payment.
     * @param formatter A formatter and validator for the currency amount value.
     * @return A list of valid line items or null if invalid.
     */
    private static List<LineItem> getValidatedLineItems(
            PaymentItem[] items, String totalCurrency, CurrencyStringFormatter formatter) {
        // Line items are optional.
        if (items == null) return new ArrayList<LineItem>();

        List<LineItem> result = new ArrayList<>(items.length);
        for (int i = 0; i < items.length; i++) {
            PaymentItem item = items[i];

            if (!hasAllPaymentItemFields(item)) return null;

            // All currencies must match.
            if (!item.amount.currency.equals(totalCurrency)) return null;

            // Value should be in correct format.
            if (!formatter.isValidAmountValue(item.amount.value)) return null;

            result.add(new LineItem(item.label, "", formatter.format(item.amount.value)));
        }

        return result;
    }

    /**
     * Validates a list of shipping options and returns their parsed representation or null if
     * invalid.
     *
     * @param options The raw shipping options to parse and validate.
     * @param totalCurrency The currency code for the total amount of payment.
     * @param formatter A formatter and validator for the currency amount value.
     * @return The UI representation of the shipping options or null if invalid.
     */
    private static SectionInformation getValidatedShippingOptions(PaymentShippingOption[] options,
            String totalCurrency, CurrencyStringFormatter formatter) {
        // Shipping options are optional.
        if (options == null || options.length == 0) {
            return new SectionInformation(PaymentRequestUI.TYPE_SHIPPING_OPTIONS);
        }

        for (int i = 0; i < options.length; i++) {
            PaymentShippingOption option = options[i];

            // Each "id", "label", "currency", and "value" should be non-empty.
            // Each "value" should be a valid amount value.
            // Each "currency" should match the total currency.
            if (option == null || TextUtils.isEmpty(option.id) || TextUtils.isEmpty(option.label)
                    || option.amount == null || TextUtils.isEmpty(option.amount.currency)
                    || TextUtils.isEmpty(option.amount.value)
                    || !totalCurrency.equals(option.amount.currency)
                    || !formatter.isValidAmountValue(option.amount.value)) {
                return null;
            }
        }

        List<PaymentOption> result = new ArrayList<>();
        int selectedItemIndex = SectionInformation.NO_SELECTION;
        for (int i = 0; i < options.length; i++) {
            PaymentShippingOption option = options[i];
            result.add(new PaymentOption(option.id, option.label,
                    formatter.format(option.amount.value), PaymentOption.NO_ICON));
            if (option.selected) selectedItemIndex = i;
        }

        return new SectionInformation(PaymentRequestUI.TYPE_SHIPPING_OPTIONS, selectedItemIndex,
                result);
    }

    /**
     * Called to retrieve the data to show in the initial PaymentRequest UI.
     */
    @Override
    public void getDefaultPaymentInformation(Callback<PaymentInformation> callback) {
        mPaymentInformationCallback = callback;

        if (mPaymentMethodsSection == null) return;

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                providePaymentInformation();
            }
        });
    }

    private void providePaymentInformation() {
        mPaymentInformationCallback.onResult(
                new PaymentInformation(mUiShoppingCart, mShippingAddressesSection,
                        mUiShippingOptions, mContactSection, mPaymentMethodsSection));
        mPaymentInformationCallback = null;
    }

    @Override
    public void getShoppingCart(final Callback<ShoppingCart> callback) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                callback.onResult(mUiShoppingCart);
            }
        });
    }

    @Override
    public void getSectionInformation(@PaymentRequestUI.DataType final int optionType,
            final Callback<SectionInformation> callback) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (optionType == PaymentRequestUI.TYPE_SHIPPING_ADDRESSES) {
                    callback.onResult(mShippingAddressesSection);
                } else if (optionType == PaymentRequestUI.TYPE_SHIPPING_OPTIONS) {
                    callback.onResult(mUiShippingOptions);
                } else if (optionType == PaymentRequestUI.TYPE_CONTACT_DETAILS) {
                    callback.onResult(mContactSection);
                } else if (optionType == PaymentRequestUI.TYPE_PAYMENT_METHODS) {
                    assert mPaymentMethodsSection != null;
                    callback.onResult(mPaymentMethodsSection);
                }
            }
        });
    }

    @Override
    public boolean onSectionOptionSelected(@PaymentRequestUI.DataType int optionType,
            PaymentOption option, Callback<PaymentInformation> callback) {
        if (optionType == PaymentRequestUI.TYPE_SHIPPING_ADDRESSES) {
            assert option instanceof AutofillAddress;
            mShippingAddressesSection.setSelectedItem(option);
            if (mMerchantNeedsShippingAddress) {
                mPaymentInformationCallback = callback;
                // This updates the line items and the shipping options asynchronously.
                mClient.onShippingAddressChange(((AutofillAddress) option).toPaymentAddress());
                return true;
            }
        } else if (optionType == PaymentRequestUI.TYPE_SHIPPING_OPTIONS) {
            // This may update the line items.
            mUiShippingOptions.setSelectedItem(option);
            mClient.onShippingOptionChange(option.getIdentifier());
        } else if (optionType == PaymentRequestUI.TYPE_CONTACT_DETAILS) {
            assert option instanceof AutofillContact;
            AutofillContact contact = (AutofillContact) option;
            if (contact.isComplete()) {
                mContactSection.setSelectedItem(option);
            } else {
                editContact(contact);
            }
        } else if (optionType == PaymentRequestUI.TYPE_PAYMENT_METHODS) {
            assert option instanceof PaymentInstrument;
            mPaymentMethodsSection.setSelectedItem(option);
        }
        return false;
    }

    private void editContact(final AutofillContact contact) {
        final EditorFieldModel phoneField = mRequestPayerPhone
                ? new EditorFieldModel(EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                          mContext.getString(R.string.autofill_profile_editor_phone_number),
                          mAllPhoneNumbers, getPhoneValidator(),
                          mContext.getString(R.string.payments_phone_required_validation_message),
                          mContext.getString(R.string.payments_phone_invalid_validation_message),
                          contact == null ? null : contact.getPayerPhone())
                : null;

        final EditorFieldModel emailField = mRequestPayerEmail
                ? new EditorFieldModel(EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
                          mContext.getString(R.string.autofill_profile_editor_email_address),
                          mAllEmailAddresses, getEmailValidator(),
                          mContext.getString(R.string.payments_email_required_validation_message),
                          mContext.getString(R.string.payments_email_invalid_validation_message),
                          contact == null ? null : contact.getPayerEmail())
                : null;

        EditorModel editor =
                new EditorModel(mContext.getString(R.string.payments_add_contact_details_label));
        if (phoneField != null) editor.addField(phoneField);
        if (emailField != null) editor.addField(emailField);

        editor.setCancelCallback(new Runnable() {
            @Override
            public void run() {
                mContactSection.setSelectedItemIndex(SectionInformation.NO_SELECTION);
                mUI.updateSection(PaymentRequestUI.TYPE_CONTACT_DETAILS, mContactSection);
            }
        });

        editor.setDoneCallback(new Runnable() {
            @Override
            public void run() {
                AutofillProfile profile =
                        contact != null ? contact.getProfile() : new AutofillProfile();
                String phone = null;
                String email = null;
                if (phoneField != null) {
                    phone = phoneField.getValue().toString();
                    profile.setPhoneNumber(phone);
                }
                if (emailField != null) {
                    email = emailField.getValue().toString();
                    profile.setEmailAddress(email);
                }
                PersonalDataManager.getInstance().setProfile(profile);

                if (contact == null) {
                    mContactSection.addAndSelectItem(
                            new AutofillContact(profile, phone, email, true));
                } else {
                    contact.completeContact(phone, email);
                }
                mUI.updateSection(PaymentRequestUI.TYPE_CONTACT_DETAILS, mContactSection);
            }
        });
        mUI.showEditor(editor);
    }

    private EditorFieldValidator getPhoneValidator() {
        if (mPhoneValidator == null) {
            mPhoneValidator = new EditorFieldValidator() {
                @Override
                public boolean isValid(CharSequence value) {
                    return value != null
                            && PhoneNumberUtils.isGlobalPhoneNumber(
                                       PhoneNumberUtils.stripSeparators(value.toString()));
                }
            };
        }
        return mPhoneValidator;
    }

    private EditorFieldValidator getEmailValidator() {
        if (mEmailValidator == null) {
            mEmailValidator = new EditorFieldValidator() {
                @Override
                public boolean isValid(CharSequence value) {
                    return value != null && Patterns.EMAIL_ADDRESS.matcher(value).matches();
                }
            };
        }
        return mEmailValidator;
    }

    @Override
    public void onSectionAddOption(@PaymentRequestUI.DataType int optionType) {
        if (optionType == PaymentRequestUI.TYPE_SHIPPING_ADDRESSES) {
            PreferencesLauncher.launchSettingsPage(mContext, AutofillProfileEditor.class.getName());
        } else if (optionType == PaymentRequestUI.TYPE_CONTACT_DETAILS) {
            editContact(null);
        } else if (optionType == PaymentRequestUI.TYPE_PAYMENT_METHODS) {
            PreferencesLauncher.launchSettingsPage(
                    mContext, AutofillCreditCardEditor.class.getName());
        }
    }

    @Override
    public void onPayClicked(PaymentOption selectedShippingAddress,
            PaymentOption selectedShippingOption, PaymentOption selectedPaymentMethod) {
        assert selectedPaymentMethod instanceof PaymentInstrument;
        PaymentInstrument instrument = (PaymentInstrument) selectedPaymentMethod;
        mPaymentAppRunning = true;
        instrument.getDetails(mMerchantName, mOrigin, mRawTotal, mRawLineItems,
                mMethodData.get(instrument.getMethodName()), this);
    }

    @Override
    public void onDismiss() {
        disconnectFromClientWithDebugMessage("Dialog dismissed");
        closeUI(false);
    }

    @Override
    public boolean merchantNeedsShippingAddress() {
        return mMerchantNeedsShippingAddress;
    }

    /**
     * Called by the merchant website to abort the payment.
     */
    @Override
    public void abort() {
        mClient.onAbort(!mPaymentAppRunning);
        if (mPaymentAppRunning) {
            if (sObserverForTest != null) sObserverForTest.onPaymentRequestServiceUnableToAbort();
        } else {
            closeClient();
            closeUI(false);
        }
    }

    /**
     * Called when the merchant website has processed the payment.
     */
    @Override
    public void complete(int result) {
        closeUI(PaymentComplete.FAIL != result);
    }

    /**
     * Called when the renderer closes the Mojo connection.
     */
    @Override
    public void close() {
        closeClient();
        closeUI(false);
    }

    /**
     * Called when the Mojo connection encounters an error.
     */
    @Override
    public void onConnectionError(MojoException e) {
        closeClient();
        closeUI(false);
    }

    /**
     * Called after retrieving the list of payment instruments in an app.
     */
    @Override
    public void onInstrumentsReady(PaymentApp app, List<PaymentInstrument> instruments) {
        mPendingApps.remove(app);

        if (instruments != null) {
            for (int i = 0; i < instruments.size(); i++) {
                PaymentInstrument instrument = instruments.get(i);
                if (mMethodData.containsKey(instrument.getMethodName())) {
                    mPendingInstruments.add(instrument);
                } else {
                    instrument.dismiss();
                }
            }
        }

        if (mPendingApps.isEmpty()) {
            mPaymentMethodsSection = new SectionInformation(
                    PaymentRequestUI.TYPE_PAYMENT_METHODS, 0, mPendingInstruments);
            mPendingInstruments.clear();

            if (mPaymentInformationCallback != null) providePaymentInformation();
        }
    }

    /**
     * Called after retrieving instrument details.
     */
    @Override
    public void onInstrumentDetailsReady(String methodName, String stringifiedDetails) {
        PaymentResponse response = new PaymentResponse();
        response.methodName = methodName;
        response.stringifiedDetails = stringifiedDetails;

        if (mContactSection != null) {
            PaymentOption selectedContact = mContactSection.getSelectedItem();
            if (selectedContact != null) {
                // Contacts are created in show(). These should all be instances of AutofillContact.
                assert selectedContact instanceof AutofillContact;
                response.payerPhone = ((AutofillContact) selectedContact).getPayerPhone();
                response.payerEmail = ((AutofillContact) selectedContact).getPayerEmail();
            }
        }

        if (mShippingAddressesSection != null) {
            PaymentOption selectedShippingAddress = mShippingAddressesSection.getSelectedItem();
            if (selectedShippingAddress != null) {
                // Shipping addresses are created in show(). These should all be instances of
                // AutofillAddress.
                assert selectedShippingAddress instanceof AutofillAddress;
                response.shippingAddress =
                        ((AutofillAddress) selectedShippingAddress).toPaymentAddress();
            }
        }

        if (mUiShippingOptions != null) {
            PaymentOption selectedShippingOption = mUiShippingOptions.getSelectedItem();
            if (selectedShippingOption != null && selectedShippingOption.getIdentifier() != null) {
                response.shippingOption = selectedShippingOption.getIdentifier();
            }
        }

        mClient.onPaymentResponse(response);
    }

    /**
     * Called if unable to retrieve instrument details.
     */
    @Override
    public void onInstrumentDetailsError() {
        disconnectFromClientWithDebugMessage("Failed to retrieve payment instrument details");
        closeUI(false);
    }

    private void disconnectFromClientWithDebugMessage(String debugMessage) {
        Log.d(TAG, debugMessage);
        mClient.onError();
        closeClient();
    }

    /**
     * Closes the UI. If the client is still connected, then it's notified of UI hiding.
     */
    private void closeUI(boolean immediateClose) {
        if (mUI != null) {
            mUI.close(immediateClose, new Runnable() {
                @Override
                public void run() {
                    if (mClient != null) mClient.onComplete();
                    closeClient();
                }
            });
            mUI = null;
        }

        if (mPaymentMethodsSection != null) {
            for (int i = 0; i < mPaymentMethodsSection.getSize(); i++) {
                PaymentOption option = mPaymentMethodsSection.getItem(i);
                assert option instanceof PaymentInstrument;
                ((PaymentInstrument) option).dismiss();
            }
            mPaymentMethodsSection = null;
        }
    }

    private void closeClient() {
        if (mClient != null) mClient.close();
        mClient = null;
    }

    @VisibleForTesting
    public static void setObserverForTest(PaymentRequestServiceObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }
}
