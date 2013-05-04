// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import java.util.List;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.Spinner;
import android.widget.AdapterView.OnItemSelectedListener;

import org.chromium.chrome.R;

/**
 * This is the layout that contains the title items for the autofill dialog.
 * In principle it shouldn't contain any logic related with the
 * actual workflow, but rather respond to any UI update messages coming to it
 * from the AutofillDialog. It should also be not dependent on the UI state of
 * the content.
 */
public class AutofillDialogTitleView extends FrameLayout {
    private ArrayAdapter<String> mAdapter;

    /**
     * Creates a title using the given context with only the default dropdown items.
     * @param context The context to create the title within.
     */
    public AutofillDialogTitleView(Context context) {
        super(context);

        LayoutInflater.from(context).inflate(R.layout.autofill_dialog_title, this, true);
        Spinner accountsSpinner = (Spinner) findViewById(R.id.accounts_spinner);
        mAdapter = new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item);
        mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        accountsSpinner.setAdapter(mAdapter);
        accountsSpinner.setEnabled(false);
    }

    /**
     * Updates account chooser dropdown with given accounts and creates the title view if needed.
     * @param accountNames The dropdown items to be listed.
     * @param selectedAccountIndex The index of a currently selected account or
     *                             -1 if nothing is selected.
     */
    public void updateAccountsAndSelect(List<String> accountNames, int selectedAccountIndex) {
        mAdapter.clear();
        mAdapter.addAll(accountNames);
        Spinner accountsSpinner = (Spinner) findViewById(R.id.accounts_spinner);
        if (selectedAccountIndex >= 0) {
            accountsSpinner.setSelection(selectedAccountIndex);
            accountsSpinner.setVisibility(View.VISIBLE);
            View logo = findViewById(R.id.accounts_logo);
            boolean isWallet = accountNames.size() > 1 && selectedAccountIndex == 0;
            logo.setVisibility(isWallet ? View.VISIBLE : View.GONE);
        } else {
            hideLogoAndAccountChooserVisibility();
        }
    }

    /**
     * Hide the contents of the title view.
     */
    public void hideLogoAndAccountChooserVisibility() {
        findViewById(R.id.accounts_spinner).setVisibility(View.GONE);
        findViewById(R.id.accounts_logo).setVisibility(View.GONE);
    }

    /**
     * Enables/disables the account chooser.
     * @param enabled True if the account chooser is enabled.
     */
    public void setAccountChooserEnabled(boolean enabled) {
        Spinner accountsSpinner = (Spinner) findViewById(R.id.accounts_spinner);
        accountsSpinner.setEnabled(enabled);
        View logo = findViewById(R.id.accounts_logo);
        logo.setEnabled(enabled);
    }

    /**
     * Sets the listener for all the dropdown members in the layout.
     * @param listener The listener object to attach to the dropdowns.
     */
    public void setOnItemSelectedListener(OnItemSelectedListener listener) {
        Spinner accounts_spinner = (Spinner) findViewById(R.id.accounts_spinner);
        accounts_spinner.setOnItemSelectedListener(listener);
    }
}
