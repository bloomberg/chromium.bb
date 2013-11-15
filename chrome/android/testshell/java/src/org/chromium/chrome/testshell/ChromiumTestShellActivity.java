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
import android.widget.Toast;

import org.chromium.base.MemoryPressureListener;
import org.chromium.chrome.browser.DevToolsServer;
import org.chromium.chrome.testshell.sync.SyncController;
import org.chromium.content.browser.ActivityContentVideoViewClient;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.ContentVideoViewClient;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.DeviceUtils;
import org.chromium.content.common.CommandLine;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

/**
 * The {@link android.app.Activity} component of a basic test shell to test Chrome features.
 */
public class ChromiumTestShellActivity extends Activity implements MenuHandler {
    private static final String TAG = "ChromiumTestShellActivity";

    private WindowAndroid mWindow;
    private TabManager mTabManager;
    private DevToolsServer mDevToolsServer;
    private SyncController mSyncController;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ChromiumTestShellApplication.initCommandLine();
        waitForDebuggerIfNeeded();

        DeviceUtils.addDeviceSpecificUserAgentSwitch(this);

        BrowserStartupController.StartupCallback callback =
                new BrowserStartupController.StartupCallback() {
                    @Override
                    public void onSuccess(boolean alreadyStarted) {
                        finishInitialization(savedInstanceState);
                    }

                    @Override
                    public void onFailure() {
                        Toast.makeText(ChromiumTestShellActivity.this,
                                       R.string.browser_process_initialization_failed,
                                       Toast.LENGTH_SHORT).show();
                        Log.e(TAG, "Chromium browser process initialization failed");
                        finish();
                    }
                };
        BrowserStartupController.get(this).startBrowserProcessesAsync(callback);
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

        mWindow = new ActivityWindowAndroid(this);
        mWindow.restoreInstanceState(savedInstanceState);
        mTabManager.setWindow(mWindow);

        mDevToolsServer = new DevToolsServer("chromium_testshell");
        mDevToolsServer.setRemoteDebuggingEnabled(true);
        mSyncController = SyncController.get(this);
        // In case this method is called after the first onStart(), we need to inform the
        // SyncController that we have started.
        mSyncController.onStart();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (mDevToolsServer != null)
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
        if (MemoryPressureListener.handleDebugIntent(this, intent.getAction())) return;

        String url = getUrlFromIntent(intent);
        if (!TextUtils.isEmpty(url)) {
            TestShellTab tab = getActiveTab();
            if (tab != null) tab.loadUrlWithSanitization(url);
        }
    }

    @Override
    protected void onStop() {
        super.onStop();

        ContentView view = getActiveContentView();
        if (view != null) view.onHide();
    }

    @Override
    protected void onStart() {
        super.onStart();

        ContentView view = getActiveContentView();
        if (view != null) view.onShow();

        if (mSyncController != null) {
            mSyncController.onStart();
        }
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
