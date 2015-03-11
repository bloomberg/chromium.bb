// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.Fragment;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.sync.internal_api.pub.PassphraseType;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * Dialog to ask to user to enter their sync password.
 */
public class PassphraseDialogFragment extends DialogFragment implements OnClickListener {

    private static final String TAG = "PassphraseDialogFragment";

    /**
     * A listener for passphrase events.
     *
     * Move back to package scope once PassphraseActivity is upstream.
     */
    public interface Listener {
        public void onPassphraseEntered(String password, boolean isGaia, boolean isUpdate);

        public void onPassphraseCanceled(boolean isGaia, boolean isUpdate);
    }

    static final String ARG_IS_GAIA = "is_gaia";
    static final String ARG_IS_UPDATE = "is_update";

    /**
     * Create a new instanceof of {@link PassphraseDialogFragment} and set its arguments.
     */
    public static PassphraseDialogFragment newInstance(
            Fragment target, boolean isGaia, boolean isUpdate) {
        PassphraseDialogFragment dialog = new PassphraseDialogFragment();
        if (target != null) {
            dialog.setTargetFragment(target, -1);
        }
        Bundle args = new Bundle();
        args.putBoolean(PassphraseDialogFragment.ARG_IS_GAIA, isGaia);
        args.putBoolean(PassphraseDialogFragment.ARG_IS_UPDATE, isUpdate);
        dialog.setArguments(args);
        return dialog;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        LayoutInflater inflater = getActivity().getLayoutInflater();
        View v = inflater.inflate(R.layout.sync_enter_passphrase, null);

        boolean isGaia = getArguments().getBoolean(ARG_IS_GAIA);
        TextView promptText = (TextView) v.findViewById(R.id.prompt_text);
        final EditText passphrase = (EditText) v.findViewById(R.id.passphrase);
        final Context context = passphrase.getContext();
        TextView resetText = (TextView) v.findViewById(R.id.reset_text);
        ProfileSyncService profileSyncService = ProfileSyncService.get(context);
        String accountName = profileSyncService.getCurrentSignedInAccountText() + "\n\n";
        if (isGaia) {
            promptText.setText(accountName + context.getText(
                    R.string.sync_enter_google_passphrase));
            passphrase.setHint(R.string.sync_enter_google_passphrase_hint);
        } else {
            if (profileSyncService.hasExplicitPassphraseTime()) {
                resetText.setText(SpanApplier.applySpans(
                        context.getString(R.string.sync_passphrase_reset_instructions),
                        new SpanInfo("<link>", "</link>", new ClickableSpan() {
                            @Override
                            public void onClick(View view) {
                                Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(
                                        context.getText(R.string.sync_dashboard_url).toString()));
                                intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                                        context.getPackageName());
                                intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
                                intent.setPackage(context.getPackageName());
                                context.startActivity(intent);
                                Activity activity = getActivity();
                                if (activity != null) activity.finish();
                            }
                        })));
                resetText.setMovementMethod(LinkMovementMethod.getInstance());
                resetText.setVisibility(View.VISIBLE);
                PassphraseType passphraseType = profileSyncService.getPassphraseType();
                switch (passphraseType) {
                    case FROZEN_IMPLICIT_PASSPHRASE:
                        promptText.setText(accountName + profileSyncService
                                .getSyncEnterGooglePassphraseBodyWithDateText());
                        break;
                    case CUSTOM_PASSPHRASE:
                        promptText.setText(accountName + profileSyncService
                                .getSyncEnterCustomPassphraseBodyWithDateText());
                        break;
                    case IMPLICIT_PASSPHRASE: // Falling through intentionally.
                    case KEYSTORE_PASSPHRASE: // Falling through intentionally.
                    default:
                        Log.w(TAG, "Found incorrect passphrase type "
                                + passphraseType
                                + ". Falling back to default string.");
                        promptText.setText(accountName
                                + profileSyncService.getSyncEnterCustomPassphraseBodyText());
                        break;
                }
            } else {
                promptText.setText(accountName
                        + profileSyncService.getSyncEnterCustomPassphraseBodyText());
            }
            passphrase.setHint(R.string.sync_enter_custom_passphrase_hint);
        }
        passphrase.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_NEXT) {
                    handleOk();
                }
                return false;
            }
        });

        final AlertDialog d = new AlertDialog.Builder(getActivity(), AlertDialog.THEME_HOLO_LIGHT)
                .setView(v)
                .setPositiveButton(R.string.ok, new Dialog.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface d, int which) {
                        // We override the onclick. This is a hack to not dismiss the dialog after
                        // click of OK and instead dismiss it after confirming the passphrase
                        // is correct.
                    }
                })
                .setNegativeButton(R.string.cancel, this)
                .setTitle(R.string.sign_in_google_account)
                .create();
        d.setOnShowListener(new DialogInterface.OnShowListener() {
            @Override
            public void onShow(DialogInterface dialog) {
                Button b = d.getButton(AlertDialog.BUTTON_POSITIVE);
                b.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        handleOk();
                    }
                });
            }
        });
        return d;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_NEGATIVE) {
            handleCancel();
        }
    }

    private void handleCancel() {
        boolean isUpdate = getArguments().getBoolean(ARG_IS_UPDATE);
        boolean isGaia = getArguments().getBoolean(ARG_IS_GAIA);
        getListener().onPassphraseCanceled(isGaia, isUpdate);
    }

    private void handleOk() {
        TextView verifying = (TextView) getDialog().findViewById(R.id.verifying);
        verifying.setText(R.string.sync_verifying);

        EditText passphraseEditText = (EditText) getDialog().findViewById(R.id.passphrase);
        String passphrase = passphraseEditText.getText().toString();

        boolean isUpdate = getArguments().getBoolean(ARG_IS_UPDATE);
        boolean isGaia = getArguments().getBoolean(ARG_IS_GAIA);
        getListener().onPassphraseEntered(passphrase, isGaia, isUpdate);
    }

    private Listener getListener() {
        Fragment target = getTargetFragment();
        if (target instanceof Listener) {
            return (Listener) target;
        }
        return (Listener) getActivity();
    }

    /**
     * Notify this fragment that the password the user entered is incorrect.
     */
    public void invalidPassphrase() {
        TextView verifying = (TextView) getDialog().findViewById(R.id.verifying);
        verifying.setText(R.string.sync_passphrase_incorrect);
    }
}
