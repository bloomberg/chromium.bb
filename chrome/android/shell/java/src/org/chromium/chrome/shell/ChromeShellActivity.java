// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.app.Activity;
import android.content.Intent;
import android.content.res.TypedArray;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.DevToolsServer;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.printing.PrintingControllerFactory;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.shell.sync.SyncController;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content.browser.ActivityContentVideoViewClient;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.DeviceUtils;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

/**
 * The {@link android.app.Activity} component of a basic test shell to test Chrome features.
 */
public class ChromeShellActivity extends Activity implements AppMenuPropertiesDelegate {
    private static final String TAG = "ChromeShellActivity";
    private static final String CHROME_DISTILLER_SCHEME = "chrome-distiller";

    /**
     * Factory used to set up a mock ActivityWindowAndroid for testing.
     */
    public interface WindowAndroidFactoryForTest {
        /**
         * @return ActivityWindowAndroid for the given activity.
         */
        public ActivityWindowAndroid getActivityWindowAndroid(Activity activity);
    }

    private static WindowAndroidFactoryForTest sWindowAndroidFactory =
            new WindowAndroidFactoryForTest() {
                @Override
                public ActivityWindowAndroid getActivityWindowAndroid(Activity activity) {
                    return new ActivityWindowAndroid(activity);
                }
            };
    private WindowAndroid mWindow;
    private TabManager mTabManager;
    private DevToolsServer mDevToolsServer;
    private SyncController mSyncController;
    private PrintingController mPrintingController;

