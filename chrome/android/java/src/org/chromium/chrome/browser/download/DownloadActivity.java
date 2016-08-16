// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.ComponentName;
import android.os.Bundle;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Activity for managing downloads handled through Chrome.
 */
public class DownloadActivity extends SnackbarActivity {
    private DownloadManagerUi mDownloadManagerUi;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean isIncognito = DownloadUtils.shouldShowOffTheRecordDownloads(getIntent());
        ComponentName parentComponent = IntentUtils.safeGetParcelableExtra(
                getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        mDownloadManagerUi = new DownloadManagerUi(this, isIncognito, parentComponent);
        setContentView(mDownloadManagerUi.getView());
    }

    @Override
    public void onBackPressed() {
        if (!mDownloadManagerUi.onBackPressed()) super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        mDownloadManagerUi.onDestroyed();
        super.onDestroy();
    }
}
