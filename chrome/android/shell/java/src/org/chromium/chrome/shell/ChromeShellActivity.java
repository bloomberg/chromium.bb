// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.DevToolsServer;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.printing.PrintingControllerFactory;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.shell.sync.SyncController;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content.browser.ActivityContentVideoViewClient;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.DeviceUtils;
import org.chromium.content.common.ContentSwitches;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;

/**
 * The {@link android.app.Activity} component of a basic test shell to test Chrome features.
 */
public class ChromeShellActivity extends Activity implements AppMenuPropertiesDelegate {
    private static final String TAG = "ChromeShellActivity";

    /**
     * Factory used to set up a mock ActivityWindowAndroid for testing.
     */
    public interface ActivityWindowAndroidFactory {
        /**
         * @return ActivityWindowAndroid for the given activity.
         */
        public ActivityWindowAndroid getActivityWindowAndroid(Activity activity);
    }

    private static ActivityWindowAndroidFactory sWindowAndroidFactory =
            new ActivityWindowAndroidFactory() {
                @Override
                public ActivityWindowAndroid getActivityWindowAndroid(Activity activity) {
                    return new ActivityWindowAndroid(activity);
                }
            };

    private WindowAndroid mWindow;
    private TabManager mTabManager;
    private ChromeShellToolbar mToolbar;
    private DevToolsServer mDevToolsServer;
    private SyncController mSyncController;
    private PrintingController mPrintingController;

    /**
     * Factory used to set up a mock AppMenuHandler for testing.
     */
    public interface AppMenuHandlerFactory {
        /**
         * @return AppMenuHandler for the given activity and menu resource id.
         */
        public AppMenuHandler getAppMenuHandler(Activity activity,
                AppMenuPropertiesDelegate delegate, int menuResourceId);
    }

    private static AppMenuHandlerFactory sAppMenuHandlerFactory =
            new AppMenuHandlerFactory() {
                @Override
                public AppMenuHandler getAppMenuHandler(Activity activity,
                        AppMenuPropertiesDelegate delegate, int menuResourceId) {
                    return new AppMenuHandler(activity, delegate, menuResourceId);
                }
            };
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
        mTabManager.initialize(mWindow, new ActivityContentVideoViewClient(this) {
            @Override
            public boolean onShowCustomView(View view) {
                if (mTabManager == null) return false;
                boolean success = super.onShowCustomView(view);
                if (!CommandLine.getInstance().hasSwitch(
                        ContentSwitches.DISABLE_OVERLAY_FULLSCREEN_VIDEO_SUBTITLE)) {
                    mTabManager.setOverlayVideoMode(true);
                }
                return success;
            }

            @Override
            public void onDestroyContentVideoView() {
                super.onDestroyContentVideoView();
                if (mTabManager != null && !CommandLine.getInstance().hasSwitch(
                        ContentSwitches.DISABLE_OVERLAY_FULLSCREEN_VIDEO_SUBTITLE)) {
                    mTabManager.setOverlayVideoMode(false);
                }
            }
        });

        String startupUrl = getUrlFromIntent(getIntent());
        if (!TextUtils.isEmpty(startupUrl)) {
            mTabManager.setStartupUrl(startupUrl);
        }
        mToolbar = (ChromeShellToolbar) findViewById(R.id.toolbar);
        mAppMenuHandler = sAppMenuHandlerFactory.getAppMenuHandler(this, this, R.menu.main_menu);
        mToolbar.setMenuHandler(mAppMenuHandler);

        mDevToolsServer = new DevToolsServer("chrome_shell");
        mDevToolsServer.setRemoteDebuggingEnabled(true);

        mPrintingController = PrintingControllerFactory.create(this);

        mSyncController = SyncController.get(this);
        // In case this method is called after the first onStart(), we need to inform the
        // SyncController that we have started.
        mSyncController.onStart();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());
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

        if (mToolbar != null) mToolbar.hideSuggestions();

        ContentViewCore viewCore = getActiveContentViewCore();
        if (viewCore != null) viewCore.onHide();
    }

    @Override
    protected void onStart() {
        super.onStart();

        ContentViewCore viewCore = getActiveContentViewCore();
        if (viewCore != null) viewCore.onShow();

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
     * @return The ContentViewCore of the active tab.
     */
    public ContentViewCore getActiveContentViewCore() {
        ChromeShellTab tab = getActiveTab();
        return tab != null ? tab.getContentViewCore() : null;
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

    @SuppressWarnings("deprecation")
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        ChromeShellTab activeTab = getActiveTab();
        switch (item.getItemId()) {
            case R.id.signin:
                if (ChromeSigninController.get(this).isSignedIn()) {
                    SyncController.openSignOutDialog(getFragmentManager());
                } else if (AccountManagerHelper.get(this).hasGoogleAccounts()) {
                    SyncController.openSigninDialog(getFragmentManager());
                } else {
                    Toast.makeText(this, R.string.signin_no_account, Toast.LENGTH_SHORT).show();
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
                    DomDistillerTabUtils.distillCurrentPageAndView(
                            activeTab.getContentViewCore().getWebContents());
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
            case R.id.share_menu_id:
            case R.id.direct_share_menu_id:
                ShareHelper.share(item.getItemId() == R.id.direct_share_menu_id, this,
                        activeTab.getTitle(), activeTab.getUrl(), null,
                        Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
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
        menu.setGroupVisible(R.id.MAIN_MENU, true);
        ChromeShellTab activeTab = getActiveTab();

        // Disable the "Back" menu item if there is no page to go to.
        MenuItem backMenuItem = menu.findItem(R.id.back_menu_id);
        backMenuItem.setEnabled(activeTab != null ? activeTab.canGoBack() : false);

        // Disable the "Forward" menu item if there is no page to go to.
        MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
        forwardMenuItem.setEnabled(activeTab != null ? activeTab.canGoForward() : false);

        // ChromeShell does not know about bookmarks yet
        menu.findItem(R.id.bookmark_this_page_id).setEnabled(true);

        MenuItem signinItem = menu.findItem(R.id.signin);
        if (ChromeSigninController.get(this).isSignedIn()) {
            signinItem.setTitle(ChromeSigninController.get(this).getSignedInAccountName());
        } else {
            signinItem.setTitle(R.string.signin_sign_in);
        }

        menu.findItem(R.id.print).setVisible(ApiCompatibilityUtils.isPrintingSupported());

        MenuItem distillPageItem = menu.findItem(R.id.distill_page);
        if (CommandLine.getInstance().hasSwitch(ChromeShellSwitches.ENABLE_DOM_DISTILLER)) {
            String url = activeTab != null ? activeTab.getUrl() : null;
            distillPageItem.setEnabled(!DomDistillerUrlUtils.isDistilledPage(url));
            distillPageItem.setVisible(true);
        } else {
            distillPageItem.setVisible(false);
        }
        ShareHelper.configureDirectShareMenuItem(this, menu.findItem(R.id.direct_share_menu_id));
    }

    @VisibleForTesting
    public AppMenuHandler getAppMenuHandler() {
        return mAppMenuHandler;
    }

    @Override
    public int getMenuThemeResourceId() {
        return R.style.OverflowMenuTheme;
    }

    @VisibleForTesting
    public static void setActivityWindowAndroidFactory(ActivityWindowAndroidFactory factory) {
        sWindowAndroidFactory = factory;
    }

    @VisibleForTesting
    public static void setAppMenuHandlerFactory(AppMenuHandlerFactory factory) {
        sAppMenuHandlerFactory = factory;
    }
}
