// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.text.SpannableString;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Class for managing the first run screen. */
class FirstRunScreen {
    /**
     * Shows the first run screen and calls callback with the result.
     */
    static void show(ChromeActivity activity, Callback<Boolean> callback) {
        ViewGroup coordinatorView = (ViewGroup) activity.findViewById(
                org.chromium.chrome.autofill_assistant.R.id.coordinator);
        View initView = LayoutInflater.from(activity)
                                .inflate(R.layout.init_screen, coordinatorView)
                                .findViewById(R.id.init_screen);

        TextView termsTextView = initView.findViewById(R.id.google_terms_message);
        String termsString = activity.getApplicationContext().getString(
                R.string.autofill_assistant_google_terms_description);

        NoUnderlineClickableSpan termsDescriptionSpan = new NoUnderlineClickableSpan(
                (widget)
                        -> {
                                // TODO(crbug.com/806868) enable when the URL for our terms &
                                // conditions is up.
                                /*
                                CustomTabActivity.showInfoPage(context,
                                        activity.getApplicationContext().getString(
                                                R.string.autofill_assistant_google_terms_url));
                                */
                        });
        SpannableString spannableMessage = SpanApplier.applySpans(
                termsString, new SpanApplier.SpanInfo("<link>", "</link>", termsDescriptionSpan));
        termsTextView.setText(spannableMessage);

        // Set focusable for accessibility.
        initView.findViewById(R.id.init).setFocusable(true);

        initView.findViewById(R.id.button_init_ok)
                .setOnClickListener(unusedView -> onClicked(true, initView, activity, callback));
        initView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView -> onClicked(false, initView, activity, callback));
        initView.announceForAccessibility(
                activity.getString(R.string.autofill_assistant_first_run_accessibility));
    }

    private static void onClicked(
            boolean accept, View initView, ChromeActivity activity, Callback<Boolean> callback) {
        ViewGroup coordinatorView = (ViewGroup) activity.findViewById(
                org.chromium.chrome.autofill_assistant.R.id.coordinator);
        CheckBox checkBox = initView.findViewById(
                org.chromium.chrome.autofill_assistant.R.id.checkbox_dont_show_init_again);
        AutofillAssistantPreferencesUtil.setInitialPreferences(accept, checkBox.isChecked());
        coordinatorView.removeView(initView);

        callback.onResult(accept);
    }
}
