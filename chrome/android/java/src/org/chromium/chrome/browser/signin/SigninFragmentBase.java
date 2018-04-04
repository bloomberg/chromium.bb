// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.os.Bundle;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.AlertDialog;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.consent_auditor.ConsentAuditorFeature;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerResult;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.GmsAvailabilityException;
import org.chromium.components.signin.GmsJustUpdatedException;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * This fragment implements sign-in screen with account picker and descriptions of signin-related
 * features. Configuration for this fragment is provided by overriding {@link #getSigninArguments}
 * derived classes.
 */
public abstract class SigninFragmentBase
        extends Fragment implements AccountPickerDialogFragment.Callback {
    private static final String TAG = "SigninFragmentBase";

    private static final String SETTINGS_LINK_OPEN = "<LINK1>";
    private static final String SETTINGS_LINK_CLOSE = "</LINK1>";

    private static final String ARGUMENT_ACCESS_POINT = "SigninFragmentBase.AccessPoint";
    public static final String ACCOUNT_PICKER_DIALOG_TAG =
            "SigninFragmentBase.AccountPickerDialogFragment";

    private @SigninAccessPoint int mSigninAccessPoint;
    // TODO(https://crbug.com/814728): Pass this as Fragment argument.
    private boolean mAllowAccountSelection = true;

    private SigninView mView;
    private ConsentTextTracker mConsentTextTracker;
    private @StringRes int mCancelButtonTextId = R.string.cancel;

    private boolean mPreselectedAccount;
    private String mSelectedAccountName;
    private boolean mIsDefaultAccountSelected;
    private AccountsChangeObserver mAccountsChangedObserver;
    private ProfileDataCache.Observer mProfileDataCacheObserver;
    private ProfileDataCache mProfileDataCache;
    private List<String> mAccountNames;
    private boolean mResumed;
    // TODO(https://crbug.com/814728): Ignore button clicks if GMS reported an error.
    private boolean mHasGmsError;

    private UserRecoverableErrorHandler.ModalDialog mGooglePlayServicesUpdateErrorHandler;
    private AlertDialog mGmsIsUpdatingDialog;
    private long mGmsIsUpdatingDialogShowTime;

    /**
     * Creates an argument bundle to start AccountSigninView from the account selection page.
     * @param accessPoint The access point for starting signin flow.
     */
    protected static Bundle createArguments(@SigninAccessPoint int accessPoint) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        return result;
    }

    protected SigninFragmentBase() {
        mAccountsChangedObserver = this::triggerUpdateAccounts;
        mProfileDataCacheObserver = (String accountId) -> updateProfileData();
    }

    /**
     * This method should return arguments Bundle that contains arguments created by
     * {@link #createArguments} and related methods.
     */
    protected abstract Bundle getSigninArguments();

    /** The sign-in was refused. */
    protected abstract void onSigninRefused();

    /**
     * The sign-in was accepted.
     * @param accountName The name of the account
     * @param isDefaultAccount Whether selected account is a default one (first of all accounts)
     * @param settingsClicked Whether the user requested to see their sync settings
     */
    protected abstract void onSigninAccepted(
            String accountName, boolean isDefaultAccount, boolean settingsClicked);

    /** Returns the access point that initiated the sign-in flow. */
    protected @SigninAccessPoint int getSigninAccessPoint() {
        return mSigninAccessPoint;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Bundle arguments = getSigninArguments();
        initAccessPoint(arguments.getInt(ARGUMENT_ACCESS_POINT, -1));

        mConsentTextTracker = new ConsentTextTracker(getResources());

        mProfileDataCache = new ProfileDataCache(getActivity(),
                getResources().getDimensionPixelSize(R.dimen.signin_account_image_size));

        // TODO(https://crbug.com/814728): Disable controls until account is preselected.
        mPreselectedAccount = false;
    }

    private void initAccessPoint(@SigninAccessPoint int accessPoint) {
        assert accessPoint == SigninAccessPoint.AUTOFILL_DROPDOWN
                || accessPoint == SigninAccessPoint.BOOKMARK_MANAGER
                || accessPoint == SigninAccessPoint.NTP_CONTENT_SUGGESTIONS
                || accessPoint == SigninAccessPoint.RECENT_TABS
                || accessPoint == SigninAccessPoint.SETTINGS
                || accessPoint == SigninAccessPoint.SIGNIN_PROMO
                || accessPoint
                        == SigninAccessPoint.START_PAGE : "invalid access point: " + accessPoint;

        mSigninAccessPoint = accessPoint;
        if (accessPoint == SigninAccessPoint.START_PAGE
                || accessPoint == SigninAccessPoint.SIGNIN_PROMO) {
            mCancelButtonTextId = R.string.no_thanks;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        dismissGmsErrorDialog();
        dismissGmsUpdatingDialog();
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mView = (SigninView) inflater.inflate(R.layout.signin_view, container, false);

        mView.getAccountPickerView().setOnClickListener(view -> onAccountPickerClicked());

        mView.getAcceptButton().setVisibility(View.GONE);
        mView.getMoreButton().setVisibility(View.VISIBLE);
        mView.getMoreButton().setOnClickListener(view -> {
            mView.getScrollView().smoothScrollBy(0, mView.getScrollView().getHeight());
            // TODO(https://crbug.com/821127): Revise this user action.
            RecordUserAction.record("Signin_MoreButton_Shown");
        });
        mView.getScrollView().setScrolledToBottomObserver(this::showAcceptButton);

        mView.getRefuseButton().setOnClickListener(view -> {
            setButtonsEnabled(false);
            onSigninRefused();
        });

        updateConsentText();
        return mView;
    }

    private void updateConsentText() {
        mConsentTextTracker.setText(mView.getTitleView(), R.string.signin_title);
        mConsentTextTracker.setText(
                mView.getSyncDescriptionView(), R.string.signin_sync_description);
        mConsentTextTracker.setText(mView.getPersonalizationDescriptionView(),
                R.string.signin_personalization_description);
        mConsentTextTracker.setText(mView.getGoogleServicesDescriptionView(),
                R.string.signin_google_services_description);
        mConsentTextTracker.setText(mView.getRefuseButton(), mCancelButtonTextId);
        mConsentTextTracker.setText(mView.getAcceptButton(), R.string.signin_accept_button);
        mConsentTextTracker.setText(mView.getMoreButton(), R.string.more);

        // The clickable "Settings" link.
        mView.getDetailsDescriptionView().setMovementMethod(LinkMovementMethod.getInstance());
        NoUnderlineClickableSpan settingsSpan = new NoUnderlineClickableSpan() {
            @Override
            public void onClick(View widget) {
                onSigninAccepted(mSelectedAccountName, mIsDefaultAccountSelected, true);
                RecordUserAction.record("Signin_Signin_WithAdvancedSyncSettings");

                // Record the fact that the user consented to the consent text by clicking on a link
                recordConsent((TextView) widget);
            }
        };
        mConsentTextTracker.setText(
                mView.getDetailsDescriptionView(), R.string.signin_details_description, input -> {
                    return SpanApplier.applySpans(input.toString(),
                            new SpanApplier.SpanInfo(
                                    SETTINGS_LINK_OPEN, SETTINGS_LINK_CLOSE, settingsSpan));
                });
    }

    private void updateProfileData() {
        if (mSelectedAccountName == null) return;
        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mSelectedAccountName);
        mView.getAccountImageView().setImageDrawable(profileData.getImage());
        mConsentTextTracker.setTextNonRecordable(
                mView.getAccountNameView(), profileData.getFullNameOrEmail());
        mConsentTextTracker.setTextNonRecordable(
                mView.getAccountEmailView(), profileData.getAccountName());
    }

    private void showAcceptButton() {
        mView.getAcceptButton().setVisibility(View.VISIBLE);
        mView.getMoreButton().setVisibility(View.GONE);
        mView.getScrollView().setScrolledToBottomObserver(null);
    }

    private void setButtonsEnabled(boolean enabled) {
        mView.getAcceptButton().setEnabled(enabled);
        mView.getRefuseButton().setEnabled(enabled);
    }

    private void onAccountPickerClicked() {
        // TODO(https://crbug.com/814728): Ignore clicks if GMS reported an error.
        showAccountPicker();
    }

    /**
     * Records the Sync consent.
     * @param confirmationView The view that the user clicked when consenting.
     */
    private void recordConsent(TextView confirmationView) {
        mConsentTextTracker.recordConsent(
                ConsentAuditorFeature.CHROME_SYNC, confirmationView, mView);
    }

    private void showAccountPicker() {
        // Account picker is already shown
        if (getChildFragmentManager().findFragmentByTag(ACCOUNT_PICKER_DIALOG_TAG) != null) return;

        AccountPickerDialogFragment dialog =
                AccountPickerDialogFragment.create(mSelectedAccountName);
        FragmentTransaction transaction = getChildFragmentManager().beginTransaction();
        transaction.add(dialog, ACCOUNT_PICKER_DIALOG_TAG);
        transaction.commit();
    }

    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount) {
        selectAccount(accountName, isDefaultAccount);
    }

    @Override
    public void onResume() {
        super.onResume();
        mResumed = true;
        AccountManagerFacade.get().addObserver(mAccountsChangedObserver);
        mProfileDataCache.addObserver(mProfileDataCacheObserver);
        triggerUpdateAccounts();
    }

    @Override
    public void onPause() {
        super.onPause();
        mResumed = false;
        mProfileDataCache.removeObserver(mProfileDataCacheObserver);
        AccountManagerFacade.get().removeObserver(mAccountsChangedObserver);
    }

    private void selectAccount(String accountName, boolean isDefaultAccount) {
        mSelectedAccountName = accountName;
        mIsDefaultAccountSelected = isDefaultAccount;
        mProfileDataCache.update(Collections.singletonList(mSelectedAccountName));
        updateProfileData();

        AccountPickerDialogFragment accountPickerFragment =
                (AccountPickerDialogFragment) getChildFragmentManager().findFragmentByTag(
                        ACCOUNT_PICKER_DIALOG_TAG);
        if (accountPickerFragment != null) {
            accountPickerFragment.updateSelectedAccount(accountName);
        }
    }

    private void triggerUpdateAccounts() {
        AccountManagerFacade.get().getGoogleAccountNames(this::updateAccounts);
    }

    private void updateAccounts(AccountManagerResult<List<String>> maybeAccountNames) {
        if (!mResumed) return;

        mAccountNames = getAccountNames(maybeAccountNames);
        mHasGmsError = mAccountNames == null;
        if (mAccountNames == null) return;

        if (mAccountNames.isEmpty()) {
            // TODO(https://crbug.com/814728): Hide account picker and change accept button text.
            mSelectedAccountName = null;
            return;
        }

        if (!mPreselectedAccount) {
            selectAccount(mAccountNames.get(0), true);
            mPreselectedAccount = true;
        }

        if (mSelectedAccountName != null && mAccountNames.contains(mSelectedAccountName)) return;

        // No selected account
        if (!mAllowAccountSelection) {
            onSigninRefused();
            return;
        }
        // TODO(https://crbug.com/814728): Cancel ConfirmSyncDataStateMachine.

        selectAccount(mAccountNames.get(0), true);
        showAccountPicker(); // Show account picker so user can confirm the account selection.
    }

    @Nullable
    private List<String> getAccountNames(AccountManagerResult<List<String>> maybeAccountNames) {
        try {
            List<String> result = maybeAccountNames.get();
            dismissGmsErrorDialog();
            dismissGmsUpdatingDialog();
            return result;
        } catch (GmsAvailabilityException e) {
            dismissGmsUpdatingDialog();
            if (e.isUserResolvableError()) {
                showGmsErrorDialog(e.getGmsAvailabilityReturnCode());
            } else {
                Log.e(TAG, "Unresolvable GmsAvailabilityException.", e);
            }
            return null;
        } catch (GmsJustUpdatedException e) {
            dismissGmsErrorDialog();
            showGmsUpdatingDialog();
            return null;
        } catch (AccountManagerDelegateException e) {
            Log.e(TAG, "Unknown exception from AccountManagerFacade.", e);
            dismissGmsErrorDialog();
            dismissGmsUpdatingDialog();
            return null;
        }
    }

    private void showGmsErrorDialog(int gmsErrorCode) {
        if (mGooglePlayServicesUpdateErrorHandler != null
                && mGooglePlayServicesUpdateErrorHandler.isShowing()) {
            return;
        }
        boolean cancelable = !SigninManager.get().isForceSigninEnabled();
        mGooglePlayServicesUpdateErrorHandler =
                new UserRecoverableErrorHandler.ModalDialog(getActivity(), cancelable);
        mGooglePlayServicesUpdateErrorHandler.handleError(getActivity(), gmsErrorCode);
    }

    private void showGmsUpdatingDialog() {
        if (mGmsIsUpdatingDialog != null) {
            return;
        }
        // TODO(https://crbug.com/814728): Use DialogFragment here.
        mGmsIsUpdatingDialog = new AlertDialog.Builder(getActivity())
                                       .setCancelable(false)
                                       .setView(R.layout.updating_gms_progress_view)
                                       .create();
        mGmsIsUpdatingDialog.show();
        mGmsIsUpdatingDialogShowTime = SystemClock.elapsedRealtime();
    }

    private void dismissGmsErrorDialog() {
        if (mGooglePlayServicesUpdateErrorHandler == null) {
            return;
        }
        mGooglePlayServicesUpdateErrorHandler.cancelDialog();
        mGooglePlayServicesUpdateErrorHandler = null;
    }

    private void dismissGmsUpdatingDialog() {
        if (mGmsIsUpdatingDialog == null) {
            return;
        }
        mGmsIsUpdatingDialog.dismiss();
        mGmsIsUpdatingDialog = null;
        RecordHistogram.recordTimesHistogram("Signin.AndroidGmsUpdatingDialogShownTime",
                SystemClock.elapsedRealtime() - mGmsIsUpdatingDialogShowTime,
                TimeUnit.MILLISECONDS);
    }
}
