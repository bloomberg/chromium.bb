// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.view.ActionMode;

import org.chromium.content.browser.WebActionMode;

/**
 * An extension for WebActionMode that supports floating ActionModes.
 */
public class FloatingWebActionMode extends WebActionMode {
    public FloatingWebActionMode(ActionMode actionMode) {
        super(actionMode);
        assert actionMode.getType() == ActionMode.TYPE_FLOATING;
    }

    @Override
    public void invalidateContentRect() {
        mActionMode.invalidateContentRect();
    }
}
