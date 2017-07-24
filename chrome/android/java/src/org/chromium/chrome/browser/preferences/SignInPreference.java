// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.accounts.Account;
import android.content.Context;
import android.graphics.Bitmap;
import android.preference.Preference;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileDownloader;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.chrome.browser.signin.AccountSigninActivity;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.signin.SigninPromoView;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService.SyncStateChangedListener;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;

/**
 * A preference that displays "Sign in to Chrome" when the user is not sign in, and displays
 * the user's name, email, profile image and sync error icon if necessary when the user is signed
 * in.
 */
public class SignInPreference extends Preference
        implements SignInAllowedObserver, ProfileDownloader.Observer,
                   AndroidSyncSettings.AndroidSyncSettingsObserver, SyncStateChangedListener {
    private boolean mShowingPromo;
    private boolean mViewEnabled;
    private SigninPromoController mSigninPromoController;

    /**
     * Constructor for inflating from XML.
     */
    public SignInPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        update();
    }

    /**
     * Starts listening for updates to the sign-in and sync state.
     */
    public void registerForUpdates() {
        SigninManager manager = SigninManager.get(getContext());
        manager.addSignInAllowedObserver(this);
        ProfileDownloader.addObserver(this);
        FirstRunSignInProcessor.updateSigninManagerFirstRunCheckDone(getContext());
        AndroidSyncSettings.registerObserver(getContext(), this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
    }

    /**
     * Stops listening for updates to the sign-in and sync state. Every call to registerForUpdates()
     * must be matched with a call to this method.
     */
    public void unregisterForUpdates() {
        SigninManager manager = SigninManager.get(getContext());
        manager.removeSignInAllowedObserver(this);
        ProfileDownloader.removeObserver(this);
        AndroidSyncSettings.unregisterObserver(getContext(), this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
    }

    /**
     * Updates the title, summary, and image based on the current sign-in state.
     */
    private void update() {
        String accountName = ChromeSigninController.get().getSignedInAccountName();
        if (SigninManager.get(getContext()).isSigninDisabledByPolicy()) {
            setupSigninDisabled();
        } else if (accountName == null) {
            if (SigninPromoController.shouldShowPromo()) {
                setupNewPromo();
            } else {
                setupOldPromo();
            }
        } else {
            setupSignedIn(accountName);
        }

        setOnPreferenceClickListener(new OnPreferenceClickListener() {
            @Override
            public boolean onPreferenceClick(Preference preference) {
                return AccountSigninActivity.startIfAllowed(
                        getContext(), SigninAccessPoint.SETTINGS);
            }
        });
    }

    private void setupSigninDisabled() {
        setLayoutResource(R.layout.account_management_account_row);
        setTitle(R.string.sign_in_to_chrome);
        setSummary(R.string.sign_in_to_chrome_disabled_summary);
        setFragment(null);
        setIcon(ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        setWidgetLayoutResource(0);
        setViewEnabled(false);
        mSigninPromoController = null;
        mShowingPromo = false;
    }

    private void setupNewPromo() {
        setLayoutResource(R.layout.custom_preference);
        setTitle("");
        setSummary("");
        setFragment(null);
        setIcon(null);
        setWidgetLayoutResource(R.layout.signin_promo_view);
        setViewEnabled(true);

        Account[] accounts = AccountManagerFacade.get().tryGetGoogleAccounts();
        String defaultAccountName = accounts.length == 0 ? null : accounts[0].name;

        if (mSigninPromoController == null) {
            ProfileDataCache profileDataCache =
                    new ProfileDataCache(getContext(), Profile.getLastUsedProfile());
            mSigninPromoController =
                    new SigninPromoController(profileDataCache, SigninAccessPoint.SETTINGS);
            mSigninPromoController.setAccountName(defaultAccountName);
            mSigninPromoController.recordSigninPromoImpression();
        } else {
            mSigninPromoController.setAccountName(defaultAccountName);
        }

        mShowingPromo = true;

        notifyChanged();
    }

    private void setupOldPromo() {
        setLayoutResource(R.layout.account_management_account_row);
        setTitle(R.string.sign_in_to_chrome);
        setSummary(R.string.sign_in_to_chrome_summary);
        setFragment(null);
        setIcon(AppCompatResources.getDrawable(getContext(), R.drawable.logo_avatar_anonymous));
        setWidgetLayoutResource(0);
        setViewEnabled(true);
        mSigninPromoController = null;

        if (!mShowingPromo) {
            RecordUserAction.record("Signin_Impression_FromSettings");
        }

        mShowingPromo = true;
    }

    private void setupSignedIn(String accountName) {
        setLayoutResource(R.layout.account_management_account_row);
        String title = AccountManagementFragment.getCachedUserName(accountName);
        if (title == null) {
            Profile profile = Profile.getLastUsedProfile();
            String cachedName = ProfileDownloader.getCachedFullName(profile);
            Bitmap cachedBitmap = ProfileDownloader.getCachedAvatar(profile);
            if (TextUtils.isEmpty(cachedName) || cachedBitmap == null) {
                AccountManagementFragment.startFetchingAccountInformation(
                        getContext(), profile, accountName);
            }
            title = TextUtils.isEmpty(cachedName) ? accountName : cachedName;
        }
        setTitle(title);
        setSummary(SyncPreference.getSyncStatusSummary(getContext()));
        setFragment(AccountManagementFragment.class.getName());
        setIcon(AccountManagementFragment.getUserPicture(getContext(), accountName));
        setWidgetLayoutResource(
                SyncPreference.showSyncErrorIcon(getContext()) ? R.layout.sync_error_widget : 0);
        setViewEnabled(true);
        mSigninPromoController = null;
        mShowingPromo = false;
    }

    // This just changes visual representation. Actual enabled flag in preference stays
    // always true to receive clicks (necessary to show "Managed by administator" toast).
    private void setViewEnabled(boolean enabled) {
        if (mViewEnabled == enabled) {
            return;
        }
        mViewEnabled = enabled;
        notifyChanged();
    }

    @Override
    protected void onBindView(final View view) {
        super.onBindView(view);
        ViewUtils.setEnabledRecursive(view, mViewEnabled);
        if (mSigninPromoController != null) {
            SigninPromoView signinPromoView = view.findViewById(R.id.signin_promo_view_container);
            mSigninPromoController.setupSigninPromoView(getContext(), signinPromoView, null);
        }
    }

    // ProfileSyncServiceListener implementation:

    @Override
    public void syncStateChanged() {
        update();
    }

    // SignInAllowedObserver

    @Override
    public void onSignInAllowedChanged() {
        update();
    }

    // ProfileDownloader.Observer

    @Override
    public void onProfileDownloaded(String accountId, String fullName, String givenName,
            Bitmap bitmap) {
        AccountManagementFragment.updateUserNamePictureCache(
                getContext(), accountId, fullName, bitmap);
        update();
    }

    // AndroidSyncSettings.AndroidSyncSettingsObserver
    @Override
    public void androidSyncSettingsChanged() {
        update();
    }
}
