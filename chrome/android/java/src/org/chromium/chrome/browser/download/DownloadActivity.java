// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.ComponentName;
import android.os.Bundle;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Activity for managing downloads handled through Chrome.
 */
public class DownloadActivity extends SnackbarActivity {
    private DownloadManagerUi mDownloadManagerUi;
    private boolean mIsOffTheRecord;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean isOffTheRecord = DownloadUtils.shouldShowOffTheRecordDownloads(getIntent());
        ComponentName parentComponent = IntentUtils.safeGetParcelableExtra(
                getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        mDownloadManagerUi = new DownloadManagerUi(this, isOffTheRecord, parentComponent);
        setContentView(mDownloadManagerUi.getView());
        mIsOffTheRecord = isOffTheRecord;
    }

    @Override
    public void onResume() {
        super.onResume();
        DownloadUtils.checkForExternallyRemovedDownloads(mDownloadManagerUi.getBackendProvider(),
                mIsOffTheRecord);
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

    @VisibleForTesting
    DownloadManagerUi getDownloadManagerUiForTests() {
        return mDownloadManagerUi;
    }
}
