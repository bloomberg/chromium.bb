// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectionToolbar;

/**
 * The SelectionToolbar for the browsing history UI.
 */
public class HistoryManagerToolbar extends SelectionToolbar<HistoryItem> {
    public HistoryManagerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateMenu(R.menu.history_manager_menu);

        // TODO(twellington): Add content descriptions to the number roll view.
    }
}
