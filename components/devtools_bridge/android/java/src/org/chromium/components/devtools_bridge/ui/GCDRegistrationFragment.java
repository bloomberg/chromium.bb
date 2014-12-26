// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.ui;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.app.Activity;
import android.app.Fragment;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import org.chromium.components.devtools_bridge.DevToolsBridgeServer;
import org.chromium.components.devtools_bridge.apiary.ApiaryClientFactory;
import org.chromium.components.devtools_bridge.apiary.OAuthResult;
import org.chromium.components.devtools_bridge.gcd.InstanceCredential;
import org.chromium.components.devtools_bridge.gcd.InstanceDescription;

import java.io.IOException;

/**
 * Fragment that responsible for:
 * 1. Displaying GCD registration status.
 * 2. Instance registration.
 * 3. Instance unregistration.
 *
 * Fragment is abstract and does not provide UI controls. Descendant is responsible for it.
 * It also should have actionable item for registration/unregistration which invokes
 * appropriate methods.
 */
public abstract class GCDRegistrationFragment extends Fragment
        implements SharedPreferences.OnSharedPreferenceChangeListener {
    private static final String TAG = "GCDRegistrationFragment";
    private static final String PREF_OWNER_EMAIL = "ui.OWNER_EMAIL";
    private static final String PREF_DISPLAY_NAME = "ui.OWNER_EMAIL";
    private static final int CODE_ACCOUNT_SELECTED = 1;

    private ApiaryClientFactory mClientFactory;
    private Account mSelectedAccount = null;

    private SharedPreferences mPreferences;

    private InstanceCredential mInstanceCredential;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mClientFactory = new ApiaryClientFactory();
        mPreferences = DevToolsBridgeServer.getPreferences(getActivity());
        mPreferences.registerOnSharedPreferenceChangeListener(this);
        mInstanceCredential = InstanceCredential.get(mPreferences);
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mPreferences.unregisterOnSharedPreferenceChangeListener(this);
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected final Void doInBackground(Void... args) {
                mClientFactory.close();
                return null;
            }
        }.execute();
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
        if (key.equals(InstanceCredential.PREF_ID)) {
            mInstanceCredential = InstanceCredential.get(mPreferences);
            onRegistrationStatusChange();
        }
    }

    public void register() {
        AccountManager manager = AccountManager.get(getActivity());

        Intent intent = manager.newChooseAccountIntent(
                mSelectedAccount,
                null /* allowableAccounts */,
                new String[] { "com.google" } /* allowableAccountTypes */,
                true /* alwaysPromptForAccount */,
                "Registration in GCD" /* descriptionOverrideText */,
                null /* addAccountAuthTokenType */,
                null /* addAccountRequiredFeatures */,
                null /* addAccountOptions */);
        startActivityForResult(intent, CODE_ACCOUNT_SELECTED);
    }

    public void unregister() {
        if (mInstanceCredential == null) return;

        new RegistrationTask("Unregistering instance") {
            private InstanceCredential mInstranceCredentialCopy = mInstanceCredential;

            @Override
            protected void doInBackgroundImpl() throws IOException, InterruptedException {
                OAuthResult result = mClientFactory.newOAuthClient()
                        .authenticate(mInstranceCredentialCopy.secret);

                checkInterrupted();

                Log.d(TAG, "Access token: " + result.accessToken);

                mClientFactory.newGCDClient(result.accessToken)
                        .deleteInstance(mInstranceCredentialCopy.id);
            }

            @Override
            protected void onSuccess() {
                saveRegistration(null, null, null);
                onRegistrationStatusChange();
                showToast("Unregistered");
            }

            @Override
            protected void onFailure(Exception e) {
                Log.e(TAG, "Unregistration failed", e);
                showToast("Unregistration failed. See log for details.");
            }
        }.execute();
    }

    public boolean isRegistered() {
        return mInstanceCredential != null;
    }

    public String getOwner() {
        return mPreferences.getString(PREF_OWNER_EMAIL, "");
    }

    public String getDisplayName() {
        return mPreferences.getString(PREF_DISPLAY_NAME, "");
    }

    public void queryOAuthToken() {
        if (mSelectedAccount == null) return;

        AccountManager manager = AccountManager.get(getActivity());

        final String ownerEmail = mSelectedAccount.name;

        manager.getAuthToken(
                mSelectedAccount,
                "oauth2:" + ApiaryClientFactory.OAUTH_SCOPE, null /* options */, getActivity(),
                new AccountManagerCallback<Bundle>() {
                    @Override
                    public void run(AccountManagerFuture<Bundle> future) {
                        try {
                            String token = future.getResult().getString(
                                    AccountManager.KEY_AUTHTOKEN);
                            register(ownerEmail, token);
                        } catch (Exception e) {
                            Log.e(TAG, "Failed to get token: ", e);
                        }
                    }
                }, null);
    }

    private void register(final String ownerEmail, final String oAuthToken) {
        new RegistrationTask("Registering instance") {
            private Context mContext;

            private String mDisplayName;
            private InstanceDescription mDescription;
            private InstanceCredential mCredential;

            @Override
            protected void onPreExecute() {
                mContext = getActivity();

                mDisplayName = generateDisplayName();
                super.onPreExecute();
            }

            @Override
            protected void doInBackgroundImpl() throws IOException, InterruptedException {
                String ticketId = mClientFactory.newGCDClient(oAuthToken)
                        .createRegistrationTicket();

                checkInterrupted();

                String gcmChannelId =
                        mClientFactory.newGCMRegistrar().blockingGetRegistrationId(mContext);

                mDescription = new InstanceDescription.Builder()
                        .setOAuthClientId(mClientFactory.getOAuthClientId())
                        .setGCMChannelId(gcmChannelId)
                        .setDisplayName(mDisplayName)
                        .build();

                mClientFactory.newAnonymousGCDClient().patchRegistrationTicket(
                        ticketId, mDescription);

                checkInterrupted();

                mCredential = mClientFactory.newAnonymousGCDClient().finalizeRegistration(ticketId);
            }

            @Override
            protected void onSuccess() {
                saveRegistration(mDescription, mCredential, ownerEmail);
                onRegistrationStatusChange();
                showToast("Registered");
            }

            @Override
            protected void onFailure(Exception e) {
                Log.e(TAG, "Registration failed", e);
                showToast("Registration failed. See log for details.");
            }
        }.execute();
    }

    @Override
    public void onActivityResult(final int requestCode, final int resultCode, final Intent data) {
        if (requestCode == CODE_ACCOUNT_SELECTED && resultCode == Activity.RESULT_OK) {
            mSelectedAccount = new Account(
                    data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME),
                    data.getStringExtra(AccountManager.KEY_ACCOUNT_TYPE));
            Log.d(TAG, "Selected account=" + mSelectedAccount);
            queryOAuthToken();
        }
    }

    private void saveRegistration(
            InstanceDescription description, InstanceCredential credential, String ownerEmail) {
        // TODO(serya): Make registration persistant.
        mInstanceCredential = credential;

        SharedPreferences.Editor editor = mPreferences.edit();
        if (description != null && credential != null && ownerEmail != null) {
            credential.put(editor);
            editor.putString(PREF_DISPLAY_NAME, description.displayName);
            editor.putString(PREF_OWNER_EMAIL, ownerEmail);
        } else {
            InstanceCredential.remove(editor);
            editor.remove(PREF_DISPLAY_NAME);
            editor.remove(PREF_OWNER_EMAIL);
        }
        editor.commit();
    }

    protected abstract void onRegistrationStatusChange();
    protected abstract String generateDisplayName();

    private abstract class RegistrationTask extends AsyncTask<Void, Void, Boolean> {
        private ProgressDialog mDialog;
        private Exception mException;

        private final String mProgressMessage;

        protected RegistrationTask(String progressMessage) {
            mProgressMessage = progressMessage;
        }

        @Override
        protected void onPreExecute() {
            mDialog = ProgressDialog.show(
                    getActivity(),
                    "GCD registration",
                    mProgressMessage,
                    true,
                    false,
                    new DialogInterface.OnCancelListener() {
                        @Override
                        public void onCancel(DialogInterface dialog) {
                            cancel(true);
                        }
                    });
        }

        @Override
        protected final Boolean doInBackground(Void... args) {
            try {
                doInBackgroundImpl();
                return Boolean.TRUE;
            } catch (IOException e) {
                mException = e;
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            return Boolean.FALSE;
        }

        protected final void checkInterrupted() throws InterruptedException {
            if (Thread.currentThread().isInterrupted()) {
                throw new InterruptedException();
            }
        }

        @Override
        protected void onPostExecute(Boolean success) {
            mDialog.dismiss();
            if (Boolean.TRUE.equals(success)) {
                onSuccess();
            } else if (mException != null) {
                onFailure(mException);
            }
        }

        protected void showToast(String message) {
            Toast.makeText(getActivity(), message, Toast.LENGTH_SHORT).show();
        }

        protected abstract void doInBackgroundImpl() throws IOException, InterruptedException;
        protected abstract void onSuccess();
        protected abstract void onFailure(Exception e);
    }
}
