// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;

import org.chromium.chrome.browser.TabBase;
import org.chromium.content.app.AppResource;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.browser.ContentView;
import org.chromium.content.common.CommandLine;
import org.chromium.ui.gfx.ActivityNativeWindow;

/**
 * The {@link Activity} component of a basic test shell to test Chrome features.
 */
public class ChromiumTestShellActivity extends Activity {
    private static final String TAG = ChromiumTestShellActivity.class.getCanonicalName();
    private static final String COMMAND_LINE_FILE =
            "/data/local/tmp/chrome-test-shell-command-line";

    private ActivityNativeWindow mWindow;
    private TabManager mTabManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (!CommandLine.isInitialized()) CommandLine.initFromFile(COMMAND_LINE_FILE);
        waitForDebuggerIfNeeded();

        initializeContentViewResources();
        ContentView.initChromiumBrowserProcess(this, ContentView.MAX_RENDERERS_AUTOMATIC);

        setContentView(R.layout.testshell_activity);
        mTabManager = (TabManager) findViewById(R.id.tab_manager);

        mWindow = new ActivityNativeWindow(this);
        mWindow.restoreInstanceState(savedInstanceState);
        mTabManager.setWindow(mWindow);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        // TODO(dtrainor): Save/restore the tab state.
        mWindow.saveInstanceState(outState);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            TabBase tab = getActiveTab();
            if (tab != null && tab.getContentView().canGoBack()) {
                tab.getContentView().goBack();
                return true;
            }
        }

        return super.onKeyUp(keyCode, event);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        String url = getUrlFromIntent(intent);
        if (!TextUtils.isEmpty(url)) {
            TabBase tab = getActiveTab();
            if (tab != null) tab.loadUrlWithSanitization(url);
        }
    }

    @Override
    protected void onPause() {
        ContentView view = getActiveContentView();
        if (view != null) view.onActivityPause();

        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();

        ContentView view = getActiveContentView();
        if (view != null) view.onActivityResume();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        mWindow.onActivityResult(requestCode, resultCode, data);
    }

    private TabBase getActiveTab() {
        return mTabManager != null ? mTabManager.getCurrentTab() : null;
    }

    private ContentView getActiveContentView() {
        TabBase tab = getActiveTab();
        return tab != null ? tab.getContentView() : null;
    }

    private void waitForDebuggerIfNeeded() {
        if (CommandLine.getInstance().hasSwitch(CommandLine.WAIT_FOR_JAVA_DEBUGGER)) {
            Log.e(TAG, "Waiting for Java debugger to connect...");
            android.os.Debug.waitForDebugger();
            Log.e(TAG, "Java debugger connected. Resuming execution.");
        }
    }

    private void initializeContentViewResources() {
        AppResource.DIMENSION_LINK_PREVIEW_OVERLAY_RADIUS = R.dimen.link_preview_overlay_radius;
        AppResource.DRAWABLE_LINK_PREVIEW_POPUP_OVERLAY = R.drawable.popup_zoomer_overlay;
        AppResource.STRING_CONTENT_VIEW_CONTENT_DESCRIPTION = R.string.accessibility_content_view;
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}