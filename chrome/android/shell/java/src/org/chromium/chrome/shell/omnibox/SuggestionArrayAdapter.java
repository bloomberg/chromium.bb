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
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.browser.omnibox.OmniboxSuggestion;
import org.chromium.chrome.shell.R;

import java.util.List;

/**
 * Adapter that provides suggestion views for the suggestion popup.
 */
class  SuggestionArrayAdapter extends ArrayAdapter<OmniboxSuggestion> {
    private final List<OmniboxSuggestion> mSuggestions;
    private EditText mUrlTextView;

    public SuggestionArrayAdapter(Context context, int res, List<OmniboxSuggestion> suggestions,
            EditText urlTextView) {
        super(context, res, suggestions);
        mSuggestions = suggestions;
        mUrlTextView = urlTextView;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View v = convertView;
        if (v == null) {
            LayoutInflater vi = (LayoutInflater) getContext().getSystemService(
                    Context.LAYOUT_INFLATER_SERVICE);
            v = vi.inflate(R.layout.suggestion_item, null);
            int height = getContext().getResources().getDimensionPixelSize(
                    R.dimen.dropdown_item_height);
            v.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, height));
        }
        TextView t1 = (TextView) v.findViewById(R.id.suggestion_item_label);
        final String suggestionText = mSuggestions.get(position).getDisplayText();
        t1.setText(suggestionText);

        TextView t2 = (TextView) v.findViewById(R.id.suggestion_item_sublabel);
        t2.setText(mSuggestions.get(position).getUrl());

        ImageView arrow = (ImageView) v.findViewById(R.id.suggestion_item_arrow);
        arrow.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mUrlTextView.setText(suggestionText);
                mUrlTextView.setSelection(suggestionText.length());
            }
        });
        return v;
    }
}
