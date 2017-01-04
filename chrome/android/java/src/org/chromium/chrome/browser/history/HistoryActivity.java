// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.os.Bundle;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.SynchronousInitializationActivity;

/**
 * Activity for displaying the browsing history manager.
 */
public class HistoryActivity extends SynchronousInitializationActivity {
    private HistoryManager mHistoryManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mHistoryManager = new HistoryManager(this);
        setContentView(mHistoryManager.getView());
    }

    @Override
    protected void onDestroy() {
        mHistoryManager.onDestroyed();
        mHistoryManager = null;
        super.onDestroy();
    }

    @VisibleForTesting
    HistoryManager getHistoryManagerForTests() {
        return mHistoryManager;
    }
}
