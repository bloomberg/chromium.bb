// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.snippets.SnippetsManager.SnippetHeader;
import org.chromium.chrome.browser.ntp.snippets.SnippetsManager.SnippetListItem;

/**
 * A class that represents the view for a header of a group of card snippets.
 */
class SnippetHeaderItemViewHolder extends SnippetListItemViewHolder {
    private TextView mHeaderTextView;

    public SnippetHeaderItemViewHolder(View view, SnippetsManager manager) {
        super(view, manager);
        mHeaderTextView = (TextView) view.findViewById(R.id.snippets_list_header);
    }

    /**
     * Creates the TextView object for displaying the header for a group of snippets
     *
     * @param parent The parent view for the header
     * @return a TextView object for displaying a header for a group of snippets
     */
    public static View createView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.new_tab_page_snippets_header, parent, false);
    }

    @Override
    public void onBindViewHolder(SnippetListItem snippetItem) {
        SnippetHeader item = (SnippetHeader) snippetItem;

        String headerText = mHeaderTextView.getContext().getString(
                R.string.snippets_header, item.mRecommendationBasis);
        mHeaderTextView.setText(headerText);
    }
}
