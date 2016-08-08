// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;

/**
 * Activity for managing downloads handled through Chrome.
 */
public class DownloadActivity extends SnackbarActivity {
    private DownloadManagerUi mDownloadManagerUi;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mDownloadManagerUi = new DownloadManagerUi(this);
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

    /**
     * Convenience method for launching this Activity.
     * @param activity Activity that is launching this the Downloads page.
     */
    public static void launch(Activity activity) {
        Intent intent = new Intent();
        intent.setClass(activity, DownloadActivity.class);
        intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
        activity.startActivity(intent);
    }
}
