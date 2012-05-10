// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Debug;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;

import org.chromium.content.browser.CommandLine;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.LibraryLoader;

/**
 * Activity for managing the Content Shell.
 */
public class ContentShellActivity extends Activity {

    private static final String COMMAND_LINE_FILE = "/data/local/content-shell-command-line";
    private static final String TAG = "ContentShellActivity";
    private static final String NATIVE_LIBRARY = "content_shell_content_view";

    private ShellManager mShellManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initializing the command line must occur before loading the library.
        CommandLine.initFromFile(COMMAND_LINE_FILE);
        String startupUrl = getUrlFromIntent(getIntent());
        if (!TextUtils.isEmpty(startupUrl)) {
            CommandLine.getInstance().appendSwitchesAndArguments(
                    new String[] {ShellView.sanitizeUrl(startupUrl)});
        }

        // TODO(jrg): once command line support is addef (for
        // --wait-for-debugger), remove this.
        // Debug.waitForDebugger();
        LibraryLoader.loadAndInitSync();
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
