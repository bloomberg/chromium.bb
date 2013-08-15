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
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.chromium.base.ChromiumActivity;
import org.chromium.base.MemoryPressureListener;
import org.chromium.chrome.browser.DevToolsServer;
import org.chromium.chrome.testshell.sync.SyncController;
import org.chromium.content.browser.ActivityContentVideoViewClient;
import org.chromium.content.browser.AndroidBrowserProcess;
import org.chromium.content.browser.BrowserStartupConfig;
import org.chromium.content.browser.ContentVideoViewClient;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.DeviceUtils;
import org.chromium.content.common.CommandLine;
import org.chromium.content.common.ProcessInitException;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.ui.WindowAndroid;

/**
 * The {@link Activity} component of a basic test shell to test Chrome features.
 */
public class ChromiumTestShellActivity extends ChromiumActivity implements MenuHandler {
    private static final String TAG = "ChromiumTestShellActivity";
    private static final String COMMAND_LINE_FILE =
            "/data/local/tmp/chromium-testshell-command-line";
    /**
     * Sending an intent with this action will simulate a memory pressure signal
     * at a critical level.
     */
    private static final String ACTION_LOW_MEMORY =
            "org.chromium.chrome_test_shell.action.ACTION_LOW_MEMORY";

    /**
     * Sending an intent with this action will simulate a memory pressure signal
     * at a moderate level.
     */
    private static final String ACTION_TRIM_MEMORY_MODERATE =
            "org.chromium.chrome_test_shell.action.ACTION_TRIM_MEMORY_MODERATE";

    private WindowAndroid mWindow;
    private TabManager mTabManager;
    private DevToolsServer mDevToolsServer;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (!CommandLine.isInitialized()) CommandLine.initFromFile(COMMAND_LINE_FILE);
        waitForDebuggerIfNeeded();

        DeviceUtils.addDeviceSpecificUserAgentSwitch(this);

        BrowserStartupConfig.setAsync(new BrowserStartupConfig.StartupCallback() {
            @Override
            public void run(int startupResult) {
                if (startupResult > 0) {
                    // TODO: Show error message.
                    Log.e(TAG, "Chromium browser process initialization failed");
                    finish();
                } else {
                    finishInitialization(savedInstanceState);
                }
            }
        });

        try {
            if (!AndroidBrowserProcess.init(this, AndroidBrowserProcess.MAX_RENDERERS_LIMIT)) {
                // Process was already running, finish initialization now.
                finishInitialization(savedInstanceState);
            }
        } catch (ProcessInitException e) {
            Log.e(TAG, "Chromium browser process initialization failed", e);
            finish();
        }
    }

    private void finishInitialization(final Bundle savedInstanceState) {
        setContentView(R.layout.testshell_activity);
        mTabManager = (TabManager) findViewById(R.id.tab_manager);
        String startupUrl = getUrlFromIntent(getIntent());
        if (!TextUtils.isEmpty(startupUrl)) {
            mTabManager.setStartupUrl(startupUrl);
        }
        TestShellToolbar mToolbar = (TestShellToolbar) findViewById(R.id.toolbar);
        mToolbar.setMenuHandler(this);

        mWindow = new WindowAndroid(this);
        mWindow.restoreInstanceState(savedInstanceState);
        mTabManager.setWindow(mWindow);

        mDevToolsServer = new DevToolsServer("chromium_testshell");
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
            TestShellTab tab = getActiveTab();
            if (tab != null && tab.getContentView().canGoBack()) {
                tab.getContentView().goBack();
                return true;
            }
        }

        return super.onKeyUp(keyCode, event);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (ACTION_LOW_MEMORY.equals(intent.getAction())) {
            MemoryPressureListener.simulateMemoryPressureSignal(TRIM_MEMORY_COMPLETE);
            return;
        } else if (ACTION_TRIM_MEMORY_MODERATE.equals(intent.getAction())) {
            MemoryPressureListener.simulateMemoryPressureSignal(TRIM_MEMORY_MODERATE);
            return;
        }

        String url = getUrlFromIntent(intent);
        if (!TextUtils.isEmpty(url)) {
            TestShellTab tab = getActiveTab();
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
     * @return The {@link TestShellTab} that is currently visible.
     */
    public TestShellTab getActiveTab() {
        return mTabManager != null ? mTabManager.getCurrentTab() : null;
    }

    /**
     * @return The ContentView of the active tab.
     */
    public ContentView getActiveContentView() {
        TestShellTab tab = getActiveTab();
        return tab != null ? tab.getContentView() : null;
    }

    /**
     * Creates a {@link TestShellTab} with a URL specified by {@code url}.
     *
     * @param url The URL the new {@link TestShellTab} should start with.
     */
    public void createTab(String url) {
        mTabManager.createTab(url);
        getActiveContentView().setContentViewClient(new ContentViewClient() {
            @Override
            public ContentVideoViewClient getContentVideoViewClient() {
                return new ActivityContentVideoViewClient(ChromiumTestShellActivity.this);
            }
        });
    }

    /**
     * From {@link MenuHandler}.
     */
    @Override
    public void showPopupMenu() {
        openOptionsMenu();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.signin:
                if (ChromeSigninController.get(this).isSignedIn())
                    SyncController.openSignOutDialog(getFragmentManager());
                else
                    SyncController.openSigninDialog(getFragmentManager());
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        menu.setGroupVisible(R.id.MAIN_MENU, true);
        MenuItem signinItem = menu.findItem(R.id.signin);
        if (ChromeSigninController.get(this).isSignedIn())
            signinItem.setTitle(ChromeSigninController.get(this).getSignedInAccountName());
        else
            signinItem.setTitle(R.string.signin_sign_in);
        return true;
    }

    private void waitForDebuggerIfNeeded() {
        if (CommandLine.getInstance().hasSwitch(CommandLine.WAIT_FOR_JAVA_DEBUGGER)) {
            Log.e(TAG, "Waiting for Java debugger to connect...");
            android.os.Debug.waitForDebugger();
            Log.e(TAG, "Java debugger connected. Resuming execution.");
        }
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
