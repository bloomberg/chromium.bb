// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Fragment;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.AccountSigninView;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninManager;

/**
 * A {@link Fragment} meant to handle sync setup for the first run experience.
 */
public class AccountFirstRunFragment extends FirstRunPage implements AccountSigninView.Delegate {
    // Per-page parameters:
    public static final String FORCE_SIGNIN_ACCOUNT_TO = "ForceSigninAccountTo";
    public static final String PRESELECT_BUT_ALLOW_TO_CHANGE = "PreselectButAllowToChange";
    public static final String IS_CHILD_ACCOUNT = "IsChildAccount";

    private AccountSigninView mView;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mView = (AccountSigninView) inflater.inflate(
                R.layout.account_signin_view, container, false);
        return mView;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        int imageSize = getResources().getDimensionPixelSize(R.dimen.signin_account_image_size);
        ProfileDataCache profileDataCache =
                new ProfileDataCache(getActivity(), Profile.getLastUsedProfile(), imageSize);
        boolean isChildAccount = getProperties().getBoolean(IS_CHILD_ACCOUNT);
        String forceAccountTo = getProperties().getString(FORCE_SIGNIN_ACCOUNT_TO);
        AccountSigninView.Listener listener = new AccountSigninView.Listener() {
            @Override
            public void onAccountSelectionCanceled() {
                getPageDelegate().refuseSignIn();
                advanceToNextPage();
            }

            @Override
            public void onNewAccount() {
                FirstRunUtils.openAccountAdder(AccountFirstRunFragment.this);
            }

            @Override
            public void onAccountSelected(
                    String accountName, boolean isDefaultAccount, boolean settingsClicked) {
                getPageDelegate().acceptSignIn(accountName, isDefaultAccount);
                if (settingsClicked) {
                    getPageDelegate().askToOpenSignInSettings();
                }
                advanceToNextPage();
            }

            @Override
            public void onFailedToSetForcedAccount(String forcedAccountName) {
                // Somehow the forced account disappeared while we were in the FRE.
                // The user would have to go through the FRE again.
                getPageDelegate().abortFirstRunExperience();
            }
        };

        if (forceAccountTo == null) {
            mView.initFromSelectionPage(profileDataCache, isChildAccount, this, listener);
        } else {
            mView.initFromConfirmationPage(profileDataCache, isChildAccount, forceAccountTo, false,
                    AccountSigninView.UNDO_INVISIBLE, this, listener);
        }

        RecordUserAction.record("MobileFre.SignInShown");
        RecordUserAction.record("Signin_Signin_FromStartPage");
        SigninManager.logSigninStartAccessPoint(SigninAccessPoint.START_PAGE);
    }

    // FirstRunPage:

    @Override
    public boolean interceptBackPressed() {
        boolean forceSignin = getProperties().getString(FORCE_SIGNIN_ACCOUNT_TO) != null;
        if (!mView.isInConfirmationScreen()
                || (forceSignin && !getProperties().getBoolean(PRESELECT_BUT_ALLOW_TO_CHANGE))) {
            return false;
        }

        if (forceSignin && getProperties().getBoolean(PRESELECT_BUT_ALLOW_TO_CHANGE)) {
            // Allow the user to choose the account or refuse to sign in,
            // and re-create this fragment.
            getProperties().remove(FORCE_SIGNIN_ACCOUNT_TO);
        }

        // Re-create the fragment if the user presses the back button when in signed in mode.
        // The fragment is re-created in the normal (signed out) mode.
        getPageDelegate().recreateCurrentPage();
        return true;
    }
}
