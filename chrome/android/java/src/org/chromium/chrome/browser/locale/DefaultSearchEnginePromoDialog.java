// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.Button;
import android.widget.RadioGroup;
import android.widget.RadioGroup.OnCheckedChangeListener;

import org.chromium.base.Callback;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.locale.LocaleManager.SearchEnginePromoType;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrl;
import org.chromium.chrome.browser.widget.PromoDialog;
import org.chromium.chrome.browser.widget.RadioButtonLayout;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** A dialog that forces the user to choose a default search engine. */
public class DefaultSearchEnginePromoDialog extends PromoDialog implements OnCheckedChangeListener {
    /** Used to determine the promo dialog contents. */
    @SearchEnginePromoType
    private final int mDialogType;

    /** Called when the dialog is dismissed after the user has chosen a search engine. */
    private final Callback<Boolean> mOnDismissed;

    private RadioGroup mRadioGroup;
    private String mSelectedKeyword;

    /**
     * Construct and show the dialog.  Will be asynchronous if the TemplateUrlService has not yet
     * been loaded.
     *
     * @param context     Context to build the dialog with.
     * @param dialogType  Type of dialog to show.
     * @param onDismissed Notified about whether the user chose an engine when it got dismissed.
     */
    public static void show(final Context context, @SearchEnginePromoType final int dialogType,
            @Nullable final Callback<Boolean> onDismissed) {
        assert LibraryLoader.isInitialized();

        // Load up the search engines.
        final TemplateUrlService instance = TemplateUrlService.getInstance();
        instance.registerLoadListener(new TemplateUrlService.LoadListener() {
            @Override
            public void onTemplateUrlServiceLoaded() {
                instance.unregisterLoadListener(this);
                new DefaultSearchEnginePromoDialog(context, dialogType, onDismissed).show();
            }
        });
        if (!instance.isLoaded()) instance.load();
    }

    private DefaultSearchEnginePromoDialog(
            Context context, int dialogType, @Nullable Callback<Boolean> onDismissed) {
        super(context);
        mDialogType = dialogType;
        mOnDismissed = onDismissed;
        setOnDismissListener(this);

        // No one should be able to bypass this dialog by clicking outside or by hitting back.
        setCancelable(false);
        setCanceledOnTouchOutside(false);
    }

    @Override
    protected DialogParams getDialogParams() {
        PromoDialog.DialogParams params = new PromoDialog.DialogParams();
        params.headerStringResource = R.string.search_engine_dialog_title;
        params.footerStringResource = R.string.search_engine_dialog_footer;
        params.primaryButtonStringResource = R.string.ok;
        return params;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Determine what search engines will be listed.
        TemplateUrlService instance = TemplateUrlService.getInstance();
        assert instance.isLoaded();
        List<TemplateUrl> engines = null;
        if (mDialogType == LocaleManager.SEARCH_ENGINE_PROMO_SHOW_EXISTING) {
            engines = instance.getSearchEngines();
        } else {
            // TODO(dfalcantara): Handle the new user case.
            assert false;
        }

        // Shuffle up the engines.
        List<CharSequence> engineNames = new ArrayList<>();
        List<String> engineKeywords = new ArrayList<>();
        Collections.shuffle(engines);
        for (int i = 0; i < engines.size(); i++) {
            TemplateUrl engine = engines.get(i);
            engineNames.add(engine.getShortName());
            engineKeywords.add(engine.getKeyword());
        }

        // Add the search engines to the dialog.
        RadioButtonLayout radioButtons = new RadioButtonLayout(getContext());
        radioButtons.addOptions(engineNames, engineKeywords);
        radioButtons.setOnCheckedChangeListener(this);
        addControl(radioButtons);

        Button button = (Button) findViewById(R.id.button_primary);
        button.setOnClickListener(this);
        updateButtonState();
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        mSelectedKeyword = (String) group.findViewById(checkedId).getTag();
        updateButtonState();
    }

    @Override
    public void onClick(View view) {
        if (view.getId() == R.id.button_primary) {
            if (mSelectedKeyword == null) {
                updateButtonState();
                return;
            }

            dismiss();
        } else {
            assert false : "Unhandled click.";
        }

        // Don't propagate the click to the parent to prevent circumventing the dialog.
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        if (mSelectedKeyword == null) {
            // This shouldn't happen, but in case it does, finish the Activity so that the user has
            // to respond to the dialog next time.
            if (getOwnerActivity() != null) getOwnerActivity().finish();
        } else {
            TemplateUrlService.getInstance().setSearchEngine(mSelectedKeyword.toString());
            // TODO(dfalcantara): Prevent the dialog from appearing again.
        }

        if (mOnDismissed != null) mOnDismissed.onResult(mSelectedKeyword != null);
    }

    private void updateButtonState() {
        findViewById(R.id.button_primary).setEnabled(mSelectedKeyword != null);
    }
}
