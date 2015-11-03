// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.ResultReceiver;
import android.view.View;

import org.chromium.content.browser.input.ImeAdapter.ImeAdapterDelegate;

/**
 * An empty ImeAdapterDelegate used for testing.
 */
public class TestImeAdapterDelegate implements ImeAdapterDelegate {
    @Override
    public void onImeEvent() {}

    @Override
    public void onKeyboardBoundsUnchanged() {}

    @Override
    public boolean performContextMenuAction(int id) {
        return false;
    }

    @Override
    public View getAttachedView() {
        return null;
    }

    @Override
    public ResultReceiver getNewShowKeyboardReceiver() {
        return null;
    }
}