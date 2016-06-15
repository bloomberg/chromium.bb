// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi.DownloadManagerUiDelegate;

/**
 * Activity for managing downloads handled through Chrome.
 */
public class DownloadActivity extends SnackbarActivity implements DownloadManagerUiDelegate {
    private DownloadManagerUi mDownloadManagerUi;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mDownloadManagerUi =
                (DownloadManagerUi) LayoutInflater.from(this).inflate(R.layout.download_main, null);
        mDownloadManagerUi.initialize(this, this);
        setContentView(mDownloadManagerUi);
    }

    @Override
    public void onBackPressed() {
        if (!mDownloadManagerUi.onBackPressed()) super.onBackPressed();
    }

    @Override
    public void onCloseButtonClicked(DownloadManagerUi ui) {
        finish();
    }

    /**
     * Convenience method for launching this Activity.
     * @param context Context to use when starting this Activity.
     */
    public static void launch(Context context) {
        Intent intent = new Intent();
        intent.setClass(context, DownloadActivity.class);
        context.startActivity(intent);
    }
}
