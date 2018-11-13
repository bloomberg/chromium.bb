// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.accounts.Account;
import android.graphics.RectF;
import android.os.Bundle;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.preferences.autofill_assistant.AutofillAssistantPreferences;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentOptions;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Queue;

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

    /** Display the final message for that long before shutting everything down. */
    private static final long GRACEFUL_SHUTDOWN_DELAY_MS = 5000;

    private static final String RFC_3339_FORMAT = "yyyy'-'MM'-'dd'T'HH':'mm':'ssZ";

    private final WebContents mWebContents;
    private final UiDelegateHolder mUiDelegateHolder;
    private final String mInitialUrl;

    /**
     * Native pointer to the UIController.
     */
    private final long mUiControllerAndroid;

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
     * Returns true if all conditions are satisfied to construct an AutofillAssistantUiController.
     *
     * @return True if a controller can be constructed.
     */
    public static boolean isConfigured(Bundle intentExtras) {
        return getBooleanParameter(intentExtras, PARAMETER_ENABLED)
                && !AutofillAssistantStudy.getUrl().isEmpty()
                && ContextUtils.getAppSharedPreferences().getBoolean(
                           AutofillAssistantPreferences.PREF_AUTOFILL_ASSISTANT_SWITCH, true);
    }

    @Override
    public void onInitOk() {
        nativeStart(mUiControllerAndroid, mInitialUrl);
    }

    /**
     * Construct Autofill Assistant UI controller.
     *
     * @param activity The CustomTabActivity of the controller associated with.
     */
    public AutofillAssistantUiController(CustomTabActivity activity) {
        // Set mUiDelegate before nativeInit, as it can be accessed through native methods from
        // nativeInit already.
        mUiDelegateHolder = new UiDelegateHolder(new AutofillAssistantUiDelegate(activity, this));
        chooseAccountAsync(activity.getInitialIntent().getExtras());

        Map<String, String> parameters = extractParameters(activity.getInitialIntent().getExtras());
        parameters.remove(PARAMETER_ENABLED);

        AutofillAssistantUiDelegate.Details initialDetails = makeDetailsFromParameters(parameters);
        if (!initialDetails.isEmpty()) {
            mUiDelegateHolder.performUiOperation(
                    uiDelegate -> uiDelegate.showDetails(initialDetails));
        }

        Tab activityTab = activity.getActivityTab();
        mWebContents = activityTab.getWebContents();
        mInitialUrl = activity.getInitialIntent().getDataString();

        mUiControllerAndroid =
                nativeInit(mWebContents, parameters.keySet().toArray(new String[parameters.size()]),
                        parameters.values().toArray(new String[parameters.size()]));

        mUiDelegateHolder.performUiOperation(uiDelegate -> uiDelegate.startOrSkipInitScreen());

        // Shut down Autofill Assistant when the tab is detached from the activity.
        activityTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                if (!isAttached) {
                    activityTab.removeObserver(this);
                    mUiDelegateHolder.shutdown();
                }
            }
        });

        // Shut down Autofill Assistant when the selected tab (foreground tab) is changed.
        TabModel currentTabModel = activity.getTabModelSelector().getCurrentModel();
        currentTabModel.addObserver(new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                currentTabModel.removeObserver(this);

                nativeGiveUp(mUiControllerAndroid);
            }
        });
    }

    @Override
    public void onDismiss() {
        mUiDelegateHolder.dismiss(R.string.autofill_assistant_stopped);
    }

    @Override
    public void onUnexpectedTaps() {
        mUiDelegateHolder.dismiss(R.string.autofill_assistant_maybe_give_up);
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

    /** Return the value if the given boolean parameter from the extras. */
    private static boolean getBooleanParameter(Bundle extras, String parameterName) {
        return extras.getBoolean(INTENT_EXTRA_PREFIX + parameterName, false);
    }

    /** Returns a map containing the extras starting with {@link #INTENT_EXTRA_PREFIX}. */
    private static Map<String, String> extractParameters(Bundle extras) {
        Map<String, String> result = new HashMap<>();
        for (String key : extras.keySet()) {
            if (key.startsWith(INTENT_EXTRA_PREFIX)) {
                result.put(key.substring(INTENT_EXTRA_PREFIX.length()), extras.get(key).toString());
            }
        }
        return result;
    }

    // TODO(crbug.com/806868): Create a fallback when there are no parameters for details.
    private static AutofillAssistantUiDelegate.Details makeDetailsFromParameters(
            Map<String, String> parameters) {
        String title = "";
        String description = "";
        Date date = null;
        for (String key : parameters.keySet()) {
            if (key.contains("E_NAME")) {
                title = parameters.get(key);
                continue;
            }

            if (key.contains("R_NAME")) {
                description = parameters.get(key);
                continue;
            }

            if (key.contains("DATETIME")) {
                try {
                    date = new SimpleDateFormat(RFC_3339_FORMAT, Locale.getDefault())
                                   .parse(parameters.get(key));
                } catch (ParseException e) {
                    // Ignore.
                }
            }
        }

        return new AutofillAssistantUiDelegate.Details(
                title, /* url= */ "", date, description, /* isFinal= */ false);
    }

    @Override
    public void onClickOverlay() {
        // TODO(crbug.com/806868): Notify native side.
    }

    @CalledByNative
    private void onShowStatusMessage(String message) {
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
                            onHideOverlay();
                            uiDelegate.showGiveUpMessage();
                            onShutdownGracefully();
                        }
                    }));
        });
    }

    @CalledByNative
    private void onHideDetails() {
        mUiDelegateHolder.performUiOperation(AutofillAssistantUiDelegate::hideDetails);
    }

    @CalledByNative
    private void onShowDetails(String title, String url, String description, int year, int month,
            int day, int hour, int minute, int second) {
        Date date;
        if (year > 0 && month > 0 && day > 0 && hour >= 0 && minute >= 0 && second >= 0) {
            Calendar calendar = Calendar.getInstance();
            // Month in Java Date is 0-based, but the one we receive from the server is 1-based.
            calendar.set(year, month - 1, day, hour, minute, second);
            date = calendar.getTime();
        } else {
            date = null;
        }

        mUiDelegateHolder.performUiOperation(uiDelegate
                -> uiDelegate.showDetails(new AutofillAssistantUiDelegate.Details(
                        title, url, date, description, /* isFinal= */ true)));
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

    /**
     * Class holder for the AutofillAssistantUiDelegate to make sure we don't make UI changes when
     * we are in a pause state (i.e. few seconds before stopping completely).
     */
    private class UiDelegateHolder {
        private final AutofillAssistantUiDelegate mUiDelegate;

        private boolean mShouldQueueUiOperations = false;
        private boolean mHasBeenShutdown = false;
        private boolean mIsShuttingDown = false;
        private SnackbarManager.SnackbarController mDismissSnackbar;
        private final Queue<Callback<AutofillAssistantUiDelegate>> mPendingUiOperations =
                new ArrayDeque<>();

        private UiDelegateHolder(AutofillAssistantUiDelegate uiDelegate) {
            mUiDelegate = uiDelegate;
        }

        /**
         * Perform a UI operation:
         *  - directly if we are not in a pause state.
         *  - later if the shutdown is cancelled.
         *  - never if Autofill Assistant is shut down.
         */
        public void performUiOperation(Callback<AutofillAssistantUiDelegate> operation) {
            if (mHasBeenShutdown || mIsShuttingDown) {
                return;
            }

            if (mShouldQueueUiOperations) {
                mPendingUiOperations.add(operation);
                return;
            }

            operation.onResult(mUiDelegate);
        }

        /**
         * Handles the dismiss operation.
         *
         * In normal mode, hides the UI, pauses UI operations and, unless undone within the time
         * delay, eventually destroy everything. In graceful shutdown mode, shutdown immediately.
         */
        public void dismiss(int stringResourceId, Object... formatArgs) {
            assert !mHasBeenShutdown;

            if (mIsShuttingDown) {
                shutdown();
                return;
            }

            if (mDismissSnackbar != null) {
                // Remove duplicate calls.
                return;
            }

            pauseUiOperations();
            mUiDelegate.hide();
            mDismissSnackbar = new SnackbarManager.SnackbarController() {
                @Override
                public void onAction(Object actionData) {
                    // Shutdown was cancelled.
                    mDismissSnackbar = null;
                    mUiDelegate.show();
                    unpauseUiOperations();
                }

                @Override
                public void onDismissNoAction(Object actionData) {
                    shutdown();
                }
            };
            mUiDelegate.showAutofillAssistantStoppedSnackbar(
                    mDismissSnackbar, stringResourceId, formatArgs);
        }

        /** Enters graceful shutdown mode once we can again perform UI operations. */
        public void enterGracefulShutdownMode() {
            mUiDelegateHolder.performUiOperation(uiDelegate -> {
                mIsShuttingDown = true;
                mPendingUiOperations.clear();
                uiDelegate.enterGracefulShutdownMode();
                ThreadUtils.postOnUiThreadDelayed(this ::shutdown, GRACEFUL_SHUTDOWN_DELAY_MS);
            });
        }

        /**
         * Hides the UI and destroys everything.
         *
         * <p>Shutdown is final: After this call from the C++ side, as it's been deleted and no UI
         * operation can run.
         */
        public void shutdown() {
            if (mHasBeenShutdown) {
                return;
            }

            mHasBeenShutdown = true;
            mPendingUiOperations.clear();
            if (mDismissSnackbar != null) {
                mUiDelegate.dismissSnackbar(mDismissSnackbar);
            }
            mUiDelegate.hide();
            nativeDestroy(mUiControllerAndroid);
        }

        /**
         * Pause all UI operations such that they can potentially be ran later using {@link
         * #unpauseUiOperations()}.
         */
        private void pauseUiOperations() {
            mShouldQueueUiOperations = true;
        }

        /**
         * Unpause and trigger all UI operations received by {@link #performUiOperation(Callback)}
         * since the last {@link #pauseUiOperations()}.
         */
        private void unpauseUiOperations() {
            mShouldQueueUiOperations = false;
            while (!mPendingUiOperations.isEmpty()) {
                mPendingUiOperations.remove().onResult(mUiDelegate);
            }
        }
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

    // native methods.
    private native long nativeInit(
            WebContents webContents, String[] parameterNames, String[] parameterValues);
    private native void nativeStart(long nativeUiControllerAndroid, String initialUrl);
    private native void nativeDestroy(long nativeUiControllerAndroid);
    private native void nativeGiveUp(long nativeUiControllerAndroid);
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
