// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.ui;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.app.Activity;
import android.app.ListFragment;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Toast;

import org.chromium.components.devtools_bridge.RTCConfiguration;
import org.chromium.components.devtools_bridge.SessionBase;
import org.chromium.components.devtools_bridge.apiary.ApiaryClientFactory;
import org.chromium.components.devtools_bridge.apiary.TestApiaryClientFactory;
import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.CommandSender;
import org.chromium.components.devtools_bridge.gcd.RemoteInstance;

import java.io.IOException;
import java.util.List;

/**
 * Fragment of application for manual testing the DevTools bridge. Shows instances
 * registered in GCD and lets unregister them.
 */
public abstract class RemoteInstanceListFragment extends ListFragment {
    private static final String TAG = "RemoteInstanceListFragment";
    private static final String NO_ID = "";
    private static final int CODE_ACCOUNT_SELECTED = 1;

    private final TestApiaryClientFactory mClientFactory = new TestApiaryClientFactory();
    private ArrayAdapter<RemoteInstance> mListAdapter;
    private String mOAuthToken;
    private RemoteInstance mSelected;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mListAdapter = new ArrayAdapter<RemoteInstance>(
                getActivity(), android.R.layout.simple_list_item_1);
        new UpdateListAction().execute();
        setListAdapter(mListAdapter);
    }

    @Override
    public void onDestroy() {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected final Void doInBackground(Void... args) {
                mClientFactory.close();
                return null;
            }
        }.execute();
        super.onDestroy();
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View root = super.onCreateView(inflater, container, savedInstanceState);
        registerForContextMenu(root);
        return root;
    }

    @Override
    public void onListItemClick(ListView listView, View view, int position, long id) {
        mSelected = mListAdapter.getItem(position);
        if (mOAuthToken == null) {
            signIn();
        } else {
            getActivity().openContextMenu(view);
        }
    }

    @Override
    public void onCreateContextMenu(
            ContextMenu menu, View view, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, view, menuInfo);

        if (mOAuthToken == null) return;

        menu.add("Connect")
                .setOnMenuItemClickListener(new ConnectAction())
                .setEnabled(mSelected != null);
        menu.add("Send invalid offer")
                .setOnMenuItemClickListener(new SendInvalidOfferAction())
                .setEnabled(mSelected != null);
        menu.add("Delete")
                .setOnMenuItemClickListener(new DeleteInstanceAction(mSelected))
                .setEnabled(mSelected != null);
        menu.add("Update list")
                .setOnMenuItemClickListener(new UpdateListAction());
        menu.add("Delete all")
                .setOnMenuItemClickListener(new DeleteAllAction());
    }

    public void signIn() {
        AccountManager manager = AccountManager.get(getActivity());

        Intent intent = manager.newChooseAccountIntent(
                null /* selectedAccount */,
                null /* allowableAccounts */,
                new String[] { "com.google" } /* allowableAccountTypes */,
                true /* alwaysPromptForAccount */,
                "Sign into GCD" /* descriptionOverrideText */,
                null /* addAccountAuthTokenType */,
                null /* addAccountRequiredFeatures */,
                null /* addAccountOptions */);
        startActivityForResult(intent, CODE_ACCOUNT_SELECTED);
    }

    @Override
    public void onActivityResult(final int requestCode, final int resultCode, final Intent data) {
        if (requestCode == CODE_ACCOUNT_SELECTED && resultCode == Activity.RESULT_OK) {
            queryOAuthToken(new Account(
                    data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME),
                    data.getStringExtra(AccountManager.KEY_ACCOUNT_TYPE)));
        }
    }

    protected abstract void connect(String oAuthToken, String remoteInstanceId);

    public void queryOAuthToken(Account accout) {
        AccountManager manager = AccountManager.get(getActivity());

        manager.getAuthToken(
                accout,
                "oauth2:" + ApiaryClientFactory.OAUTH_SCOPE, null /* options */, getActivity(),
                new AccountManagerCallback<Bundle>() {
                    @Override
                    public void run(AccountManagerFuture<Bundle> future) {
                        try {
                            mOAuthToken = future.getResult().getString(
                                    AccountManager.KEY_AUTHTOKEN);
                            updateList();
                        } catch (Exception e) {
                            Log.d(TAG, "Failed to get token: ", e);
                        }
                    }
                }, null);
    }

    public void updateList() {
        new UpdateListAction().execute();
    }

    private final class ConnectAction implements MenuItem.OnMenuItemClickListener {
        @Override
        public boolean onMenuItemClick(MenuItem item) {
            if (mOAuthToken != null && mSelected != null) {
                connect(mOAuthToken, mSelected.id);
                return true;
            }
            return false;
        }
    }

    private abstract class AsyncAction extends AsyncTask<Void, Void, Boolean>
            implements MenuItem.OnMenuItemClickListener {
        private final String mActionName;

        protected AsyncAction(String actionName) {
            mActionName = actionName;
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            execute();
            return true;
        }

        @Override
        protected final Boolean doInBackground(Void... args) {
            try {
                doInBackgroundImpl();
                return Boolean.TRUE;
            } catch (IOException e) {
                Log.e(TAG, mActionName + " failed", e);
                return Boolean.FALSE;
            }
        }

        @Override
        protected void onPostExecute(Boolean success) {
            if (Boolean.TRUE.equals(success)) {
                onSuccess();
                showToast(mActionName + " completed");
            } else {
                onFailure();
                showToast(mActionName + " failed");
            }
        }

        protected void showToast(String message) {
            Toast.makeText(getActivity(), message, Toast.LENGTH_SHORT).show();
        }

        protected abstract void doInBackgroundImpl() throws IOException;
        protected abstract void onSuccess();
        protected void onFailure() {}
    }

    private final class SendInvalidOfferAction extends AsyncAction {
        private final String mOAuthTokenCopy = mOAuthToken;
        private final RemoteInstance mSelectedCopy = mSelected;
        private IOException mException;

        public SendInvalidOfferAction() {
            super("Sending invalid offer");
        }

        @Override
        protected final void doInBackgroundImpl() throws IOException {
            if (mOAuthTokenCopy == null || mSelected == null) {
                throw new IOException("Action cann't be applied.");
            }

            final String sessionId = "sessionId";
            final String offer = "INVALID_OFFER";
            final RTCConfiguration config = new RTCConfiguration();

            CommandSender sender = new CommandSender() {
                @Override
                protected void send(Command command, Runnable completionCallback) {
                    try {
                        mClientFactory.newTestGCDClient(mOAuthTokenCopy)
                                .send(mSelectedCopy.id, command);
                    } catch (IOException e) {
                        mException = e;
                        command.setFailure("IO exception");
                    } finally {
                        completionCallback.run();
                    }
                }
            };

            sender.startSession(sessionId, config, offer, new SessionBase.NegotiationCallback() {
                @Override
                public void onSuccess(String answer) {
                    mException = new IOException("Unexpected success");
                }

                @Override
                public void onFailure(String errorMessage) {
                    Log.i(TAG, "Expected failure: " + errorMessage);
                }
            });

            if (mException != null)
                throw mException;
        }

        @Override
        protected void onSuccess() {
        }
    }

    private final class UpdateListAction extends AsyncAction {
        private final String mOAuthTokenCopy = mOAuthToken;
        private List<RemoteInstance> mResult;

        public UpdateListAction() {
            super("Updating instance list");
        }

        @Override
        protected final void doInBackgroundImpl() throws IOException {
            if (mOAuthTokenCopy == null) return;

            mResult = mClientFactory.newTestGCDClient(mOAuthTokenCopy).fetchInstances();
        }

        @Override
        protected void onSuccess() {
            mListAdapter.clear();
            if (mOAuthTokenCopy == null) {
                mListAdapter.add(new RemoteInstance(NO_ID, "Sign in"));
            } else if (mResult.size() > 0) {
                mListAdapter.addAll(mResult);
            } else {
                mListAdapter.add(new RemoteInstance(NO_ID, "Empty list"));
            }
        }

        @Override
        protected void onFailure() {
            mListAdapter.clear();
            mListAdapter.add(new RemoteInstance(NO_ID, "Update failed"));
        }

        @Override
        protected void showToast(String message) {}
    }

    private final class DeleteInstanceAction extends AsyncAction {
        private final String mOAuthTokenCopy = mOAuthToken;
        private final RemoteInstance mInstance;

        public DeleteInstanceAction(RemoteInstance instance) {
            super("Deleting instance");
            mInstance = instance;
        }

        @Override
        protected final void doInBackgroundImpl() throws IOException {
            mClientFactory.newTestGCDClient(mOAuthTokenCopy).deleteInstance(mInstance.id);
        }

        @Override
        protected void onSuccess() {
            updateList();
        }
    }

    private final class DeleteAllAction extends AsyncAction {
        private final String mOAuthTokenCopy = mOAuthToken;

        public DeleteAllAction() {
            super("Deleting all");
        }

        @Override
        protected final void doInBackgroundImpl() throws IOException {
            for (RemoteInstance instance :
                        mClientFactory.newTestGCDClient(mOAuthTokenCopy).fetchInstances()) {
                mClientFactory.newTestGCDClient(mOAuthTokenCopy).deleteInstance(instance.id);
            }
        }

        @Override
        protected void onSuccess() {
            updateList();
        }
    }
}
