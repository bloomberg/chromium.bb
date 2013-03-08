// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import java.util.ArrayList;
import java.util.List;

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
    private static final List<String> mConstantItems = new ArrayList<String>();

    private List<String> mAccountNames;
    private ArrayAdapter<String> mAdapter;

    /**
     * Create a title using the given context with only the default dropdown items.
     * @param context The context to create the title within.
     */
    public AutofillDialogTitleView(Context context) {
        super(context);
        if (mConstantItems.isEmpty()) {
            mConstantItems.add(getResources().getString(R.string.autofill_new_account));
            mConstantItems.add(getResources().getString(R.string.autofill_use_local));
        }

        LayoutInflater.from(context).inflate(R.layout.autofill_dialog_title, this, true);
        Spinner accounts_spinner = (Spinner)findViewById(R.id.accounts_spinner);
        mAdapter = new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item);
        mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mAdapter.addAll(mConstantItems);
        accounts_spinner.setAdapter(mAdapter);
    }

    /**
     * Create a title using the given context with the given dropdown items.
     * @param context The context to create the title within.
     * @param accountNames The dropdown items to be listed.
     */
    public AutofillDialogTitleView(Context context, List<String> accountNames) {
        this(context);
        mAccountNames = accountNames;
        mAdapter.clear();
        mAdapter.addAll(mAccountNames);
        mAdapter.addAll(mConstantItems);
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