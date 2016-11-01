// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.widget.ArrayAdapter;

import java.util.List;

/**
 * Subclass of ArrayAdapter used to display a dropdown hint that won't appear in the dropdown
 * options. The last element is the hint.
 *
 * @param <T> The type of element to be inserted into the adapter.
 */
public class HintArrayAdapter<T> extends ArrayAdapter<T> {

    /**
     * Creates an array adapter for which the last element is not shown.
     *
     * @param context  The current context.
     * @param resource The resource ID for a layout file containing a layout to use when
     *                 instantiating views.
     * @param objects  The objects to represent in the ListView, the last of which is not displayed
     *                 in the dropdown, but is used as the hint.
     */
    public HintArrayAdapter(Context context, int resource, List<T> objects) {
        super(context, resource, objects);
    }

    @Override
    public int getCount() {
        // Don't display last item, it is used as hint.
        int count = super.getCount();
        return count > 0 ? count - 1 : count;
    }
}