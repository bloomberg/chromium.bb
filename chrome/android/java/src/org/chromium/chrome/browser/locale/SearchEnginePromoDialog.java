// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SearchEnginePreference;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * A promotion dialog showing that the default search provider will be set to Sogou.
 */
public class SearchEnginePromoDialog extends Dialog implements View.OnClickListener {

    private final LocaleManager mLocaleManager;
    private final ClickableSpan mSpan = new NoUnderlineClickableSpan() {
        @Override
        public void onClick(View widget) {
            Intent intent = PreferencesLauncher.createIntentForSettingsPage(getContext(),
                    SearchEnginePreference.class.getName());
            getContext().startActivity(intent);
        }
    };

    /**
     * Creates an instance of the dialog.
     */
    public SearchEnginePromoDialog(Context context, LocaleManager localeManager) {
        super(context, R.style.SimpleDialog);
        mLocaleManager = localeManager;
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
            keepGoogle();
        } else if (view.getId() == R.id.ok_button) {
            useSogou();
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
}
