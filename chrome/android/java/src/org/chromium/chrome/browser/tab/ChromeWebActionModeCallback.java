// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.view.ActionMode;
import android.view.Menu;

import org.chromium.chrome.R;
import org.chromium.content.browser.WebActionModeCallback;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Overrides the default SelectionActionModeCallback from content and provides chrome-specific
 * changes:
 * - sets the title for the action bar description based on tablet/phone UI.
 */
public class ChromeWebActionModeCallback extends WebActionModeCallback {
    ChromeWebActionModeCallback(Context context, ActionHandler actionHandler) {
        super(context, actionHandler);
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        mode.setTitle(DeviceFormFactor.isTablet(getContext())
                ? getContext().getString(R.string.actionbar_textselection_title) : null);
        return super.onCreateActionMode(mode, menu);
    }
}
