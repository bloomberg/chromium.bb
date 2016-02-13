// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.IBinder;
import android.os.StrictMode;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsIntent;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.Window;
import android.widget.ImageButton;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocument;
import org.chromium.chrome.browser.datausage.DataUseTabUIManager;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.rappor.RapporServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;

import java.util.List;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends ChromeActivity {
    private static final String TAG = "CustomTabActivity";

    private static CustomTabContentHandler sActiveContentHandler;

    private FindToolbarManager mFindToolbarManager;
    private CustomTabIntentDataProvider mIntentDataProvider;
    private IBinder mSession;
    private CustomTabContentHandler mCustomTabContentHandler;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    private boolean mShouldOverridePackage;

    private boolean mRecordedStartupUma;
    private boolean mShouldReplaceCurrentEntry;
    private CustomTabObserver mTabObserver;

    // Only the normal tab model is observed because there is no icognito tabmodel in Custom Tabs.
    private TabModelObserver mTabModelObserver = new EmptyTabModelObserver() {
        @Override
        public void didAddTab(Tab tab, TabLaunchType type) {
            if (mTabObserver == null) {
                mTabObserver = new CustomTabObserver(getApplication(), mSession);
            }
            tab.addObserver(mTabObserver);
        }

        @Override
        public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
            // TODO(ianwen): If the user selects an background tab by keyboard
            // shortcuts, close all tabs that are above the selected tab.
            // http://crbug.com/500058
        }
    };

    /**
     * Sets the currently active {@link CustomTabContentHandler} in focus.
     * @param contentHandler {@link CustomTabContentHandler} to set.
     */
    public static void setActiveContentHandler(CustomTabContentHandler contentHandler) {
        sActiveContentHandler = contentHandler;
    }

    /**
     * Used to check whether an incoming intent can be handled by the
     * current {@link CustomTabContentHandler}.
     * @return Whether the active {@link CustomTabContentHandler} has handled the intent.
     */
    public static boolean handleInActiveContentIfNeeded(Intent intent) {
        if (sActiveContentHandler == null) return false;

        if (sActiveContentHandler.shouldIgnoreIntent(intent)) {
            Log.w(TAG, "Incoming intent to Custom Tab was ignored.");
            return false;
        }

        IBinder session = IntentUtils.safeGetBinderExtra(intent, CustomTabsIntent.EXTRA_SESSION);
        if (session == null || !session.equals(sActiveContentHandler.getSession())) return false;

        String url = IntentHandler.getUrlFromIntent(intent);
        if (TextUtils.isEmpty(url)) return false;
        sActiveContentHandler.loadUrlAndTrackFromTimestamp(new LoadUrlParams(url),
                IntentHandler.getTimestampFromIntent(intent));
        return true;
    }

    /**
     * Checks whether the active {@link CustomTabContentHandler} belongs to the given session, and
     * if true, update toolbar's custom button.
     * @param session     The {@link IBinder} that the calling client represents.
     * @param bitmap      The new icon for action button.
     * @param description The new content description for the action button.
     * @return Whether the update is successful.
     */
    static boolean updateCustomButton(IBinder session, int id, Bitmap bitmap, String description) {
        ThreadUtils.assertOnUiThread();
        // Do nothing if there is no activity or the activity does not belong to this session.
        if (sActiveContentHandler == null || !sActiveContentHandler.getSession().equals(session)) {
            return false;
        }
        return sActiveContentHandler.updateCustomButton(id, bitmap, description);
    }

    @Override
    public boolean isCustomTab() {
        return true;
    }

    @Override
    public void onStart() {
        super.onStart();
        CustomTabsConnection.getInstance(getApplication())
                .keepAliveForSession(mIntentDataProvider.getSession(),
                        mIntentDataProvider.getKeepAliveServiceIntent());
    }

    @Override
    public void onStop() {
        super.onStop();
        CustomTabsConnection.getInstance(getApplication())
                .dontKeepAliveForSession(mIntentDataProvider.getSession());
    }

    @Override
    public void preInflationStartup() {
        super.preInflationStartup();
        mIntentDataProvider = new CustomTabIntentDataProvider(getIntent(), this);
        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);
    }

    @Override
    public void postInflationStartup() {
        super.postInflationStartup();
        setTabModelSelector(new TabModelSelectorImpl(this,
                TabModelSelectorImpl.CUSTOM_TABS_SELECTOR_INDEX, getWindowAndroid(), false));
        getToolbarManager().setCloseButtonDrawable(mIntentDataProvider.getCloseButtonDrawable());
        getToolbarManager().setShowTitle(mIntentDataProvider.getTitleVisibilityState()
                == CustomTabsIntent.SHOW_PAGE_TITLE);
        int toolbarColor = mIntentDataProvider.getToolbarColor();
        getToolbarManager().updatePrimaryColor(toolbarColor);
        if (toolbarColor != ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_primary_color)) {
            ApiCompatibilityUtils.setStatusBarColor(getWindow(),
                    ColorUtils.getDarkenedColorForStatusBar(toolbarColor));
        }

        // Setting task title and icon to be null will preserve the client app's title and icon.
        ApiCompatibilityUtils.setTaskDescription(this, null, null, toolbarColor);
        showCustomButtonOnToolbar();
        showBottomBarIfNecessary();
    }

    @Override
    public void finishNativeInitialization() {
        mSession = mIntentDataProvider.getSession();
        // If extra headers have been passed, cancel any current prerender, as
        // prerendering doesn't support extra headers.
        if (IntentHandler.getExtraHeadersFromIntent(getIntent()) != null) {
            CustomTabsConnection.getInstance(getApplication()).cancelPrerender(mSession);
        }
        Tab mainTab = createMainTab();
        getTabModelSelector().getModel(false).addObserver(mTabModelObserver);
        getTabModelSelector().getModel(false).addTab(mainTab, 0, mainTab.getLaunchType());

        ToolbarControlContainer controlContainer = (ToolbarControlContainer) findViewById(
                R.id.control_container);
        LayoutManagerDocument layoutDriver = new CustomTabLayoutManager(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content), controlContainer);
        mFindToolbarManager = new FindToolbarManager(this, getTabModelSelector(),
                getToolbarManager().getActionModeController().getActionModeCallback());
        if (getContextualSearchManager() != null) {
            getContextualSearchManager().setFindToolbarManager(mFindToolbarManager);
        }
        getToolbarManager().initializeWithNative(getTabModelSelector(), getFullscreenManager(),
                mFindToolbarManager, null, layoutDriver, null, null, null,
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        CustomTabActivity.this.finish();
                    }
                });

        mainTab.setFullscreenManager(getFullscreenManager());
        mCustomTabContentHandler = new CustomTabContentHandler() {
            @Override
            public void loadUrlAndTrackFromTimestamp(LoadUrlParams params, long timestamp) {
                loadUrlInCurrentTab(params, timestamp);
            }

            @Override
            public IBinder getSession() {
                return mSession;
            }

            @Override
            public boolean shouldIgnoreIntent(Intent intent) {
                return mIntentHandler.shouldIgnoreIntent(CustomTabActivity.this, intent);
            }

            @Override
            public boolean updateCustomButton(int id, Bitmap bitmap, String description) {
                CustomButtonParams params = mIntentDataProvider.getButtonParamsForId(id);
                if (params == null) return false;

                params.update(bitmap, description);
                if (params.showOnToolbar()) {
                    if (!CustomButtonParams.doesIconFitToolbar(CustomTabActivity.this, bitmap)) {
                        return false;
                    }
                    showCustomButtonOnToolbar();
                } else {
                    updateBottomBarButton(params);
                }
                return true;
            }
        };
        DataUseTabUIManager.onCustomTabInitialNavigation(mainTab,
                CustomTabsConnection.getInstance(getApplication())
                        .getClientPackageNameForSession(mSession),
                IntentHandler.getUrlFromIntent(getIntent()));
        mainTab.setAppAssociatedWith(CustomTabsConnection.getInstance(getApplication())
                .getClientPackageNameForSession(mSession));
        recordClientPackageName();
        loadUrlInCurrentTab(new LoadUrlParams(IntentHandler.getUrlFromIntent(getIntent())),
                IntentHandler.getTimestampFromIntent(getIntent()));
        super.finishNativeInitialization();
    }

    private Tab createMainTab() {
        String url = IntentHandler.getUrlFromIntent(getIntent());
        // Get any referrer that has been explicitly set by the app.
        String referrerUrl = IntentHandler.getReferrerUrlIncludingExtraHeaders(getIntent(), this);
        if (referrerUrl == null) {
            Referrer referrer = CustomTabsConnection.getInstance(getApplication())
                    .getReferrerForSession(mSession);
            if (referrer != null) referrerUrl = referrer.getUrl();
        }
        Tab tab = new Tab(TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID),
                Tab.INVALID_TAB_ID, false, this, getWindowAndroid(),
                TabLaunchType.FROM_EXTERNAL_APP, null, null);
        CustomTabsConnection customTabsConnection =
                CustomTabsConnection.getInstance(getApplication());
        WebContents webContents =
                customTabsConnection.takePrerenderedUrl(mSession, url, referrerUrl);
        if (webContents == null) {
            webContents = customTabsConnection.takeSpareWebContents();
            // TODO(lizeb): Remove this once crbug.com/521729 is fixed.
            if (webContents != null) mShouldReplaceCurrentEntry = true;
        }
        if (webContents == null) {
            webContents = WebContentsFactory.createWebContents(false, false);
        }
        tab.initialize(webContents, getTabContentManager(),
                new CustomTabDelegateFactory(mIntentDataProvider.shouldEnableUrlBarHiding()), false,
                false);
        tab.getTabRedirectHandler().updateIntent(getIntent());
        tab.getView().requestFocus();
        return tab;
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        getTabModelSelector().onNativeLibraryReady(getTabContentManager());
    }

    private void recordClientPackageName() {
        String clientName = CustomTabsConnection.getInstance(getApplication())
                .getClientPackageNameForSession(mSession);
        if (TextUtils.isEmpty(clientName)) clientName = mIntentDataProvider.getClientPackageName();
        final String packageName = clientName;
        if (TextUtils.isEmpty(packageName) || packageName.contains(getPackageName())) return;
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RapporServiceBridge.sampleString(
                        "CustomTabs.ServiceClient.PackageName", packageName);
            }
        });
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        setActiveContentHandler(mCustomTabContentHandler);

        if (!mRecordedStartupUma) {
            mRecordedStartupUma = true;
            ExternalAppId externalId =
                    IntentHandler.determineExternalIntentSource(getPackageName(), getIntent());
            RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId",
                    externalId.ordinal(), ExternalAppId.INDEX_BOUNDARY.ordinal());
        }
    }

    @Override
    public void onPauseWithNative() {
        super.onPauseWithNative();
        CustomTabsConnection.getInstance(getApplication()).notifyNavigationEvent(
                mSession, CustomTabsCallback.TAB_HIDDEN);
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        setActiveContentHandler(null);
    }

    /**
     * Loads the current tab with the given load params while taking client
     * referrer and extra headers into account.
     */
    void loadUrlInCurrentTab(LoadUrlParams params, long timeStamp) {
        Intent intent = getIntent();
        IntentHandler.addReferrerAndHeaders(params, intent, this);
        if (params.getReferrer() == null) {
            params.setReferrer(CustomTabsConnection.getInstance(getApplication())
                    .getReferrerForSession(mSession));
        }
        // See ChromeTabCreator#getTransitionType(). This marks the navigation chain as starting
        // from an external intent.
        params.setTransitionType(PageTransition.LINK | PageTransition.FROM_API);
        mTabObserver.trackNextPageLoadFromTimestamp(timeStamp);
        if (mShouldReplaceCurrentEntry) params.setShouldReplaceCurrentEntry(true);
        mShouldReplaceCurrentEntry = false;
        getActivityTab().loadUrl(params);
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        if (getActivityTab() == null) return;
        getActivityTab().loadUrl(new LoadUrlParams(searchUrl));
    }

    @Override
    public TabModelSelectorImpl getTabModelSelector() {
        return (TabModelSelectorImpl) super.getTabModelSelector();
    }

    @Override
    protected AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new CustomTabAppMenuPropertiesDelegate(this, mIntentDataProvider.getMenuTitles(),
                mIntentDataProvider.shouldShowShareMenuItem());
    }

    @Override
    protected int getAppMenuLayoutId() {
        return R.menu.custom_tabs_menu;
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    public String getPackageName() {
        if (mShouldOverridePackage) return mIntentDataProvider.getClientPackageName();
        return super.getPackageName();
    }

    @Override
    public void finish() {
        super.finish();
        if (mIntentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            overridePendingTransition(mIntentDataProvider.getAnimationEnterRes(),
                    mIntentDataProvider.getAnimationExitRes());
            mShouldOverridePackage = false;
        }
    }

    @Override
    protected boolean handleBackPressed() {
        if (getActivityTab() == null) return false;

        if (exitFullscreenIfShowing()) return true;

        if (!getToolbarManager().back()) {
            if (getCurrentTabModel().getCount() > 1) {
                getCurrentTabModel().closeTab(getActivityTab(), false, false, false);
            } else {
                finish();
            }
        }
        return true;
    }

    /**
     * Configures the custom button on toolbar. Does nothing if invalid data is provided by clients.
     */
    private void showCustomButtonOnToolbar() {
        final CustomButtonParams params = mIntentDataProvider.getCustomButtonOnToolbar();
        if (params == null) return;
        getToolbarManager().setCustomActionButton(
                params.getIcon(getResources()),
                params.getDescription(),
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        String creatorPackage =
                                ApiCompatibilityUtils.getCreatorPackage(params.getPendingIntent());
                        if (mIntentDataProvider.finishAfterOpeningInBrowser()
                                && TextUtils.equals(getPackageName(), creatorPackage)) {
                            openCurrentUrlInBrowser();
                            finish();
                        } else {
                            mIntentDataProvider.sendButtonPendingIntentWithUrl(
                                    getApplicationContext(), getActivityTab().getUrl());
                            RecordUserAction.record("CustomTabsCustomActionButtonClick");
                        }
                    }
                });
    }

    /**
     * Updates the button on the bottom bar that corresponds to the given {@link CustomButtonParams}
     */
    private void updateBottomBarButton(CustomButtonParams params) {
        ViewGroup bottomBar = (ViewGroup) findViewById(R.id.bottombar);
        ImageButton button = (ImageButton) bottomBar.findViewById(params.getId());
        button.setContentDescription(params.getDescription());
        button.setImageDrawable(params.getIcon(getResources()));
    }

    /**
     * Inflates the bottom bar {@link ViewStub} and its shadow, and populates it with items.
     */
    private void showBottomBarIfNecessary() {
        // TODO (yusufo): Find a better place for the layout code here and in CustomButtonParams.
        if (!mIntentDataProvider.shouldShowBottomBar()) return;

        ViewStub bottomBarStub = ((ViewStub) findViewById(R.id.bottombar_stub));
        bottomBarStub.setLayoutResource(R.layout.custom_tabs_bottombar);
        bottomBarStub.inflate();

        View shadow = findViewById(R.id.bottombar_shadow);
        shadow.setVisibility(View.VISIBLE);

        ViewGroup bottomBar = (ViewGroup) findViewById(R.id.bottombar);
        bottomBar.setBackgroundColor(mIntentDataProvider.getBottomBarColor());
        List<CustomButtonParams> items = mIntentDataProvider.getCustomButtonsOnBottombar();
        for (CustomButtonParams params : items) {
            if (params.showOnToolbar()) continue;
            final PendingIntent pendingIntent = params.getPendingIntent();
            OnClickListener clickListener = null;
            if (pendingIntent != null) {
                clickListener = new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        Intent addedIntent = new Intent();
                        addedIntent.setData(Uri.parse(getActivityTab().getUrl()));
                        try {
                            pendingIntent.send(CustomTabActivity.this, 0, addedIntent, null, null);
                        } catch (CanceledException e) {
                            Log.e(TAG,
                                    "CanceledException while sending pending intent in custom tab");
                        }
                    }
                };
            }
            ImageButton button = params.buildBottomBarButton(this, bottomBar, clickListener);
            bottomBar.addView(button);
        }
    }

    @Override
    public boolean shouldShowAppMenu() {
        return getActivityTab() != null && getToolbarManager().isInitialized();
    }

    @Override
    protected void showAppMenuForKeyboardEvent() {
        if (!shouldShowAppMenu()) return;
        super.showAppMenuForKeyboardEvent();
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int menuIndex = getAppMenuPropertiesDelegate().getIndexOfMenuItem(item);
        if (menuIndex >= 0) {
            mIntentDataProvider.clickMenuItemWithUrl(this, menuIndex,
                    getTabModelSelector().getCurrentTab().getUrl());
            RecordUserAction.record("CustomTabsMenuCustomMenuItem");
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(event, this,
                getToolbarManager().isInitialized());
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!getToolbarManager().isInitialized()) {
            return super.onKeyDown(keyCode, event);
        }
        return KeyboardShortcuts.onKeyDown(event, this, true, false)
                || super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating new tabs, bookmark, history, print, help, focus_url, etc.
        if (id == R.id.focus_url_bar || id == R.id.all_bookmarks_menu_id
                || id == R.id.bookmark_this_page_id || id == R.id.print_id || id == R.id.help_id
                || id == R.id.recent_tabs_menu_id || id == R.id.new_incognito_tab_menu_id
                || id == R.id.new_tab_menu_id || id == R.id.open_history_menu_id) {
            return true;
        } else if (id == R.id.open_in_browser_id) {
            openCurrentUrlInBrowser();
            RecordUserAction.record("CustomTabsMenuOpenInChrome");
            return true;
        } else if (id == R.id.find_in_page_id) {
            mFindToolbarManager.showToolbar();
            if (getContextualSearchManager() != null) {
                getContextualSearchManager().hideContextualSearch(StateChangeReason.UNKNOWN);
            }
            if (fromMenu) {
                RecordUserAction.record("MobileMenuFindInPage");
            } else {
                RecordUserAction.record("MobileShortcutFindInPage");
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    protected void setStatusBarColor(Tab tab, int color) {
        // Intentionally do nothing as CustomTabActivity explicitly sets status bar color.
    }

    /**
     * @return The {@link AppMenuPropertiesDelegate} associated with this activity. For test
     *         purposes only.
     */
    @VisibleForTesting
    @Override
    public CustomTabAppMenuPropertiesDelegate getAppMenuPropertiesDelegate() {
        return (CustomTabAppMenuPropertiesDelegate) super.getAppMenuPropertiesDelegate();
    }

    @Override
    public void onCheckForUpdate(boolean updateAvailable) {
    }

    /**
     * @return The {@link CustomTabIntentDataProvider} for this {@link CustomTabActivity}. For test
     *         purposes only.
     */
    @VisibleForTesting
    CustomTabIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }

    /**
     * Opens the URL currently being displayed in the Custom Tab in the regular browser.
     */
    void openCurrentUrlInBrowser() {
        String url = getTabModelSelector().getCurrentTab().getUrl();
        if (DomDistillerUrlUtils.isDistilledPage(url)) {
            url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
        }
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(ChromeLauncherActivity.EXTRA_IS_ALLOWED_TO_RETURN_TO_PARENT, false);

        // Temporarily allowing disk access while fixing. TODO: http://crbug.com/581860
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        StrictMode.allowThreadDiskWrites();
        try {
            startActivity(intent);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }
}
