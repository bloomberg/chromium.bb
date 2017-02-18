// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * A simple activity that allows Chrome to start the physical web sharing service.
 */
public class PhysicalWebShareActivity extends AppCompatActivity {
    private static final String TAG = "PhysicalWebShareActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            Intent intent = getIntent();
            if (intent == null) return;
            if (!Intent.ACTION_SEND.equals(intent.getAction())) return;
            if (!IntentUtils.safeHasExtra(getIntent(), ShareHelper.EXTRA_TASK_ID)) return;
            handleShareAction();
        } finally {
            finish();
        }
    }

    private void handleShareAction() {
        // TODO(iankc): implement sharing
    }

    public static boolean sharingIsEnabled(Tab currentTab) {
        return PhysicalWeb.sharingIsEnabled() && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
    }
}
