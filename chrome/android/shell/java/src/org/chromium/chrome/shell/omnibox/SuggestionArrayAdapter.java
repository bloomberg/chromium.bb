// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.omnibox;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import org.chromium.chrome.browser.omnibox.OmniboxSuggestion;
import org.chromium.chrome.shell.R;

import java.util.List;

/**
 * Adapter that provides suggestion views for the suggestion popup.
 */
class  SuggestionArrayAdapter extends ArrayAdapter<OmniboxSuggestion> {
    private final List<OmniboxSuggestion> mSuggestions;

    public SuggestionArrayAdapter(Context context, int res, List<OmniboxSuggestion> suggestions) {
        super(context, res, suggestions);
        mSuggestions = suggestions;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View v = convertView;
        if (v == null) {
            LayoutInflater vi = (LayoutInflater) getContext().getSystemService(
                    Context.LAYOUT_INFLATER_SERVICE);
            v = vi.inflate(R.layout.dropdown_item, null);
            int height = getContext().getResources().getDimensionPixelSize(
                    R.dimen.dropdown_item_height);
            v.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, height));
        }
        TextView t1 = (TextView) v.findViewById(R.id.dropdown_label);
        t1.setText(mSuggestions.get(position).getDisplayText());

        TextView t2 = (TextView) v.findViewById(R.id.dropdown_sublabel);
        t2.setText(mSuggestions.get(position).getUrl());
        return v;
    }
}
