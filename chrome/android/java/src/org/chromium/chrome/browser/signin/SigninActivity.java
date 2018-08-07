// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.SynchronousInitializationActivity;

/**
 * Allows user to pick an account and sign in. Started from Settings and various sign-in promos.
 */
// TODO(https://crbug.com/820491): extend AsyncInitializationActivity.
public class SigninActivity extends SynchronousInitializationActivity {
    private static final String TAG = "SigninActivity";
    private static final String ARGUMENT_FRAGMENT_NAME = "SigninActivity.FragmentName";
    private static final String ARGUMENT_FRAGMENT_ARGS = "SigninActivity.FragmentArgs";

    /**
     * Creates an {@link Intent} which can be used to start sign-in flow.
     * @param accessPoint {@link AccessPoint} for starting sign-in flow. Used in metrics.
     */
    public static Intent createIntent(
            Context context, @AccountSigninActivity.AccessPoint int accessPoint) {
        return createIntentInternal(
                context, SigninFragment.class, SigninFragment.createArguments(accessPoint));
    }

    /**
     * Creates an argument bundle to start default sign-in flow from personalized sign-in promo.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Intent createIntentForPromoDefaultFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        return createIntentInternal(context, SigninFragment.class,
                SigninFragment.createArgumentsForPromoDefaultFlow(accessPoint, accountName));
    }

    /**
     * Creates an argument bundle to start "Choose account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint The access point for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    public static Intent createIntentForPromoChooseAccountFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        return createIntentInternal(context, SigninFragment.class,
                SigninFragment.createArgumentsForPromoChooseAccountFlow(accessPoint, accountName));
    }

    /**
     * Creates an argument bundle to start "New account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint The access point for starting sign-in flow.
     */
    public static Intent createIntentForPromoAddAccountFlow(
            Context context, @SigninAccessPoint int accessPoint) {
        return createIntentInternal(context, SigninFragment.class,
                SigninFragment.createArgumentsForPromoAddAccountFlow(accessPoint));
    }

    /**
     * Creates an {@link Intent} which can be used to start consent bump flow.
     * @param accountName The name of the signed in account.
     */
    public static Intent createIntentForConsentBump(Context context, String accountName) {
        return createIntentInternal(context, ConsentBumpFragment.class,
                ConsentBumpFragment.createArguments(accountName));
    }

    private static Intent createIntentInternal(
            Context context, Class<? extends Fragment> fragmentName, Bundle fragmentArgs) {
        Intent intent = new Intent(context, SigninActivity.class);
        intent.putExtra(ARGUMENT_FRAGMENT_NAME, fragmentName.getName());
        intent.putExtra(ARGUMENT_FRAGMENT_ARGS, fragmentArgs);
        intent.putExtras(fragmentArgs);
        return intent;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.signin_activity);

        FragmentManager fragmentManager = getSupportFragmentManager();
        Fragment fragment = fragmentManager.findFragmentById(R.id.fragment_container);
        if (fragment == null) {
            String fragmentName = getIntent().getStringExtra(ARGUMENT_FRAGMENT_NAME);
            Bundle fragmentArgs = getIntent().getBundleExtra(ARGUMENT_FRAGMENT_ARGS);
            fragment = Fragment.instantiate(this, fragmentName, fragmentArgs);
            fragmentManager.beginTransaction().add(R.id.fragment_container, fragment).commit();
        }
    }
}
