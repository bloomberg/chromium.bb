// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.sync.ui.ClearSyncDataPreferences;
import org.chromium.signin.InvestigatedScenario;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * The fragment shown when the user was previously signed in, then disconnected their account,
 * and is now attempting to sign in to a new account. This dialog warns the user that they should
 * clear their browser data, or else their bookmarks etc from their old account will be merged with
 * the new account when they sign in. This dialog assumes it is being created in the middle of the
 * signin flow, and as such is purposefully package private.
 */
class ConfirmAccountChangeFragment
        extends DialogFragment implements DialogInterface.OnClickListener {
    private static final String KEY_OLD_ACCOUNT_NAME = "lastAccountName";
    private static final String KEY_NEW_ACCOUNT_NAME = "newAccountName";

    /**
     * Prompts the user with a dialog if the user is changing accounts. If there is not a change
     * of accounts or the user accepts through the dialog, the signin flow is continued. This
     * means that the signin flow should have been started by the caller of this method.
     */
    public static void confirmSyncAccount(String syncAccountName, Activity activity) {
        // TODO(skym): Warn for high risk upgrade scenario, crbug.com/572754.
        if (SigninInvestigator.investigate(syncAccountName)
                == InvestigatedScenario.DIFFERENT_ACCOUNT) {
            ConfirmAccountChangeFragment dialog = newInstance(
                    syncAccountName, PrefServiceBridge.getInstance().getSyncLastAccountName());
            dialog.show(activity.getFragmentManager(), null);
        } else {
            // Do not display dialog, just sign-in.
            SigninManager.get(activity).progressSignInFlowCheckPolicy();
        }
    }

    public static ConfirmAccountChangeFragment newInstance(
            String syncAccountName, String lastSyncAccountName) {
        ConfirmAccountChangeFragment dialogFragment = new ConfirmAccountChangeFragment();
        Bundle args = new Bundle();
        args.putString(KEY_OLD_ACCOUNT_NAME, lastSyncAccountName);
        args.putString(KEY_NEW_ACCOUNT_NAME, syncAccountName);
        dialogFragment.setArguments(args);
        return dialogFragment;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        final Activity activity = getActivity();
        String lastSyncAccountName = getArguments().getString(KEY_OLD_ACCOUNT_NAME);
        String currentAccountName = getArguments().getString(KEY_NEW_ACCOUNT_NAME);

        LayoutInflater inflater = activity.getLayoutInflater();
        View v = inflater.inflate(R.layout.confirm_sync_account_change_account, null);
        final TextView textView = (TextView) v.findViewById(R.id.confirmMessage);
        String message = activity.getString(R.string.confirm_account_change_dialog_message,
                lastSyncAccountName, currentAccountName);

        // Show clear sync data dialog when the user clicks the "settings" link.
        SpannableString messageWithLink = SpanApplier.applySpans(
                message, new SpanInfo("<link>", "</link>", new ClickableSpan() {
                    @Override
                    public void onClick(View widget) {
                        showClearSyncDataPreferences();
                    }
                }));

        RecordUserAction.record("Signin_Show_ImportDataPrompt");
        textView.setText(messageWithLink);
        textView.setMovementMethod(LinkMovementMethod.getInstance());
        return new AlertDialog.Builder(getActivity(), R.style.AlertDialogTheme)
                .setTitle(R.string.confirm_account_change_dialog_title)
                .setPositiveButton(R.string.confirm_account_change_dialog_signin, this)
                .setNegativeButton(R.string.cancel, this)
                .setView(v)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            RecordUserAction.record("Signin_ImportDataPrompt_ImportData");
            SigninManager.get(getActivity()).progressSignInFlowCheckPolicy();
        } else if (which == AlertDialog.BUTTON_NEGATIVE) {
            RecordUserAction.record("Signin_ImportDataPrompt_Cancel");
            SigninManager.get(getActivity()).abortSignIn();
        }
    }

    private void showClearSyncDataPreferences() {
        Preferences prefActivity = (Preferences) getActivity();
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(prefActivity,
                ClearSyncDataPreferences.class.getName());
        prefActivity.startActivity(intent);

        // Cancel out of current sign in.
        SigninManager.get(getActivity()).abortSignIn();
        dismiss();

        RecordUserAction.record("Signin_ImportDataPrompt_DontImport");
    }
}
