// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import java.util.ArrayList;

import android.content.Context;
import android.view.LayoutInflater;
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

    private ArrayList<String> mAccountNames;
    private ArrayAdapter<String> mAdapter;

    /**
     * Create a title using the given context with only the default dropdown items.
     * @param context The context to create the title within.
     */
    public AutofillDialogTitleView(Context context) {
        super(context);

        LayoutInflater.from(context).inflate(R.layout.autofill_dialog_title, this, true);
        Spinner accountsSpinner = (Spinner)findViewById(R.id.accounts_spinner);
        mAdapter = new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item);
        mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        accountsSpinner.setAdapter(mAdapter);
    }

    /**
     * Update account chooser dropdown with given accounts and create the title view if needed.
     * @param accountNames The dropdown items to be listed.
     * @param selectedAccountIndex The index of a currently selected account or
     *                             -1 if nothing is selected.
     */
    public void updateAccountsAndSelect(ArrayList<String> accountNames,
            int selectedAccountIndex) {
        mAdapter.clear();
        mAccountNames = accountNames;
        mAdapter.addAll(mAccountNames);
        Spinner accountsSpinner = (Spinner)findViewById(R.id.accounts_spinner);
        if (selectedAccountIndex >= 0) {
            accountsSpinner.setSelection(selectedAccountIndex);
        }
    }

    /**
     * Set the listener for all the dropdown members in the layout.
     * @param listener The listener object to attach to the dropdowns.
     */
    public void setOnItemSelectedListener(OnItemSelectedListener listener) {
        Spinner accounts_spinner = (Spinner)findViewById(R.id.accounts_spinner);
        accounts_spinner.setOnItemSelectedListener(listener);
    }
}