// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.BufferType;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogProperties;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.modaldialog.ModalDialogViewBinder;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * Prompt that asks users to confirm user's name before saving card to Google.
 */
public class AutofillNameFixFlowPrompt implements ModalDialogView.Controller {
    /**
     * Legal message line with links to show in the fix flow prompt.
     */
    public static class LegalMessageLine {
        /**
         * A link in the legal message line.
         */
        public static class Link {
            /**
             * The starting inclusive index of the link position in the text.
             */
            public int start;

            /**
             * The ending exclusive index of the link position in the text.
             */
            public int end;

            /**
             * The URL of the link.
             */
            public String url;

            /**
             * Creates a new instance of the link.
             *
             * @param start The starting inclusive index of the link position in the text.
             * @param end The ending exclusive index of the link position in the text.
             * @param url The URL of the link.
             */
            public Link(int start, int end, String url) {
                this.start = start;
                this.end = end;
                this.url = url;
            }
        }

        /**
         * The plain text legal message line.
         */
        public String text;

        /**
         * A collection of links in the legal message line.
         */
        public final List<Link> links = new ArrayList<Link>();

        /**
         * Creates a new instance of the legal message line.
         *
         * @param legalText The plain text legal message.
         */
        public LegalMessageLine(String legalText) {
            text = legalText;
        }
    }

    /**
     * An interface to handle the interaction with
     * an AutofillNameFixFlowPrompt object.
     */
    public interface AutofillNameFixFlowPromptDelegate {
        /**
         * Called when dialog is dismissed.
         */
        void onPromptDismissed();

        /**
         * Called when user accepted/confirmed the prompt.
         *
         * @param name Card holder name.
         */
        void onUserAccept(String name);

        /**
         * Called when user clicked on legal message.
         *
         * @param url Legal message URL that user clicked.
         */
        void onLegalMessageLinkClicked(String url);
    }

    private final AutofillNameFixFlowPromptDelegate mDelegate;
    private final ModalDialogView mDialog;

    private final EditText mUserNameInput;

    private ModalDialogManager mModalDialogManager;
    private Context mContext;

    /**
     * Fix flow prompt to confirm user name before saving the card to Google.
     */
    public AutofillNameFixFlowPrompt(Context context, AutofillNameFixFlowPromptDelegate delegate,
            String title, String inferredName, List<LegalMessageLine> legalMessageLines,
            String confirmButtonLabel, int drawableId) {
        mDelegate = delegate;
        LayoutInflater inflater = LayoutInflater.from(context);
        View customView = inflater.inflate(R.layout.autofill_name_fixflow, null);

        mUserNameInput = (EditText) customView.findViewById(R.id.cc_name_edit);
        mUserNameInput.setText(inferredName, BufferType.EDITABLE);

        SpannableStringBuilder legalMessageText = new SpannableStringBuilder();
        for (LegalMessageLine line : legalMessageLines) {
            SpannableString text = new SpannableString(line.text);
            for (final LegalMessageLine.Link link : line.links) {
                text.setSpan(new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        delegate.onLegalMessageLinkClicked(link.url);
                    }
                }, link.start, link.end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            }
            legalMessageText.append(legalMessageText);
        }
        TextView legalMessageView = (TextView) customView.findViewById(R.id.cc_name_legal_message);
        legalMessageView.setText(legalMessageText);
        legalMessageView.setMovementMethod(LinkMovementMethod.getInstance());

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.TITLE_ICON, context, drawableId)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, confirmButtonLabel)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, context.getResources(),
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mDialog = new ModalDialogView(context);
        PropertyModelChangeProcessor.create(model, mDialog, new ModalDialogViewBinder());

        // Hitting the "submit" button on the software keyboard should submit.
        mUserNameInput.setOnEditorActionListener((view, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                onClick(ModalDialogView.ButtonType.POSITIVE);
                return true;
            }
            return false;
        });
    }

    /**
     * Show the dialog. If activity is null this method will not do anything.
     */
    public void show(ChromeActivity activity) {
        if (activity == null) return;

        mContext = activity;
        mModalDialogManager = activity.getModalDialogManager();

        mModalDialogManager.showDialog(mDialog, ModalDialogManager.ModalDialogType.APP);
    }

    protected void dismiss() {
        mModalDialogManager.dismissDialog(mDialog);
    }

    @Override
    public void onClick(@ModalDialogView.ButtonType int buttonType) {
        if (buttonType == ModalDialogView.ButtonType.POSITIVE) {
            mDelegate.onUserAccept(mUserNameInput.getText().toString());
        } else if (buttonType == ModalDialogView.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(
                    mDialog, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(@DialogDismissalCause int dismissalCause) {
        mDelegate.onPromptDismissed();
    }
}
