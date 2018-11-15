// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.accounts.Account;
import android.content.Context;
import android.graphics.RectF;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.telephony.TelephonyManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.List;
import java.util.Map;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantUiController implements AutofillAssistantUiDelegate.Client {
    /** Prefix for Intent extras relevant to this feature. */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /** OAuth2 scope that RPCs require. */
    private static final String AUTH_TOKEN_TYPE =
            "oauth2:https://www.googleapis.com/auth/userinfo.profile";

    private final WebContents mWebContents;
    private final String mInitialUrl;

    // TODO(crbug.com/806868): Move mCurrentDetails and mStatusMessage to a Model (refactor to MVC).
    private AutofillAssistantUiDelegate.Details mCurrentDetails =
            AutofillAssistantUiDelegate.Details.getEmptyDetails();
    private String mStatusMessage;

    /** Native pointer to the UIController. */
    private final long mUiControllerAndroid;

    private UiDelegateHolder mUiDelegateHolder;

    /**
     * Indicates whether {@link mAccount} has been initialized.
     */
    private boolean mAccountInitialized;

    /**
     * Account to authenticate as when sending RPCs. Not relevant until the accounts have been
     * fetched, and mAccountInitialized set to true. Can still be null after the accounts are
     * fetched, in which case authentication is disabled.
     */
    @Nullable
    private Account mAccount;

    /** If set, fetch the access token once the accounts are fetched. */
    private boolean mShouldFetchAccessToken;

    /**
     * Construct Autofill Assistant UI controller.
     *
     * @param activity The CustomTabActivity of the controller associated with.
     */
    public AutofillAssistantUiController(
            CustomTabActivity activity, Map<String, String> parameters) {
        mWebContents = activity.getActivityTab().getWebContents();
        mInitialUrl = activity.getInitialIntent().getDataString();
        mUiControllerAndroid =
                nativeInit(mWebContents, parameters.keySet().toArray(new String[parameters.size()]),
                        parameters.values().toArray(new String[parameters.size()]),
                        LocaleUtils.getDefaultLocaleString(), getCountryIso());

        chooseAccountAsync(activity.getInitialIntent().getExtras());
    }

    void setUiDelegateHolder(UiDelegateHolder uiDelegateHolder) {
        mUiDelegateHolder = uiDelegateHolder;
    }

    @Override
    public void onInitOk() {
        assert mUiDelegateHolder != null;
        nativeStart(mUiControllerAndroid, mInitialUrl);
    }

    @Override
    public void onDismiss() {
        mUiDelegateHolder.dismiss(R.string.autofill_assistant_stopped);
    }

    @Override
    public AutofillAssistantUiDelegate.Details getDetails() {
        return mCurrentDetails;
    }

    @Override
    public String getStatusMessage() {
        return mStatusMessage;
    }

    @Override
    public void onUnexpectedTaps() {
        mUiDelegateHolder.dismiss(R.string.autofill_assistant_maybe_give_up);
    }

    @Override
    public void scrollBy(float distanceXRatio, float distanceYRatio) {
        nativeScrollBy(mUiControllerAndroid, distanceXRatio, distanceYRatio);
    }

    @Override
    public void updateTouchableArea() {
        nativeUpdateTouchableArea(mUiControllerAndroid);
    }

    @Override
    public void onScriptSelected(String scriptPath) {
        nativeOnScriptSelected(mUiControllerAndroid, scriptPath);
    }

    @Override
    public void onAddressSelected(String guid) {
        nativeOnAddressSelected(mUiControllerAndroid, guid);
    }

    @Override
    public void onCardSelected(String guid) {
        nativeOnCardSelected(mUiControllerAndroid, guid);
    }

    @Override
    public String getDebugContext() {
        return nativeOnRequestDebugContext(mUiControllerAndroid);
    }

    /**
     * Immediately and unconditionally destroys the UI Controller.
     *
     * <p>Call {@link UiDelegateHolder#shutdown} to shutdown Autofill Assistant properly and safely.
     *
     * <p>Destroy is different from shutdown in that {@code unsafeDestroy} just deletes the native
     * controller and all the objects it owns, without changing the state of the UI. When that
     * happens, everything stops irrevocably on the native side. Doing this at the wrong time will
     * cause crashes.
     */
    void unsafeDestroy() {
        nativeDestroy(mUiControllerAndroid);
    }

    @CalledByNative
    private void onShowStatusMessage(String message) {
        mStatusMessage = message;
        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.showStatusMessage(message));
    }

    @CalledByNative
    private void onShowOverlay() {
        mUiDelegateHolder.performUiOperation(uiDelegate -> {
            uiDelegate.showOverlay();
            uiDelegate.disableProgressBarPulsing();
        });
    }

    @CalledByNative
    private void onHideOverlay() {
        mUiDelegateHolder.performUiOperation(uiDelegate -> {
            uiDelegate.hideOverlay();
            uiDelegate.enableProgressBarPulsing();
        });
    }

    @CalledByNative
    private void onShutdown() {
        mUiDelegateHolder.shutdown();
    }

    @CalledByNative
    private void onShutdownGracefully() {
        mUiDelegateHolder.enterGracefulShutdownMode();
    }

    @CalledByNative
    private void onUpdateScripts(
            String[] scriptNames, String[] scriptPaths, boolean[] scriptsHighlightFlags) {
        assert scriptNames.length == scriptPaths.length;
        assert scriptNames.length == scriptsHighlightFlags.length;

        ArrayList<AutofillAssistantUiDelegate.ScriptHandle> scriptHandles = new ArrayList<>();
        // Note that scriptNames, scriptPaths and scriptsHighlightFlags are one-on-one matched by
        // index.
        for (int i = 0; i < scriptNames.length; i++) {
            scriptHandles.add(new AutofillAssistantUiDelegate.ScriptHandle(
                    scriptNames[i], scriptPaths[i], scriptsHighlightFlags[i]));
        }

        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.updateScripts(scriptHandles));
    }

    @CalledByNative
    private void onChooseAddress() {
        // TODO(crbug.com/806868): Remove this method once all scripts use payment request.
        mUiDelegateHolder.performUiOperation(uiDelegate
                -> uiDelegate.showProfiles(PersonalDataManager.getInstance().getProfilesToSuggest(
                        /* includeNameInLabel= */ true)));
    }

    @CalledByNative
    private void onChooseCard() {
        // TODO(crbug.com/806868): Remove this method once all scripts use payment request.
        mUiDelegateHolder.performUiOperation(uiDelegate
                -> uiDelegate.showCards(PersonalDataManager.getInstance().getCreditCardsToSuggest(
                        /* includeServerCards= */ true)));
    }

    @CalledByNative
    private void onRequestPaymentInformation(boolean requestShipping, boolean requestPayerName,
            boolean requestPayerPhone, boolean requestPayerEmail, int shippingType, String title,
            String[] supportedBasicCardNetworks) {
        PaymentOptions paymentOptions = new PaymentOptions();
        paymentOptions.requestShipping = requestShipping;
        paymentOptions.requestPayerName = requestPayerName;
        paymentOptions.requestPayerPhone = requestPayerPhone;
        paymentOptions.requestPayerEmail = requestPayerEmail;
        paymentOptions.shippingType = shippingType;

        mUiDelegateHolder.performUiOperation(uiDelegate -> {
            uiDelegate.showPaymentRequest(mWebContents, paymentOptions, title,
                    supportedBasicCardNetworks, (selectedPaymentInformation -> {
                        uiDelegate.closePaymentRequest();
                        if (selectedPaymentInformation.succeed) {
                            nativeOnGetPaymentInformation(mUiControllerAndroid,
                                    selectedPaymentInformation.succeed,
                                    selectedPaymentInformation.card,
                                    selectedPaymentInformation.address,
                                    selectedPaymentInformation.payerName,
                                    selectedPaymentInformation.payerPhone,
                                    selectedPaymentInformation.payerEmail,
                                    selectedPaymentInformation.isTermsAndConditionsAccepted);
                        } else {
                            // A failed payment request flow indicates that the UI was either
                            // dismissed or the back button was clicked. In that case we gracefully
                            // shut down.
                            mUiDelegateHolder.giveUp();
                        }
                    }));
        });
    }

    /**
     * Updates the currently shown details.
     *
     * @return false if details were rejected.
     */
    boolean maybeUpdateDetails(AutofillAssistantUiDelegate.Details newDetails) {
        if (!mCurrentDetails.isSimilarTo(newDetails)) {
            return false;
        }

        if (mCurrentDetails.isEmpty() && newDetails.isEmpty()) {
            // No update on UI needed.
            return true;
        }

        mCurrentDetails = AutofillAssistantUiDelegate.Details.merge(mCurrentDetails, newDetails);
        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.showDetails(mCurrentDetails));
        return true;
    }

    @CalledByNative
    private void onHideDetails() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::hideDetails);
    }

    @CalledByNative
    private boolean onShowDetails(String title, String url, String description, int year, int month,
            int day, int hour, int minute, int second) {
        Date date;
        if (year > 0 && month > 0 && day > 0 && hour >= 0 && minute >= 0 && second >= 0) {
            Calendar calendar = Calendar.getInstance();
            calendar.clear();
            // Month in Java Date is 0-based, but the one we receive from the server is 1-based.
            calendar.set(year, month - 1, day, hour, minute, second);
            date = calendar.getTime();
        } else {
            date = null;
        }

        return maybeUpdateDetails(new AutofillAssistantUiDelegate.Details(
                title, url, date, description, /* isFinal= */ true));
    }

    @CalledByNative
    private void onShowProgressBar(int progress, String message) {
        mUiDelegateHolder.performUiOperation(
                uiDelegate -> uiDelegate.showProgressBar(progress, message));
    }

    @CalledByNative
    private void onHideProgressBar() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::hideProgressBar);
    }

    @CalledByNative
    private void updateTouchableArea(boolean enabled, float[] coords) {
        List<RectF> boxes = new ArrayList<>();
        for (int i = 0; i < coords.length; i += 4) {
            boxes.add(new RectF(/* left= */ coords[i], /* top= */ coords[i + 1],
                    /* right= */ coords[i + 2], /* bottom= */ coords[i + 3]));
        }
        mUiDelegateHolder.performUiOperation(
                uiDelegate -> { uiDelegate.updateTouchableArea(enabled, boxes); });
    }


    @CalledByNative
    private void fetchAccessToken() {
        if (!mAccountInitialized) {
            // Still getting the account list. Fetch the token as soon as an account is available.
            mShouldFetchAccessToken = true;
            return;
        }
        if (mAccount == null) {
            nativeOnAccessToken(mUiControllerAndroid, true, "");
            return;
        }

        AccountManagerFacade.get().getAuthToken(
                mAccount, AUTH_TOKEN_TYPE, new AccountManagerFacade.GetAuthTokenCallback() {
                    @Override
                    public void tokenAvailable(String token) {
                        nativeOnAccessToken(mUiControllerAndroid, true, token);
                    }

                    @Override
                    public void tokenUnavailable(boolean isTransientError) {
                        if (!isTransientError) {
                            nativeOnAccessToken(mUiControllerAndroid, false, "");
                        }
                    }
                });
    }

    @CalledByNative
    private void invalidateAccessToken(String accessToken) {
        if (mAccount == null) {
            return;
        }

        AccountManagerFacade.get().invalidateAuthToken(accessToken);
    }

    /** Choose an account to authenticate as for making RPCs to the backend. */
    private void chooseAccountAsync(Bundle extras) {
        AccountManagerFacade.get().tryGetGoogleAccounts(accounts -> {
            if (accounts.length == 1) {
                // If there's only one account, there aren't any doubts.
                onAccountChosen(accounts[0]);
                return;
            }
            Account signedIn =
                    findAccountByName(accounts, nativeGetPrimaryAccountName(mUiControllerAndroid));
            if (signedIn != null) {
                // TODO(crbug.com/806868): Compare against account name from extras and complain if
                // they don't match.
                onAccountChosen(signedIn);
                return;
            }
            for (String extra : extras.keySet()) {
                if (extra.endsWith("ACCOUNT_NAME")) {
                    Account account = findAccountByName(accounts, extras.getString(extra));
                    if (account != null) {
                        onAccountChosen(account);
                        return;
                    }
                }
            }
            onAccountChosen(null);
        });
    }

    private void onAccountChosen(@Nullable Account account) {
        mAccount = account;
        mAccountInitialized = true;
        // TODO(crbug.com/806868): Consider providing a way of signing in this case, to enforce
        // that all calls are authenticated.

        if (mShouldFetchAccessToken) {
            mShouldFetchAccessToken = false;
            fetchAccessToken();
        }
    }

    private static Account findAccountByName(Account[] accounts, String name) {
        for (Account account : accounts) {
            if (account.name.equals(name)) {
                return account;
            }
        }
        return null;
    }

    /** Returns the country that the device is currently located in. This currently only works
     * for devices with active SIM cards. For a more general solution, we should probably use
     * the LocationManager together with the Geocoder.*/
    private String getCountryIso() {
        TelephonyManager telephonyManager =
                (TelephonyManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.TELEPHONY_SERVICE);

        // According to API, location for CDMA networks is unreliable
        if (telephonyManager != null
                && telephonyManager.getPhoneType() != TelephonyManager.PHONE_TYPE_CDMA)
            return telephonyManager.getNetworkCountryIso();
        else
            return null;
    }

    // native methods.
    private native long nativeInit(WebContents webContents, String[] parameterNames,
            String[] parameterValues, String locale, String countryCode);
    private native void nativeStart(long nativeUiControllerAndroid, String initialUrl);
    private native void nativeDestroy(long nativeUiControllerAndroid);
    private native void nativeScrollBy(
            long nativeUiControllerAndroid, float distanceXRatio, float distanceYRatio);
    private native void nativeUpdateTouchableArea(long nativeUiControllerAndroid);
    private native void nativeOnScriptSelected(long nativeUiControllerAndroid, String scriptPath);
    private native void nativeOnAddressSelected(long nativeUiControllerAndroid, String guid);
    private native void nativeOnCardSelected(long nativeUiControllerAndroid, String guid);
    private native void nativeOnGetPaymentInformation(long nativeUiControllerAndroid,
            boolean succeed, @Nullable PersonalDataManager.CreditCard card,
            @Nullable PersonalDataManager.AutofillProfile address, @Nullable String payerName,
            @Nullable String payerPhone, @Nullable String payerEmail,
            boolean isTermsAndConditionsAccepted);
    private native void nativeOnAccessToken(
            long nativeUiControllerAndroid, boolean success, String accessToken);
    private native String nativeGetPrimaryAccountName(long nativeUiControllerAndroid);
    private native String nativeOnRequestDebugContext(long nativeUiControllerAndroid);
}
