// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.View;

class SelectionHandleController {

    private View mParent;

    SelectionHandleController(View parent) {
        mParent = parent;
    }

    /** Hide selection anchors, and don't automatically show them. */
    public void hideAndDisallowAutomaticShowing() {
        // TODO(olilan): add method implementation (this is just a stub for ImeAdapter).
    }
}
