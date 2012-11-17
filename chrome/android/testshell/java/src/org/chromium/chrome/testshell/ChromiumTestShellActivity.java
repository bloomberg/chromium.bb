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

import org.chromium.chrome.browser.DevToolsServer;
import org.chromium.chrome.browser.TabBase;
import org.chromium.content.app.AppResource;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.DeviceUtils;
import org.chromium.content.common.CommandLine;
import org.chromium.ui.gfx.ActivityNativeWindow;

/**
 * The {@link Activity} component of a basic test shell to test Chrome features.
 */
public class ChromiumTestShellActivity extends Activity {
    private static final String TAG = ChromiumTestShellActivity.class.getCanonicalName();
    private static final String COMMAND_LINE_FILE =
            "/data/local/tmp/chromium-testshell-command-line";

    private ActivityNativeWindow mWindow;
    private TabManager mTabManager;
    private DevToolsServer mDevToolsServer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (!CommandLine.isInitialized()) CommandLine.initFromFile(COMMAND_LINE_FILE);
        waitForDebuggerIfNeeded();

        DeviceUtils.addDeviceSpecificUserAgentSwitch(this);

        initializeContentViewResources();
        ContentView.initChromiumBrowserProcess(this, ContentView.MAX_RENDERERS_AUTOMATIC);

        setContentView(R.layout.testshell_activity);
        mTabManager = (TabManager) findViewById(R.id.tab_manager);
        String startupUrl = getUrlFromIntent(getIntent());
        if (!TextUtils.isEmpty(startupUrl)) {
            mTabManager.setStartupUrl(startupUrl);
        }

        mWindow = new ActivityNativeWindow(this);
        mWindow.restoreInstanceState(savedInstanceState);
        mTabManager.setWindow(mWindow);

        mDevToolsServer = new DevToolsServer(true, "chromium_testshell_devtools_remote");
        mDevToolsServer.setRemoteDebuggingEnabled(true);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        mDevToolsServer.destroy();
        mDevToolsServer = null;
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

    /**
     * @return The {@link TabBase} that is currently visible.
     */
    public TabBase getActiveTab() {
        return mTabManager != null ? mTabManager.getCurrentTab() : null;
    }

    /**
     * @return The ContentView of the active tab.
     */
    public ContentView getActiveContentView() {
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
        AppResource.DRAWABLE_ICON_ACTION_BAR_SHARE = R.drawable.ic_menu_share_holo_light;
        AppResource.DRAWABLE_ICON_ACTION_BAR_WEB_SEARCH = R.drawable.ic_menu_search_holo_light;
        AppResource.DRAWABLE_LINK_PREVIEW_POPUP_OVERLAY = R.drawable.popup_zoomer_overlay;
        AppResource.ID_AUTOFILL_LABEL = R.id.autofill_label;
        AppResource.ID_AUTOFILL_NAME = R.id.autofill_name;
        AppResource.LAYOUT_AUTOFILL_TEXT = R.layout.autofill_text;
        AppResource.STRING_ACTION_BAR_SHARE = R.string.action_bar_share;
        AppResource.STRING_ACTION_BAR_WEB_SEARCH = R.string.action_bar_search;
        AppResource.STRING_CONTENT_VIEW_CONTENT_DESCRIPTION = R.string.accessibility_content_view;
        AppResource.STRING_MEDIA_PLAYER_MESSAGE_PLAYBACK_ERROR =
                R.string.media_player_error_text_invalid_progressive_playback;
        AppResource.STRING_MEDIA_PLAYER_MESSAGE_UNKNOWN_ERROR =
                R.string.media_player_error_text_unknown;
        AppResource.STRING_MEDIA_PLAYER_ERROR_BUTTON = R.string.media_player_error_button;
        AppResource.STRING_MEDIA_PLAYER_ERROR_TITLE = R.string.media_player_error_title;
        AppResource.STRING_MEDIA_PLAYER_LOADING_VIDEO = R.string.media_player_loading_video;
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
