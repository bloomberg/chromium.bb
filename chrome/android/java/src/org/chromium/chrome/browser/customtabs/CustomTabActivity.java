// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.StrictMode;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsIntent;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.RemoteViews;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocument;
import org.chromium.chrome.browser.datausage.DataUseTabUIManager;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.pageinfo.WebsiteSettingsPopup;
import org.chromium.chrome.browser.rappor.RapporServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends ChromeActivity {
    public static final int RESULT_BACK_PRESSED = 1;
    public static final int RESULT_STOPPED = 2;
    public static final int RESULT_CLOSED = 3;

    private static final String TAG = "CustomTabActivity";

    private static CustomTabContentHandler sActiveContentHandler;

    private FindToolbarManager mFindToolbarManager;
    private CustomTabIntentDataProvider mIntentDataProvider;
    private IBinder mSession;
    private CustomTabContentHandler mCustomTabContentHandler;
    private Tab mMainTab;
    private CustomTabBottomBarDelegate mBottomBarDelegate;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    private boolean mShouldOverridePackage;

    private boolean mRecordedStartupUma;
    private boolean mShouldReplaceCurrentEntry;
    private boolean mHasCreatedTabEarly;
    private CustomTabObserver mTabObserver;

    private String mPrerenderedUrl;
    private boolean mHasPrerendered;

    // Only the normal tab model is observed because there is no icognito tabmodel in Custom Tabs.
    private TabModelObserver mTabModelObserver = new EmptyTabModelObserver() {
        @Override
        public void didAddTab(Tab tab, TabLaunchType type) {
            tab.addObserver(mTabObserver);
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

    /**
     * Checks whether the active {@link CustomTabContentHandler} belongs to the given session, and
     * if true, updates {@link RemoteViews} on the secondary toolbar.
     * @return Whether the update is successful.
     */
    static boolean updateRemoteViews(IBinder session, RemoteViews remoteViews, int[] clickableIDs,
            PendingIntent pendingIntent) {
        ThreadUtils.assertOnUiThread();
        // Do nothing if there is no activity or the activity does not belong to this session.
        if (sActiveContentHandler == null || !sActiveContentHandler.getSession().equals(session)) {
            return false;
        }
        return sActiveContentHandler.updateRemoteViews(remoteViews, clickableIDs, pendingIntent);
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
        // This happens before super.onStop() to maximize chances of getting the Tab while it's
        // alive.
        // TODO(dfalcantara): Once this is addressed on M50, consider transferring the Tab directly
        //                    via Tab reparenting.
        if (mIntentDataProvider.isOpenedByChrome() && isHerbResultNeeded()) {
            createHerbResultIntent(RESULT_STOPPED);
            finish();
        }

        super.onStop();
        CustomTabsConnection.getInstance(getApplication())
                .dontKeepAliveForSession(mIntentDataProvider.getSession());
    }

    @Override
    public void preInflationStartup() {
        super.preInflationStartup();
        mIntentDataProvider = new CustomTabIntentDataProvider(getIntent(), this);
        mSession = mIntentDataProvider.getSession();
        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);
        if (CustomTabsConnection.hasWarmUpBeenFinished(getApplication())) {
            mMainTab = createMainTab();
            loadUrlInTab(mMainTab, new LoadUrlParams(getUrlToLoad()),
                    IntentHandler.getTimestampFromIntent(getIntent()));
            mHasCreatedTabEarly = true;
        }
    }

    @Override
    public void postInflationStartup() {
        super.postInflationStartup();
        setTabModelSelector(new TabModelSelectorImpl(this,
                TabModelSelectorImpl.CUSTOM_TABS_SELECTOR_INDEX, getWindowAndroid(), false));
        getToolbarManager().setCloseButtonDrawable(mIntentDataProvider.getCloseButtonDrawable());
        getToolbarManager().setShowTitle(mIntentDataProvider.getTitleVisibilityState()
                == CustomTabsIntent.SHOW_PAGE_TITLE);
        if (CustomTabsConnection.getInstance(getApplication())
                .shouldHideDomainForSession(mSession)) {
            getToolbarManager().setUrlBarHidden(true);
        }
        int toolbarColor = mIntentDataProvider.getToolbarColor();
        getToolbarManager().updatePrimaryColor(toolbarColor);
        if (!mIntentDataProvider.isOpenedByChrome()) {
            getToolbarManager().setShouldUpdateToolbarPrimaryColor(false);
        }
        if (toolbarColor != ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_primary_color)) {
            ApiCompatibilityUtils.setStatusBarColor(getWindow(),
                    ColorUtils.getDarkenedColorForStatusBar(toolbarColor));
        }

        // Setting task title and icon to be null will preserve the client app's title and icon.
        ApiCompatibilityUtils.setTaskDescription(this, null, null, toolbarColor);
        showCustomButtonOnToolbar();
        mBottomBarDelegate = new CustomTabBottomBarDelegate(this, mIntentDataProvider);
        mBottomBarDelegate.showBottomBarIfNecessary();
    }

    @Override
    public void finishNativeInitialization() {
        final CustomTabsConnection connection = CustomTabsConnection.getInstance(getApplication());
        // If extra headers have been passed, cancel any current prerender, as
        // prerendering doesn't support extra headers.
        if (IntentHandler.getExtraHeadersFromIntent(getIntent()) != null) {
            connection.cancelPrerender(mSession);
        }
        if (mHasCreatedTabEarly) {
            // When the tab is created early, we don't have the TabContentManager connected, since
            // compositor related controllers were not initialized at that point.
            mMainTab.attachTabContentManager(getTabContentManager());
        } else {
            mMainTab = createMainTab();
        }
        getTabModelSelector().getModel(false).addObserver(mTabModelObserver);
        getTabModelSelector().getModel(false).addTab(mMainTab, 0, mMainTab.getLaunchType());

        ToolbarControlContainer controlContainer = (ToolbarControlContainer) findViewById(
                R.id.control_container);
        LayoutManagerDocument layoutDriver = new CustomTabLayoutManager(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content), controlContainer);
        mFindToolbarManager = new FindToolbarManager(this,
                getToolbarManager().getActionModeController().getActionModeCallback());
        if (getContextualSearchManager() != null) {
            getContextualSearchManager().setFindToolbarManager(mFindToolbarManager);
        }
        getToolbarManager().initializeWithNative(getTabModelSelector(), getFullscreenManager(),
                mFindToolbarManager, null, layoutDriver, null, null, null,
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        if (mIntentDataProvider.isOpenedByChrome() && isHerbResultNeeded()) {
                            createHerbResultIntent(RESULT_CLOSED);
                        }
                        RecordUserAction.record("CustomTabs.CloseButtonClicked");
                        finishAndClose();
                    }
                });

        mMainTab.setFullscreenManager(getFullscreenManager());
        mCustomTabContentHandler = new CustomTabContentHandler() {
            @Override
            public void loadUrlAndTrackFromTimestamp(LoadUrlParams params, long timestamp) {
                if (!TextUtils.isEmpty(params.getUrl())) {
                    params.setUrl(DataReductionProxySettings.getInstance()
                            .maybeRewriteWebliteUrl(params.getUrl()));
                }
                loadUrlInTab(getActivityTab(), params, timestamp);
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
                    if (mBottomBarDelegate != null) {
                        mBottomBarDelegate.updateBottomBarButtons(params);
                    }
                }
                return true;
            }

            @Override
            public boolean updateRemoteViews(RemoteViews remoteViews, int[] clickableIDs,
                    PendingIntent pendingIntent) {
                if (mBottomBarDelegate == null) return false;
                return mBottomBarDelegate.updateRemoteViews(remoteViews, clickableIDs,
                        pendingIntent);
            }
        };
        String url = getUrlToLoad();
        DataUseTabUIManager.onCustomTabInitialNavigation(
                mMainTab, connection.getClientPackageNameForSession(mSession), url);
        recordClientPackageName();
        connection.showSignInToastIfNecessary(mSession, getIntent());
        if (mHasCreatedTabEarly) {
            if (!mMainTab.isLoading()) postDeferredStartupIfNeeded();
        } else {
            loadUrlInTab(mMainTab, new LoadUrlParams(url),
                    IntentHandler.getTimestampFromIntent(getIntent()));
        }
        super.finishNativeInitialization();
    }

    private Tab createMainTab() {
        CustomTabsConnection customTabsConnection =
                CustomTabsConnection.getInstance(getApplication());
        String url = getUrlToLoad();
        // Get any referrer that has been explicitly set by the app.
        String referrerUrl = IntentHandler.getReferrerUrlIncludingExtraHeaders(getIntent(), this);
        if (referrerUrl == null) {
            Referrer referrer = customTabsConnection.getReferrerForSession(mSession);
            if (referrer != null) referrerUrl = referrer.getUrl();
        }
        Tab tab = new Tab(TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID),
                Tab.INVALID_TAB_ID, false, this, getWindowAndroid(),
                TabLaunchType.FROM_EXTERNAL_APP, null, null);
        tab.setAppAssociatedWith(customTabsConnection.getClientPackageNameForSession(mSession));

        mPrerenderedUrl = customTabsConnection.getPrerenderedUrl(mSession);
        WebContents webContents =
                customTabsConnection.takePrerenderedUrl(mSession, url, referrerUrl);
        mHasPrerendered = webContents != null;
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
        mTabObserver = new CustomTabObserver(getApplication(), mSession);
        tab.addObserver(mTabObserver);
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
    private void loadUrlInTab(final Tab tab, final LoadUrlParams params, long timeStamp) {
        Intent intent = getIntent();
        String url = getUrlToLoad();
        if (mHasPrerendered && UrlUtilities.urlsFragmentsDiffer(mPrerenderedUrl, url)) {
            mHasPrerendered = false;
            LoadUrlParams temporaryParams = new LoadUrlParams(mPrerenderedUrl);
            IntentHandler.addReferrerAndHeaders(temporaryParams, intent, this);
            tab.loadUrl(temporaryParams);
            params.setShouldReplaceCurrentEntry(true);
        }

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
        if (mShouldReplaceCurrentEntry
                && tab.getWebContents().getNavigationController().getEntryAtIndex(0)
                == null) {
            // If the spare web contents has gotten a loadurl but has not committed yet, wait
            // until commit to start the actual load.
            tab.getWebContents().addObserver(new WebContentsObserver() {
                @Override
                public void didCommitProvisionalLoadForFrame(long frameId, boolean isMainFrame,
                        String url, int transitionType) {
                    if (!isMainFrame) return;
                    tab.loadUrl(params);
                    tab.getWebContents().removeObserver(this);
                }
            });
        } else {
            tab.loadUrl(params);
        }
        mShouldReplaceCurrentEntry = false;
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
    public Tab getActivityTab() {
        Tab tab = super.getActivityTab();
        if (tab == null) tab = mMainTab;
        return tab;
    }

    @Override
    protected AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new CustomTabAppMenuPropertiesDelegate(this, mIntentDataProvider.getMenuTitles(),
                mIntentDataProvider.shouldShowShareMenuItem(),
                mIntentDataProvider.shouldShowBookmarkMenuItem(),
                mIntentDataProvider.isOpenedByChrome());
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
        // Prevent the menu window from leaking.
        if (getAppMenuHandler() != null) getAppMenuHandler().hideAppMenu();

        super.finish();
        if (mIntentDataProvider != null && mIntentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            overridePendingTransition(mIntentDataProvider.getAnimationEnterRes(),
                    mIntentDataProvider.getAnimationExitRes());
            mShouldOverridePackage = false;
        }
    }

    /**
     * Finishes the activity and removes the reference from the Android recents.
     */
    public void finishAndClose() {
        // When on top of another app, finish is all that is required.
        finish();
    }

    @Override
    protected boolean handleBackPressed() {
        RecordUserAction.record("CustomTabs.SystemBack");

        if (getActivityTab() == null) return false;

        if (exitFullscreenIfShowing()) return true;

        if (!getToolbarManager().back()) {
            if (getCurrentTabModel().getCount() > 1) {
                getCurrentTabModel().closeTab(getActivityTab(), false, false, false);
            } else {
                if (mIntentDataProvider.isOpenedByChrome() && isHerbResultNeeded()) {
                    createHerbResultIntent(RESULT_BACK_PRESSED);
                }
                finishAndClose();
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
                        if (mIntentDataProvider.isOpenedByChrome()
                                && TextUtils.equals(getPackageName(), creatorPackage)) {
                            RecordUserAction.record(
                                    "TaskManagement.OpenInChromeActionButtonClicked");
                            if (openCurrentUrlInBrowser(false)) finishAndClose();
                        } else {
                            mIntentDataProvider.sendButtonPendingIntentWithUrl(
                                    getApplicationContext(), getActivityTab().getUrl());
                            RecordUserAction.record("CustomTabsCustomActionButtonClick");
                        }
                    }
                });
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
                || id == R.id.print_id || id == R.id.help_id
                || id == R.id.recent_tabs_menu_id || id == R.id.new_incognito_tab_menu_id
                || id == R.id.new_tab_menu_id || id == R.id.open_history_menu_id) {
            return true;
        } else if (id == R.id.bookmark_this_page_id
                && !mIntentDataProvider.shouldShowBookmarkMenuItem()) {
            return true;
        } else if (id == R.id.open_in_browser_id) {
            openCurrentUrlInBrowser(false);
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
        } else if (id == R.id.info_menu_id) {
            WebsiteSettingsPopup.show(this, getTabModelSelector().getCurrentTab(),
                    getToolbarManager().getContentPublisher());
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    protected void setStatusBarColor(Tab tab, int color) {
        // Intentionally do nothing as CustomTabActivity explicitly sets status bar color.  Except
        // for Custom Tabs opened by Chrome.
        if (mIntentDataProvider.isOpenedByChrome()) super.setStatusBarColor(tab, color);
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
     * @param forceReparenting Whether tab reparenting should be forced for testing.
     *
     * @return Whether or not the tab was sent over successfully.
     */
    boolean openCurrentUrlInBrowser(boolean forceReparenting) {
        Tab tab = getActivityTab();
        if (tab == null) return false;

        String url = tab.getUrl();
        if (DomDistillerUrlUtils.isDistilledPage(url)) {
            url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
        }
        if (TextUtils.isEmpty(url)) url = getUrlToLoad();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(ChromeLauncherActivity.EXTRA_IS_ALLOWED_TO_RETURN_TO_PARENT, false);

        boolean willChromeHandleIntent = getIntentDataProvider().isOpenedByChrome();
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        StrictMode.allowThreadDiskWrites();
        try {
            willChromeHandleIntent |= ExternalNavigationDelegateImpl
                    .willChromeHandleIntent(this, intent, true);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        Bundle startActivityOptions = ActivityOptionsCompat.makeCustomAnimation(
                this, R.anim.abc_fade_in, R.anim.abc_fade_out).toBundle();
        if (willChromeHandleIntent || forceReparenting) {
            Runnable finalizeCallback = new Runnable() {
                @Override
                public void run() {
                    finishAndClose();
                }
            };

            mMainTab = null;
            tab.detachAndStartReparenting(intent, startActivityOptions, finalizeCallback);
        } else {
            // Temporarily allowing disk access while fixing. TODO: http://crbug.com/581860
            StrictMode.allowThreadDiskReads();
            StrictMode.allowThreadDiskWrites();
            try {
                startActivity(intent, startActivityOptions);
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
        return true;
    }

    /**
     * @return Whether {@link ChromeTabbedActivity} is waiting for a result from this Activity.
     */
    private boolean isHerbResultNeeded() {
        if (!TextUtils.equals(FeatureUtilities.getHerbFlavor(), ChromeSwitches.HERB_FLAVOR_DILL)) {
            return false;
        }

        String callingActivity =
                getCallingActivity() == null ? null : getCallingActivity().getClassName();
        return TextUtils.equals(callingActivity, ChromeTabbedActivity.class.getName());
    }

    /**
     * Lets the original Activity know how this {@link CustomTabActivity} was finished.
     */
    private void createHerbResultIntent(int result) {
        if (getActivityTab() == null) return;
        Intent resultIntent = new Intent();

        switch (result) {
            case RESULT_STOPPED:
                // Send the URL to the browser.  Should pass the Tab in the future.
                resultIntent.setAction(Intent.ACTION_VIEW);
                resultIntent.setData(Uri.parse(getActivityTab().getUrl()));
                break;

            case RESULT_BACK_PRESSED:
            case RESULT_CLOSED:
                break;

            default:
                assert false;
        }

        setResult(result, resultIntent);
    }

    /**
     * @return The URL that should be used from this intent. If it is a WebLite url, it may be
     *         overridden if the Data Reduction Proxy is using Lo-Fi previews.
     */
    private String getUrlToLoad() {
        String url = IntentHandler.getUrlFromIntent(getIntent());
        if (!TextUtils.isEmpty(url)) {
            url = DataReductionProxySettings.getInstance().maybeRewriteWebliteUrl(url);
        }
        return url;
    }
}
