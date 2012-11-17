// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import org.chromium.content.app.AppResource;

import java.util.ArrayList;

/**
 * Autofill suggestion adapter for AutofillWindow.
 */
public class AutofillListAdapter extends ArrayAdapter<AutofillSuggestion> {
    private Context mContext;

    AutofillListAdapter(Context context, ArrayList<AutofillSuggestion> objects) {
        super(context, AppResource.LAYOUT_AUTOFILL_TEXT, objects);
        mContext = context;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View layout = convertView;
        if (convertView == null) {
            LayoutInflater inflater =
                    (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            layout = inflater.inflate(AppResource.LAYOUT_AUTOFILL_TEXT, null);
        }
        TextView nameView = (TextView) layout.findViewById(AppResource.ID_AUTOFILL_NAME);
        nameView.setText(getItem(position).mName);
        TextView labelView = (TextView) layout.findViewById(AppResource.ID_AUTOFILL_LABEL);
        labelView.setText(getItem(position).mLabel);

        return layout;
    }
}