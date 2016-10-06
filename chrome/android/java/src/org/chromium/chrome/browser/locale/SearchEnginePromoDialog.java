// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SearchEnginePreference;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * A promotion dialog showing that the default search provider will be set to Sogou.
 */
public class SearchEnginePromoDialog extends Dialog
        implements View.OnClickListener, OnDismissListener {
    private static final int CHOICE_USE_SOGOU = 0;
    private static final int CHOICE_KEEP_GOOGLE = 1;

    private final LocaleManager mLocaleManager;
    private final ClickableSpan mSpan = new NoUnderlineClickableSpan() {
        @Override
        public void onClick(View widget) {
            Intent intent = PreferencesLauncher.createIntentForSettingsPage(getContext(),
                    SearchEnginePreference.class.getName());
            getContext().startActivity(intent);
        }
    };

    private int mChoice = CHOICE_USE_SOGOU;

    /**
     * Creates an instance of the dialog.
     */
    public SearchEnginePromoDialog(Context context, LocaleManager localeManager) {
        super(context, R.style.SimpleDialog);
        mLocaleManager = localeManager;
        setOnDismissListener(this);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Do not allow this dialog to be reconstructed because it requires native side loaded.
        if (savedInstanceState != null) {
            dismiss();
            return;
        }
        setContentView(R.layout.search_engine_promo);

        View keepGoogleButton = findViewById(R.id.keep_google_button);
        View okButton = findViewById(R.id.ok_button);
        keepGoogleButton.setOnClickListener(this);
        okButton.setOnClickListener(this);

        TextView textView = (TextView) findViewById(R.id.description);
        SpannableString description = SpanApplier.applySpans(
                getContext().getString(R.string.sogou_explanation),
                new SpanInfo("<link>", "</link>", mSpan));
        textView.setText(description);
        textView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    @Override
    public void onClick(View view) {
        if (view.getId() == R.id.keep_google_button) {
            mChoice = CHOICE_KEEP_GOOGLE;
        } else if (view.getId() == R.id.ok_button) {
            mChoice = CHOICE_USE_SOGOU;
        } else {
            assert false : "Not handled click.";
        }
        dismiss();
    }

    private void keepGoogle() {
        mLocaleManager.setSearchEngineAutoSwitch(false);
        mLocaleManager.addSpecialSearchEngines();
    }

    private void useSogou() {
        mLocaleManager.setSearchEngineAutoSwitch(true);
        mLocaleManager.addSpecialSearchEngines();
        mLocaleManager.overrideDefaultSearchEngine();
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        if (mChoice == CHOICE_KEEP_GOOGLE) {
            keepGoogle();
        } else if (mChoice == CHOICE_USE_SOGOU) {
            useSogou();
        }
        ContextUtils.getAppSharedPreferences().edit()
                .putBoolean(LocaleManager.PREF_PROMO_SHOWN, true).apply();
    }
}
