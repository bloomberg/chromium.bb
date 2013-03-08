// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import java.util.ArrayList;
import java.util.List;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;

import android.content.DialogInterface.OnClickListener;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ScrollView;
import android.widget.AdapterView.OnItemSelectedListener;

import org.chromium.chrome.R;

/**
 * This is the dialog that will act as the java side controller between the
 * AutofillDialogGlue object and the UI elements on the page. It contains the
 * title and the content views and relays messages from the backend to them.
 */
public class AutofillDialog extends AlertDialog
        implements OnClickListener, OnItemSelectedListener {
    private AutofillDialogContentView mContentView;
    private AutofillDialogTitleView mTitleView;
    private boolean mWillDismiss = false;

    protected AutofillDialog(Context context) {
        super(context);

        addTitleWithPlaceholders();

        ScrollView scroll = new ScrollView(context);
        mContentView = (AutofillDialogContentView) getLayoutInflater().
                inflate(R.layout.autofill_dialog_content, null);
        scroll.addView(mContentView);
        setView(scroll);

        setButton(AlertDialog.BUTTON_NEGATIVE,
                getContext().getResources().getString(R.string.autofill_negative_button), this);
        setButton(AlertDialog.BUTTON_POSITIVE,
                getContext().getResources().getString(R.string.autofill_positive_button), this);

        mContentView.setOnItemSelectedListener(this);
    }

    @Override
    public void dismiss() {
        if (mWillDismiss) super.dismiss();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (mContentView.getCurrentLayout() == AutofillDialogContentView.LAYOUT_STEADY) {
            mWillDismiss = true;
            return;
        }

        mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_STEADY);
        getButton(BUTTON_POSITIVE).setText(R.string.autofill_positive_button);
        mWillDismiss = false;
    }

    @Override
    public void onItemSelected(AdapterView<?> spinner, View view, int position,
            long id) {
        if (spinner.getId() == R.id.accounts_spinner) {
            mContentView.changeLayoutTo(AutofillDialogContentView.LAYOUT_FETCHING);
            return;
        }

        if (!mContentView.selectionShouldChangeLayout(spinner, position) ||
                mContentView.getCurrentLayout() != AutofillDialogContentView.LAYOUT_STEADY) {
            return;
        }

        int newLayout = spinner.getId() == R.id.address_spinner ?
                AutofillDialogContentView.LAYOUT_EDITING_SHIPPING :
                        AutofillDialogContentView.LAYOUT_EDITING_BILLING;
        mContentView.changeLayoutTo(newLayout);
        spinner.setSelection(0);
        getButton(BUTTON_POSITIVE).setText(R.string.autofill_positive_button_editing);
    }

    @Override
    public void onNothingSelected(AdapterView<?> spinner) {
    }

    //TODO(yusufo): Remove this. updateAccountChooserAndAddTitle will get initiated from glue.
    private void addTitleWithPlaceholders() {
        List<String> accounts = new ArrayList<String>();
        accounts.add("placeholder@gmail.com");
        accounts.add("placeholder@google.com");
        updateAccountChooserAndAddTitle(accounts);
    }

    /**
     * Update account chooser dropdown with given accounts and create the title view if needed.
     * @param accounts The accounts to be used for the dropdown.
     */
    public void updateAccountChooserAndAddTitle(List<String> accounts) {
        if (mTitleView == null) {
            mTitleView = new AutofillDialogTitleView(getContext(), accounts);
            mTitleView.setOnItemSelectedListener(this);
            setCustomTitle(mTitleView);
            return;
        }
        // TODO(yusufo): Add code to update accounts after title is created.
    }
}