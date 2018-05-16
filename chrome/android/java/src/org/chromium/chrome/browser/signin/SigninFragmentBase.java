// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;
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
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerResult;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ChildAccountStatus;
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
    private static final String ARGUMENT_ACCOUNT_NAME = "SigninFragmentBase.AccountName";
    private static final String ARGUMENT_IS_DEFAULT_ACCOUNT = "SigninFragmentBase.IsDefaultAccount";
    private static final String ARGUMENT_CHILD_ACCOUNT_STATUS =
            "SigninFragmentBase.ChildAccountStatus";

    public static final String ACCOUNT_PICKER_DIALOG_TAG =
            "SigninFragmentBase.AccountPickerDialogFragment";

    private static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    private @SigninAccessPoint int mSigninAccessPoint;
    private boolean mForceSignin;
    private @ChildAccountStatus.Status int mChildAccountStatus;

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
    private boolean mDestroyed;
    // TODO(https://crbug.com/814728): Ignore button clicks if GMS reported an error.
    private boolean mHasGmsError;

    private UserRecoverableErrorHandler.ModalDialog mGooglePlayServicesUpdateErrorHandler;
    private AlertDialog mGmsIsUpdatingDialog;
    private long mGmsIsUpdatingDialogShowTime;
    private ConfirmSyncDataStateMachine mConfirmSyncDataStateMachine;

    /**
     * Creates an argument bundle for the default SigninFragmentBase flow (default account will be
     * selected, account selection is enabled, etc.).
     * @param accessPoint The access point for starting signin flow.
     */
    protected static Bundle createArguments(@SigninAccessPoint int accessPoint) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        return result;
    }

    /**
     * Creates an argument bundle for a custom SigninFragmentBase flow.
     * @param accessPoint The access point for starting signin flow.
     * @param accountName The account to preselect.
     * @param isDefaultAccount Whether the selected account is the default one.
     * @param childAccountStatus Whether the selected account is a child one.
     */
    protected static Bundle createArgumentsForForcedSigninFlow(@SigninAccessPoint int accessPoint,
            String accountName, boolean isDefaultAccount,
            @ChildAccountStatus.Status int childAccountStatus) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        result.putString(ARGUMENT_ACCOUNT_NAME, accountName);
        result.putBoolean(ARGUMENT_IS_DEFAULT_ACCOUNT, isDefaultAccount);
        result.putInt(ARGUMENT_CHILD_ACCOUNT_STATUS, childAccountStatus);
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

    /** Returns whether this fragment is in "force sign-in" mode. */
    protected boolean isForcedSignin() {
        return mForceSignin;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Bundle arguments = getSigninArguments();
        initAccessPoint(arguments.getInt(ARGUMENT_ACCESS_POINT, -1));

        mSelectedAccountName = arguments.getString(ARGUMENT_ACCOUNT_NAME, null);
        mIsDefaultAccountSelected = arguments.getBoolean(ARGUMENT_IS_DEFAULT_ACCOUNT, false);
        mChildAccountStatus =
                arguments.getInt(ARGUMENT_CHILD_ACCOUNT_STATUS, ChildAccountStatus.NOT_CHILD);
        mForceSignin = mSelectedAccountName != null;

        mConsentTextTracker = new ConsentTextTracker(getResources());

        ProfileDataCache.BadgeConfig badgeConfig = null;
        if (ChildAccountStatus.isChild(mChildAccountStatus)) {
            Bitmap badge =
                    BitmapFactory.decodeResource(getResources(), R.drawable.ic_account_child_20dp);
            int badgePositionX = getResources().getDimensionPixelOffset(R.dimen.badge_position_x);
            int badgePositionY = getResources().getDimensionPixelOffset(R.dimen.badge_position_y);
            int badgeBorderSize = getResources().getDimensionPixelSize(R.dimen.badge_border_size);
            badgeConfig = new ProfileDataCache.BadgeConfig(
                    badge, new Point(badgePositionX, badgePositionY), badgeBorderSize);
        }
        mProfileDataCache = new ProfileDataCache(getActivity(),
                getResources().getDimensionPixelSize(R.dimen.user_picture_size), badgeConfig);

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
        if (mConfirmSyncDataStateMachine != null) {
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed = */ true);
            mConfirmSyncDataStateMachine = null;
        }
        mDestroyed = true;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mView = (SigninView) inflater.inflate(R.layout.signin_view, container, false);

        mView.getAccountPickerView().setOnClickListener(view -> onAccountPickerClicked());

        mView.getRefuseButton().setOnClickListener(this::onRefuseButtonClicked);
        mView.getAcceptButton().setVisibility(View.GONE);
        mView.getAcceptButton().setOnClickListener(this::onAcceptButtonClicked);
        mView.getMoreButton().setVisibility(View.VISIBLE);
        mView.getMoreButton().setOnClickListener(view -> {
            mView.getScrollView().smoothScrollBy(0, mView.getScrollView().getHeight());
            // TODO(https://crbug.com/821127): Revise this user action.
            RecordUserAction.record("Signin_MoreButton_Shown");
        });
        mView.getScrollView().setScrolledToBottomObserver(this::showAcceptButton);

        if (mForceSignin) {
            mView.getAccountPickerEndImageView().setImageResource(
                    R.drawable.ic_check_googblue_24dp);
            mView.getAccountPickerEndImageView().setAlpha(1.0f);
            mView.getRefuseButton().setVisibility(View.GONE);
            mView.getAcceptButtonEndPadding().setVisibility(View.INVISIBLE);
        }

        updateConsentText();
        return mView;
    }

    private void updateConsentText() {
        // TODO(https://crbug.com/814728): Change texts if mIsChildAccount is true.
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
                onSettingsLinkClicked(widget);
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

        final String fullName = profileData.getFullName();
        if (!TextUtils.isEmpty(fullName)) {
            mConsentTextTracker.setTextNonRecordable(mView.getAccountTextPrimary(), fullName);
            mConsentTextTracker.setTextNonRecordable(
                    mView.getAccountTextSecondary(), profileData.getAccountName());
            mView.getAccountTextSecondary().setVisibility(View.VISIBLE);
        } else {
            // Full name is not available, show the email in the primary TextView.
            mConsentTextTracker.setTextNonRecordable(
                    mView.getAccountTextPrimary(), profileData.getAccountName());
            mView.getAccountTextSecondary().setVisibility(View.GONE);
        }
    }

    private void showAcceptButton() {
        mView.getAcceptButton().setVisibility(View.VISIBLE);
        mView.getMoreButton().setVisibility(View.GONE);
        mView.getScrollView().setScrolledToBottomObserver(null);
    }

    private void onAccountPickerClicked() {
        // TODO(https://crbug.com/814728): Ignore clicks if GMS reported an error.
        showAccountPicker();
    }

    private void onRefuseButtonClicked(View button) {
        // TODO(https://crbug.com/814728): Disable controls.
        onSigninRefused();
    }

    private void onAcceptButtonClicked(View button) {
        // TODO(https://crbug.com/814728): Disable controls.
        RecordUserAction.record("Signin_Signin_WithDefaultSyncSettings");

        // Record the fact that the user consented to the consent text by clicking on a button
        recordConsent((TextView) button);
        seedAccountsAndSignin(false);
    }

    private void onSettingsLinkClicked(View view) {
        // TODO(https://crbug.com/814728): Disable controls.
        RecordUserAction.record("Signin_Signin_WithAdvancedSyncSettings");

        // Record the fact that the user consented to the consent text by clicking on a link
        recordConsent((TextView) view);
        seedAccountsAndSignin(true);
    }

    private void seedAccountsAndSignin(boolean settingsClicked) {
        // Ensure that the AccountTrackerService has a fully up to date GAIA id <-> email mapping,
        // as this is needed for the previous account check.
        final long seedingStartTime = SystemClock.elapsedRealtime();
        if (AccountTrackerService.get().checkAndSeedSystemAccounts()) {
            recordAccountTrackerServiceSeedingTime(seedingStartTime);
            runStateMachineAndSignin(settingsClicked);
            return;
        }

        AccountTrackerService.OnSystemAccountsSeededListener listener =
                new AccountTrackerService.OnSystemAccountsSeededListener() {
                    @Override
                    public void onSystemAccountsSeedingComplete() {
                        AccountTrackerService.get().removeSystemAccountsSeededListener(this);
                        recordAccountTrackerServiceSeedingTime(seedingStartTime);

                        // Don't start sign-in if this fragment has been destroyed.
                        if (mDestroyed) return;
                        runStateMachineAndSignin(settingsClicked);
                    }

                    @Override
                    public void onSystemAccountsChanged() {}
                };
        AccountTrackerService.get().addSystemAccountsSeededListener(listener);
    }

    private void runStateMachineAndSignin(boolean settingsClicked) {
        mConfirmSyncDataStateMachine = new ConfirmSyncDataStateMachine(getContext(),
                getChildFragmentManager(),
                ConfirmImportSyncDataDialog.ImportSyncType.PREVIOUS_DATA_FOUND,
                PrefServiceBridge.getInstance().getSyncLastAccountName(), mSelectedAccountName,
                new ConfirmImportSyncDataDialog.Listener() {
                    @Override
                    public void onConfirm(boolean wipeData) {
                        mConfirmSyncDataStateMachine = null;

                        // Don't start sign-in if this fragment has been destroyed.
                        if (mDestroyed) return;
                        SigninManager.wipeSyncUserDataIfRequired(wipeData).then((Void v) -> {
                            onSigninAccepted(mSelectedAccountName, mIsDefaultAccountSelected,
                                    settingsClicked);
                        });
                    }

                    @Override
                    public void onCancel() {
                        mConfirmSyncDataStateMachine = null;
                        // TODO(https://crbug.com/814728): Re-enable controls.
                    }
                });
    }

    private static void recordAccountTrackerServiceSeedingTime(long seedingStartTime) {
        RecordHistogram.recordTimesHistogram("Signin.AndroidAccountSigninViewSeedingTime",
                SystemClock.elapsedRealtime() - seedingStartTime, TimeUnit.MILLISECONDS);
    }

    /**
     * Records the Sync consent.
     * @param confirmationView The view that the user clicked when consenting.
     */
    private void recordConsent(TextView confirmationView) {
        // TODO(crbug.com/831257): Provide the account id synchronously from AccountManagerFacade.
        final AccountIdProvider accountIdProvider = AccountIdProvider.getInstance();
        new AsyncTask<Void, Void, String>() {
            @Override
            public String doInBackground(Void... params) {
                return accountIdProvider.getAccountId(mSelectedAccountName);
            }

            @Override
            public void onPostExecute(String accountId) {
                mConsentTextTracker.recordConsent(
                        accountId, ConsentAuditorFeature.CHROME_SYNC, confirmationView, mView);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void showAccountPicker() {
        // Account picker is already shown
        if (getAccountPickerDialogFragment() != null) return;

        AccountPickerDialogFragment dialog =
                AccountPickerDialogFragment.create(mSelectedAccountName);
        FragmentTransaction transaction = getChildFragmentManager().beginTransaction();
        transaction.add(dialog, ACCOUNT_PICKER_DIALOG_TAG);
        transaction.commit();
    }

    private AccountPickerDialogFragment getAccountPickerDialogFragment() {
        return (AccountPickerDialogFragment) getChildFragmentManager().findFragmentByTag(
                ACCOUNT_PICKER_DIALOG_TAG);
    }

    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount) {
        selectAccount(accountName, isDefaultAccount);
    }

    @Override
    public void addAccount() {
        RecordUserAction.record("Signin_AddAccountToDevice");
        // TODO(https://crbug.com/842860): Revise createAddAccountIntent and AccountAdder.
        AccountManagerFacade.get().createAddAccountIntent((@Nullable Intent intent) -> {
            if (intent != null) {
                startActivityForResult(intent, ADD_ACCOUNT_REQUEST_CODE);
                return;
            }

            // AccountManagerFacade couldn't create intent, use AccountAdder as fallback.
            AccountAdder.getInstance().addAccount(this, ADD_ACCOUNT_REQUEST_CODE);
        });
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == ADD_ACCOUNT_REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            AccountPickerDialogFragment accountPickerFragment = getAccountPickerDialogFragment();
            if (accountPickerFragment != null) {
                accountPickerFragment.dismiss();
            }
            // TODO(https://crbug.com/814728): Select the added account.
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        mResumed = true;
        AccountManagerFacade.get().addObserver(mAccountsChangedObserver);
        mProfileDataCache.addObserver(mProfileDataCacheObserver);
        triggerUpdateAccounts();

        mView.startAnimations();
    }

    @Override
    public void onPause() {
        super.onPause();
        mResumed = false;
        mProfileDataCache.removeObserver(mProfileDataCacheObserver);
        AccountManagerFacade.get().removeObserver(mAccountsChangedObserver);

        mView.stopAnimations();
    }

    private void selectAccount(String accountName, boolean isDefaultAccount) {
        mSelectedAccountName = accountName;
        mIsDefaultAccountSelected = isDefaultAccount;
        mProfileDataCache.update(Collections.singletonList(mSelectedAccountName));
        updateProfileData();

        AccountPickerDialogFragment accountPickerFragment = getAccountPickerDialogFragment();
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

        if (mConfirmSyncDataStateMachine != null) {
            // Any dialogs that may have been showing are now invalid (they were created
            // for the previously selected account).
            mConfirmSyncDataStateMachine.cancel(/* isBeingDestroyed = */ false);
            mConfirmSyncDataStateMachine = null;
        }

        // Account for forced sign-in flow disappeared before the sign-in was completed.
        if (mForceSignin) {
            onSigninRefused();
            return;
        }

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
