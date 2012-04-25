// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.KeyEvent;

import org.chromium.content.browser.ContentView;

/**
 * Activity for managing the Content Shell.
 */
public class ContentShellActivity extends Activity {

    private static final String COMMAND_LINE_FILE = "/data/local/content-shell-command-line";

    private ShellManager mShellManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initializing the command line must occur before loading the library.
        // TODO(tedchoc): Initialize command line from file.
        String startupUrl = getUrlFromIntent(getIntent());
        if (!TextUtils.isEmpty(startupUrl)) {
            // TODO(tedchoc): Append URL to command line.
        }

        // TODO(tedchoc): Load the native library.
        initializeContentViewResources();

        setContentView(R.layout.content_shell_activity);
        mShellManager = (ShellManager) findViewById(R.id.shell_container);
        ContentView.enableMultiProcess(this, ContentView.MAX_RENDERERS_AUTOMATIC);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode != KeyEvent.KEYCODE_BACK) return super.onKeyUp(keyCode, event);

        ShellView activeView = getActiveShellView();
        if (activeView != null && activeView.getContentView().canGoBack()) {
            activeView.getContentView().goBack();
            return true;
        }

        return super.onKeyUp(keyCode, event);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        String url = getUrlFromIntent(intent);
        if (!TextUtils.isEmpty(url)) {
            ShellView activeView = getActiveShellView();
            if (activeView != null) {
                activeView.loadUrl(url);
            }
        }
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    /**
     * @return The {@link ShellManager} configured for the activity or null if it has not been
     *         created yet.
     */
    public ShellManager getShellManager() {
        return mShellManager;
    }

    /**
     * @return The currently visible {@link ShellView} or null if one is not showing.
     */
    public ShellView getActiveShellView() {
        return mShellManager != null ? mShellManager.getActiveShellView() : null;
    }

    private void initializeContentViewResources() {
        ContentView.registerPopupOverlayCornerRadius(0);
        ContentView.registerPopupOverlayResourceId(R.drawable.popup_zoomer_overlay);
    }
}