    private AppMenuHandler mAppMenuHandler;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ChromeShellApplication.initCommandLine();
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
                        Toast.makeText(ChromeShellActivity.this,
                                       R.string.browser_process_initialization_failed,
                                       Toast.LENGTH_SHORT).show();
                        Log.e(TAG, "Chromium browser process initialization failed");
                        finish();
                    }
                };
        try {
            BrowserStartupController.get(this).startBrowserProcessesAsync(callback);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to load native library.", e);
            System.exit(-1);
        }
    }

    private void finishInitialization(final Bundle savedInstanceState) {
        setContentView(R.layout.chrome_shell_activity);
        mTabManager = (TabManager) findViewById(R.id.tab_manager);

        mWindow = sWindowAndroidFactory.getActivityWindowAndroid(this);
        mWindow.restoreInstanceState(savedInstanceState);
        mTabManager.initialize(mWindow, new ActivityContentVideoViewClient(this));

        String startupUrl = getUrlFromIntent(getIntent());
        if (!TextUtils.isEmpty(startupUrl)) {
            mTabManager.setStartupUrl(startupUrl);
        }
        ChromeShellToolbar mToolbar = (ChromeShellToolbar) findViewById(R.id.toolbar);
        mAppMenuHandler = new AppMenuHandler(this, this, R.menu.main_menu);
        mToolbar.setMenuHandler(mAppMenuHandler);

        mDevToolsServer = new DevToolsServer("chrome_shell");
        mDevToolsServer.setRemoteDebuggingEnabled(true);

        mPrintingController = PrintingControllerFactory.create(this);

        mSyncController = SyncController.get(this);
        // In case this method is called after the first onStart(), we need to inform the
        // SyncController that we have started.
        mSyncController.onStart();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (mDevToolsServer != null) mDevToolsServer.destroy();
        mDevToolsServer = null;
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        // TODO(dtrainor): Save/restore the tab state.
        if (mWindow != null) mWindow.saveInstanceState(outState);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            ChromeShellTab tab = getActiveTab();
            if (tab != null && tab.canGoBack()) {
                tab.goBack();
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
            ChromeShellTab tab = getActiveTab();
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
     * @return The {@link WindowAndroid} associated with this activity.
     */
    public WindowAndroid getWindowAndroid() {
        return mWindow;
    }

    /**
     * @return The {@link ChromeShellTab} that is currently visible.
     */
    public ChromeShellTab getActiveTab() {
        return mTabManager != null ? mTabManager.getCurrentTab() : null;
    }

    /**
     * @return The ContentView of the active tab.
     */
    public ContentView getActiveContentView() {
        ChromeShellTab tab = getActiveTab();
        return tab != null ? tab.getContentView() : null;
    }

    /**
     * Creates a {@link ChromeShellTab} with a URL specified by {@code url}.
     *
     * @param url The URL the new {@link ChromeShellTab} should start with.
     */
    @VisibleForTesting
    public void createTab(String url) {
        mTabManager.createTab(url);
    }

    /**
     * Override the menu key event to show AppMenu.
     */
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_MENU && event.getRepeatCount() == 0) {
            mAppMenuHandler.showAppMenu(findViewById(R.id.menu_button), true, false);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        ChromeShellTab activeTab = getActiveTab();
        switch (item.getItemId()) {
            case R.id.signin:
                if (ChromeSigninController.get(this).isSignedIn()) {
                    SyncController.openSignOutDialog(getFragmentManager());
                } else {
                    SyncController.openSigninDialog(getFragmentManager());
                }
                return true;
            case R.id.print:
                if (activeTab != null) {
                    mPrintingController.startPrint(new TabPrinter(activeTab),
                            new PrintManagerDelegateImpl(this));
                }
                return true;
            case R.id.distill_page:
                if (activeTab != null) {
                    String viewUrl = DomDistillerUrlUtils.getDistillerViewUrlFromUrl(
                            CHROME_DISTILLER_SCHEME, activeTab.getUrl());
                    activeTab.loadUrlWithSanitization(viewUrl);
                }
                return true;
            case R.id.back_menu_id:
                if (activeTab != null && activeTab.canGoBack()) {
                    activeTab.goBack();
                }
                return true;
            case R.id.forward_menu_id:
                if (activeTab != null && activeTab.canGoForward()) {
                    activeTab.goForward();
                }
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    private void waitForDebuggerIfNeeded() {
        if (CommandLine.getInstance().hasSwitch(BaseSwitches.WAIT_FOR_JAVA_DEBUGGER)) {
            Log.e(TAG, "Waiting for Java debugger to connect...");
            android.os.Debug.waitForDebugger();
            Log.e(TAG, "Java debugger connected. Resuming execution.");
        }
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    @Override
    public boolean shouldShowAppMenu() {
        return true;
    }

    @Override
    public void prepareMenu(Menu menu) {
        // Disable the "Back" menu item if there is no page to go to.
        MenuItem backMenuItem = menu.findItem(R.id.back_menu_id);
        backMenuItem.setEnabled(getActiveTab().canGoBack());

        // Disable the "Forward" menu item if there is no page to go to.
        MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
        forwardMenuItem.setEnabled(getActiveTab().canGoForward());

        // ChromeShell does not know about bookmarks yet
        menu.findItem(R.id.bookmark_this_page_id).setEnabled(false);

        MenuItem signinItem = menu.findItem(R.id.signin);
        if (ChromeSigninController.get(this).isSignedIn()) {
            signinItem.setTitle(ChromeSigninController.get(this).getSignedInAccountName());
        } else {
            signinItem.setTitle(R.string.signin_sign_in);
        }

        menu.findItem(R.id.print).setVisible(ApiCompatibilityUtils.isPrintingSupported());

        menu.findItem(R.id.distill_page).setVisible(
                CommandLine.getInstance().hasSwitch(ChromeShellSwitches.ENABLE_DOM_DISTILLER));

        menu.setGroupVisible(R.id.MAIN_MENU, true);
    }

    @Override
    public boolean shouldShowIconRow() {
        return true;
    }

    @Override
    public int getMenuThemeResourceId() {
        return android.R.style.Theme_Holo_Light;
    }

    @Override
    public int getItemRowHeight() {
        TypedArray a = obtainStyledAttributes(
                new int[] {android.R.attr.listPreferredItemHeightSmall});
        int itemRowHeight = a.getDimensionPixelSize(0, 0);
        a.recycle();
        return itemRowHeight;
    }

    @VisibleForTesting
    public static void setActivityWindowAndroidFactory(WindowAndroidFactoryForTest factory) {
        sWindowAndroidFactory = factory;
    }
}
