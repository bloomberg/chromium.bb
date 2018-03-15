// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Fragment;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;

import org.chromium.chrome.R;

/**
 * This fragment implements sign-in screen with account picker and descriptions of signin-related
 * features. Configuration for this fragment is provided by overriding {@link #getSigninArguments}
 * derived classes.
 */
public abstract class SigninFragmentBase extends Fragment {
    private static final String TAG = "SigninFragmentBase";

    private static final String ARGUMENT_ACCESS_POINT = "SigninFragmentBase.AccessPoint";

    private @SigninAccessPoint int mSigninAccessPoint;
    private @StringRes int mCancelButtonTextId = R.string.cancel;

    /**
     * Creates an argument bundle to start AccountSigninView from the account selection page.
     * @param accessPoint The access point for starting signin flow.
     */
    protected static Bundle createArguments(@SigninAccessPoint int accessPoint) {
        Bundle result = new Bundle();
        result.putInt(ARGUMENT_ACCESS_POINT, accessPoint);
        return result;
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
}
