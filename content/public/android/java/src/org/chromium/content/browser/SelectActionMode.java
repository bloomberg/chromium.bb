// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.ActionMode;

import org.chromium.base.Log;

/**
 * An ActionMode for in-page selection. This class wraps an ActionMode created
 * by the associated View, providing modified interaction with that ActionMode.
 */
public class SelectActionMode {
    private static final String TAG = "cr.SelectActionMode";

    protected final ActionMode mActionMode;

    /**
     * Constructs a SelectActionMode instance wrapping a concrete ActionMode.
     * @param actionMode the wrapped ActionMode.
     */
    public SelectActionMode(ActionMode actionMode) {
        assert actionMode != null;
        mActionMode = actionMode;
    }

    /**
     * @see ActionMode#finish()
     */
    public void finish() {
        mActionMode.finish();
    }

    /**
     * @see ActionMode#invalidate()
     */
    public void invalidate() {
        // Try/catch necessary for framework bug, crbug.com/446717.
        try {
            mActionMode.invalidate();
        } catch (NullPointerException e) {
            Log.w(TAG, "Ignoring NPE from ActionMode.invalidate() as workaround for L", e);
        }
    }

    /**
     * @see ActionMode#invalidateContentRect()
     */
    public void invalidateContentRect() {}
}
