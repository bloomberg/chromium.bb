// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;

/**
 * Represents the data for a header of a group of snippets
 */
public class SnippetHeaderListItem implements NewTabPageListItem {
    /** Whether the header should be shown. */
    private boolean mVisible = false;

    /**
     * Creates the View object for displaying the header for a group of snippets
     *
     * @param parent The parent view for the header
     * @return a View object for displaying a header for a group of snippets
     */
    public static View createView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.new_tab_page_snippets_header, parent, false);
    }

    @Override
    public int getType() {
        return NewTabPageListItem.VIEW_TYPE_HEADER;
    }

    public boolean isVisible() {
        return mVisible;
    }

    public void setVisible(boolean visible) {
        mVisible = visible;
    }
}