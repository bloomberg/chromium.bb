// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.os.Bundle;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SyncAndServicesPreferences;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This fragment implements sign-in screen for {@link SigninActivity}. */
public class SigninFragment extends SigninFragmentBase {
    private static final String TAG = "SigninFragment";

    private static final String ARGUMENT_ACCESS_POINT = "SigninFragment.AccessPoint";
    private static final String ARGUMENT_PERSONALIZED_PROMO_ACTION =
            "SigninFragment.PersonalizedPromoAction";

    @IntDef({PromoAction.NONE, PromoAction.WITH_DEFAULT, PromoAction.NOT_DEFAULT,
            PromoAction.NEW_ACCOUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PromoAction {
        int NONE = 0;
        int WITH_DEFAULT = 1;
        int NOT_DEFAULT = 2;
        int NEW_ACCOUNT = 3;
    }

    private @SigninAccessPoint int mSigninAccessPoint;
    private @PromoAction int mPromoAction;

    /**
     * Creates an argument bundle to start sign-in.
     * @param accessPoint The access point for starting sign-in flow.
     */
    public static Bundle createArguments(@SigninAccessPoint int accessPoint) {
        Bundle result = SigninFragmentBase.createArguments(null);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        return result;
    }

    /**
     * Creates an argument bundle to start sign-in from personalized sign-in promo.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Bundle createArgumentsForPromoDefaultFlow(
            @SigninAccessPoint int accessPoint, String accountName) {
        Bundle result = SigninFragmentBase.createArguments(accountName);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putInt(ARGUMENT_PERSONALIZED_PROMO_ACTION, PromoAction.WITH_DEFAULT);
        return result;
    }

    /**
     * Creates an argument bundle to start "Choose account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Bundle createArgumentsForPromoChooseAccountFlow(
            @SigninAccessPoint int accessPoint, String accountName) {
        Bundle result = SigninFragmentBase.createArgumentsForChooseAccountFlow(accountName);
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putInt(ARGUMENT_PERSONALIZED_PROMO_ACTION, PromoAction.NOT_DEFAULT);
        return result;
    }

    /**
     * Creates an argument bundle to start "New account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint The access point for starting sign-in flow.
     */
    public static Bundle createArgumentsForPromoAddAccountFlow(@SigninAccessPoint int accessPoint) {
        Bundle result = SigninFragmentBase.createArgumentsForAddAccountFlow();
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putInt(ARGUMENT_PERSONALIZED_PROMO_ACTION, PromoAction.NEW_ACCOUNT);
        return result;
    }

    // Every fragment must have a public default constructor.
    public SigninFragment() {}

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int accessPoint = getSigninArguments().getInt(ARGUMENT_ACCESS_POINT, -1);
        assert accessPoint == SigninAccessPoint.AUTOFILL_DROPDOWN
                || accessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                || accessPoint == SigninAccessPoint.NTP_CONTENT_SUGGESTIONS
                || accessPoint == SigninAccessPoint.RECENT_TABS
                || accessPoint == SigninAccessPoint.SETTINGS
                || accessPoint == SigninAccessPoint.SIGNIN_PROMO
                || accessPoint
                        == SigninAccessPoint.START_PAGE : "invalid access point: " + accessPoint;
        mSigninAccessPoint = accessPoint;
        mPromoAction =
                getSigninArguments().getInt(ARGUMENT_PERSONALIZED_PROMO_ACTION, PromoAction.NONE);

        SigninManager.logSigninStartAccessPoint(mSigninAccessPoint);
        recordSigninStartedHistogramAccountInfo();
        recordSigninStartedUserAction();
    }

    @Override
    protected Bundle getSigninArguments() {
        return getArguments();
    }

    @Override
    protected void onSigninRefused() {
        getActivity().finish();
    }

    @Override
    protected void onSigninAccepted(String accountName, boolean isDefaultAccount,
            boolean settingsClicked, Runnable callback) {
        if (PrefServiceBridge.getInstance().getSyncLastAccountName() != null) {
            AccountSigninActivity.recordSwitchAccountSourceHistogram(
                    AccountSigninActivity.SwitchAccountSource.SIGNOUT_SIGNIN);
        }

        SigninManager.get().signIn(accountName, getActivity(), new SigninManager.SignInCallback() {
            @Override
            public void onSignInComplete() {
                UnifiedConsentServiceBridge.setUnifiedConsentGiven(true);
                if (settingsClicked) {
                    PreferencesLauncher.launchSettingsPage(
                            getActivity(), SyncAndServicesPreferences.class);
                }

                recordSigninCompletedHistogramAccountInfo();
                getActivity().finish();
                callback.run();
            }

            @Override
            public void onSignInAborted() {
                callback.run();
            }
        });
    }

    @Override
    protected int getNegativeButtonTextId() {
        return mSigninAccessPoint == SigninAccessPoint.SIGNIN_PROMO ? R.string.no_thanks
                                                                    : R.string.cancel;
    }

    private void recordSigninCompletedHistogramAccountInfo() {
        final String histogram;
        switch (mPromoAction) {
            case PromoAction.NONE:
                return;
            case PromoAction.WITH_DEFAULT:
                histogram = "Signin.SigninCompletedAccessPoint.WithDefault";
                break;
            case PromoAction.NOT_DEFAULT:
                histogram = "Signin.SigninCompletedAccessPoint.NotDefault";
                break;
            case PromoAction.NEW_ACCOUNT:
                // On Android, the promo does not have a button to add an account when there is
                // already an account on the device. That flow goes through the NotDefault promo
                // instead. Always use the NoExistingAccount variant.
                histogram = "Signin.SigninCompletedAccessPoint.NewAccountNoExistingAccount";
                break;
            default:
                assert false : "Unexpected sign-in flow type!";
                return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                histogram, mSigninAccessPoint, SigninAccessPoint.MAX);
    }

    private void recordSigninStartedHistogramAccountInfo() {
        final String histogram;
        switch (mPromoAction) {
            case PromoAction.NONE:
                return;
            case PromoAction.WITH_DEFAULT:
                histogram = "Signin.SigninStartedAccessPoint.WithDefault";
                break;
            case PromoAction.NOT_DEFAULT:
                histogram = "Signin.SigninStartedAccessPoint.NotDefault";
                break;
            case PromoAction.NEW_ACCOUNT:
                // On Android, the promo does not have a button to add an account when there is
                // already an account on the device. That flow goes through the NotDefault promo
                // instead. Always use the NoExistingAccount variant.
                histogram = "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount";
                break;
            default:
                assert false : "Unexpected sign-in flow type!";
                return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                histogram, mSigninAccessPoint, SigninAccessPoint.MAX);
    }

    private void recordSigninStartedUserAction() {
        switch (mSigninAccessPoint) {
            case SigninAccessPoint.AUTOFILL_DROPDOWN:
                RecordUserAction.record("Signin_Signin_FromAutofillDropdown");
                break;
            case SigninAccessPoint.BOOKMARK_MANAGER:
                RecordUserAction.record("Signin_Signin_FromBookmarkManager");
                break;
            case SigninAccessPoint.RECENT_TABS:
                RecordUserAction.record("Signin_Signin_FromRecentTabs");
                break;
            case SigninAccessPoint.SETTINGS:
                RecordUserAction.record("Signin_Signin_FromSettings");
                break;
            case SigninAccessPoint.SIGNIN_PROMO:
                RecordUserAction.record("Signin_Signin_FromSigninPromo");
                break;
            case SigninAccessPoint.NTP_CONTENT_SUGGESTIONS:
                RecordUserAction.record("Signin_Signin_FromNTPContentSuggestions");
                break;
            default:
                assert false : "Invalid access point.";
        }
    }
}
