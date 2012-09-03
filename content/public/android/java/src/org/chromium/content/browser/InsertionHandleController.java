// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.view.View;

class InsertionHandleController {

    /** The view over which the insertion handle should be shown */
    private View mParent;

    private Context mContext;

    InsertionHandleController(View parent) {
        mParent = parent;
        mContext = parent.getContext();
    }

    /** Disallows the handle from being shown automatically when cursor position changes */
    public void hideAndDisallowAutomaticShowing() {
        // TODO(olilan): add method implementation (this is just a stub for ImeAdapter).
    }
}
