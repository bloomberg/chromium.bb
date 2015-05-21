// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

/**
 * A class that represents a custom ActionMode.Callback.
 */
public class CustomSelectionActionModeCallback implements ActionMode.Callback {

    private ContextualMenuBar mContextualMenuBar;

    /**
     * Sets the @param contextualMenuBar.
     */
    public void setContextualMenuBar(ContextualMenuBar contextualMenuBar) {
        mContextualMenuBar = contextualMenuBar;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return true;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {
        mContextualMenuBar.hideControls();
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        mContextualMenuBar.showControls();
        return true;
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        return false;
    }
}
