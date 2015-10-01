// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.TargetApi;
import android.graphics.Rect;
import android.os.Build;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

/**
 * A wrapper for SelectActionModeCallback that extends ActionMode.Callback2 to
 * support floating ActionModes.
 */
@TargetApi(Build.VERSION_CODES.M)
public class FloatingWebActionModeCallback extends ActionMode.Callback2 {
    private final WebActionModeCallback mWrappedCallback;

    public FloatingWebActionModeCallback(WebActionModeCallback wrappedCallback) {
        mWrappedCallback = wrappedCallback;
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        // If the created ActionMode isn't actually floating, abort creation altogether.
        if (mode.getType() != ActionMode.TYPE_FLOATING) return false;
        return mWrappedCallback.onCreateActionMode(mode, menu);
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return mWrappedCallback.onPrepareActionMode(mode, menu);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        return mWrappedCallback.onActionItemClicked(mode, item);
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {
        mWrappedCallback.onDestroyActionMode(mode);
    }

    @Override
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        mWrappedCallback.onGetContentRect(mode, view, outRect);
    }
}
