// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.CUSTOM_TABS_UI_TYPE_DEFAULT;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.CUSTOM_TABS_UI_TYPE_INFO_PAGE;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.StrictMode;
import android.provider.Browser;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.RemoteViews;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
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
import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocument;
import org.chromium.chrome.browser.datausage.DataUseTabUIManager;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.page_info.PageInfoPopup;
import org.chromium.chrome.browser.rappor.RapporServiceBridge;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.TimeUnit;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends ChromeActivity {

    private static final String TAG = "CustomTabActivity";
    private static final String LAST_URL_PREF = "pref_last_custom_tab_url";

    // For CustomTabs.WebContentsStateOnLaunch, see histograms.xml. Append only.
    private static final int WEBCONTENTS_STATE_NO_WEBCONTENTS = 0;
    private static final int WEBCONTENTS_STATE_PRERENDERED_WEBCONTENTS = 1;
    private static final int WEBCONTENTS_STATE_SPARE_WEBCONTENTS = 2;
    private static final int WEBCONTENTS_STATE_TRANSFERRED_WEBCONTENTS = 3;
    private static final int WEBCONTENTS_STATE_MAX = 4;

    private static CustomTabContentHandler sActiveContentHandler;

    private FindToolbarManager mFindToolbarManager;
    private CustomTabIntentDataProvider mIntentDataProvider;
    private CustomTabsSessionToken mSession;
    private CustomTabContentHandler mCustomTabContentHandler;
    private Tab mMainTab;
    private CustomTabBottomBarDelegate mBottomBarDelegate;
    private CustomTabTabPersistencePolicy mTabPersistencePolicy;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    private boolean mShouldOverridePackage;

    private boolean mHasCreatedTabEarly;
    private boolean mIsInitialResume = true;
    // Whether there is any speculative page loading associated with the session.
    private boolean mHasSpeculated;
    private CustomTabObserver mTabObserver;

    private String mSpeculatedUrl;

    private boolean mUsingPrerender;
    private boolean mUsingHiddenTab;

    private boolean mIsClosing;

    // This boolean is used to do a hack in navigation history for
    // prerender and hidden tab loads with unmatching fragments.
    private boolean mIsFirstLoad;

    private final CustomTabsConnection mConnection = CustomTabsConnection.getInstance();

    private static class PageLoadMetricsObserver implements PageLoadMetrics.Observer {
        private final CustomTabsConnection mConnection;
        private final CustomTabsSessionToken mSession;
        private final WebContents mWebContents;

        public PageLoadMetricsObserver(CustomTabsConnection connection,
                CustomTabsSessionToken session, Tab tab) {
            mConnection = connection;
            mSession = session;
            mWebContents = tab.getWebContents();
        }

        @Override
        public void onFirstContentfulPaint(
                WebContents webContents, long navigationStartTick, long firstContentfulPaintMs) {
            if (webContents != mWebContents) return;

            mConnection.notifyPageLoadMetric(mSession, PageLoadMetrics.FIRST_CONTENTFUL_PAINT,
                    navigationStartTick, firstContentfulPaintMs);
        }

        @Override
        public void onLoadEventStart(
                WebContents webContents, long navigationStartTick, long loadEventStartMs) {
            if (webContents != mWebContents) return;

            mConnection.notifyPageLoadMetric(mSession, PageLoadMetrics.LOAD_EVENT_START,
                    navigationStartTick, loadEventStartMs);
        }
    }

    private static class CustomTabCreator extends ChromeTabCreator {
        private final boolean mSupportsUrlBarHiding;
        private final boolean mIsOpenedByChrome;
        private final BrowserStateBrowserControlsVisibilityDelegate mVisibilityDelegate;

        public CustomTabCreator(
                ChromeActivity activity, WindowAndroid nativeWindow, boolean incognito,
                boolean supportsUrlBarHiding, boolean isOpenedByChrome) {
            super(activity, nativeWindow, incognito);
            mSupportsUrlBarHiding = supportsUrlBarHiding;
            mIsOpenedByChrome = isOpenedByChrome;
            mVisibilityDelegate = activity.getFullscreenManager().getBrowserVisibilityDelegate();
        }

        @Override
        public TabDelegateFactory createDefaultTabDelegateFactory() {
            return new CustomTabDelegateFactory(
                    mSupportsUrlBarHiding, mIsOpenedByChrome, mVisibilityDelegate);
        }
    }

    private PageLoadMetricsObserver mMetricsObserver;

    // Only the normal tab model is observed because there is no incognito tabmodel in Custom Tabs.
    private TabModelObserver mTabModelObserver = new EmptyTabModelObserver() {
        @Override
        public void didAddTab(Tab tab, TabLaunchType type) {
            PageLoadMetrics.addObserver(mMetricsObserver);
            tab.addObserver(mTabObserver);
        }

        @Override
        public void didCloseTab(int tabId, boolean incognito) {
            PageLoadMetrics.removeObserver(mMetricsObserver);
            // Finish the activity after we intent out.
            if (getTabModelSelector().getCurrentModel().getCount() == 0) finishAndClose(false);
        }

        @Override
        public void tabRemoved(Tab tab) {
            tab.removeObserver(mTabObserver);
            PageLoadMetrics.removeObserver(mMetricsObserver);
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
     * Called when a CustomTabs intent is handled.
     *
     * Used to check whether an incoming intent can be handled by the current
     * {@link CustomTabContentHandler}, and to perform action on new Intent.
     *
     * @return Whether the active {@link CustomTabContentHandler} has handled the intent.
     */
    public static boolean handleInActiveContentIfNeeded(Intent intent) {
        CustomTabsSessionToken session = CustomTabsSessionToken.getSessionTokenFromIntent(intent);

        String url = IntentHandler.getUrlFromIntent(intent);
        if (TextUtils.isEmpty(url)) return false;
        CustomTabsConnection.getInstance().onHandledIntent(session, url, intent);

        if (sActiveContentHandler == null) return false;
        if (session == null || !session.equals(sActiveContentHandler.getSession())) return false;
        if (sActiveContentHandler.shouldIgnoreIntent(intent)) {
            Log.w(TAG, "Incoming intent to Custom Tab was ignored.");
            return false;
        }
        sActiveContentHandler.loadUrlAndTrackFromTimestamp(new LoadUrlParams(url),
                IntentHandler.getTimestampFromIntent(intent));
        return true;
    }

    /**
     * @return Whether the given session is the currently active session.
     */
    public static boolean isActiveSession(CustomTabsSessionToken session) {
        if (sActiveContentHandler == null) return false;
        if (session == null || sActiveContentHandler.getSession() == null) return false;
        return sActiveContentHandler.getSession().equals(session);
    }

    /**
     * Checks whether the active {@link CustomTabContentHandler} belongs to the given session, and
     * if true, update toolbar's custom button.
     * @param session     The {@link IBinder} that the calling client represents.
     * @param bitmap      The new icon for action button.
     * @param description The new content description for the action button.
     * @return Whether the update is successful.
     */
    static boolean updateCustomButton(
            CustomTabsSessionToken session, int id, Bitmap bitmap, String description) {
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
    static boolean updateRemoteViews(
            CustomTabsSessionToken session, RemoteViews remoteViews, int[] clickableIDs,
            PendingIntent pendingIntent) {
        ThreadUtils.assertOnUiThread();
        // Do nothing if there is no activity or the activity does not belong to this session.
        if (sActiveContentHandler == null || !sActiveContentHandler.getSession().equals(session)) {
            return false;
        }
        return sActiveContentHandler.updateRemoteViews(remoteViews, clickableIDs, pendingIntent);
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        int initialBackgroundColor = mIntentDataProvider.getInitialBackgroundColor();
        if (mIntentDataProvider.isTrustedIntent() && initialBackgroundColor != Color.TRANSPARENT) {
            return new ColorDrawable(initialBackgroundColor);
        } else {
            return super.getBackgroundDrawable();
        }
    }

    @Override
    public boolean isCustomTab() {
        return true;
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime.CustomTabs", timeMs, TimeUnit.MILLISECONDS);
    }

    @Override
    public void onStart() {
        super.onStart();
        mIsClosing = false;
        mConnection.keepAliveForSession(
                mIntentDataProvider.getSession(), mIntentDataProvider.getKeepAliveServiceIntent());
    }

    @Override
    public void onStop() {
        super.onStop();
        mConnection.dontKeepAliveForSession(mIntentDataProvider.getSession());
    }

    @Override
    public void preInflationStartup() {
        // Parse the data from the Intent before calling super to allow the Intent to customize
        // the Activity parameters, including the background of the page.
        mIntentDataProvider = new CustomTabIntentDataProvider(getIntent(), this);

        super.preInflationStartup();
        mSession = mIntentDataProvider.getSession();
        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);
        mSpeculatedUrl = mConnection.getSpeculatedUrl(mSession);
        mHasSpeculated = !TextUtils.isEmpty(mSpeculatedUrl);
        if (getSavedInstanceState() == null && CustomTabsConnection.hasWarmUpBeenFinished()) {
            initializeTabModels();
            mMainTab = getHiddenTab();
            if (mMainTab == null) mMainTab = createMainTab();
            mIsFirstLoad = true;
            loadUrlInTab(mMainTab, new LoadUrlParams(getUrlToLoad()),
                    IntentHandler.getTimestampFromIntent(getIntent()));
            mHasCreatedTabEarly = true;
        }
    }

    @Override
    public boolean shouldAllocateChildConnection() {
        return !mHasCreatedTabEarly && !mHasSpeculated
                && !WarmupManager.getInstance().hasSpareWebContents();
    }

    @Override
    public void postInflationStartup() {
        super.postInflationStartup();

        getToolbarManager().setCloseButtonDrawable(mIntentDataProvider.getCloseButtonDrawable());
        getToolbarManager().setShowTitle(mIntentDataProvider.getTitleVisibilityState()
                == CustomTabsIntent.SHOW_PAGE_TITLE);
        if (mConnection.shouldHideDomainForSession(mSession)) {
            getToolbarManager().setUrlBarHidden(true);
        }
        int toolbarColor = mIntentDataProvider.getToolbarColor();
        getToolbarManager().updatePrimaryColor(toolbarColor, false);
        if (!mIntentDataProvider.isOpenedByChrome()) {
            getToolbarManager().setShouldUpdateToolbarPrimaryColor(false);
        }
        if (toolbarColor != ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_primary_color)) {
            ApiCompatibilityUtils.setStatusBarColor(getWindow(),
                    ColorUtils.getDarkenedColorForStatusBar(toolbarColor));
        }
        // Properly attach tab's infobar to the view hierarchy, as the main tab might have been
        // initialized prior to inflation.
        if (mMainTab != null) {
            ViewGroup bottomContainer = (ViewGroup) findViewById(R.id.bottom_container);
            mMainTab.getInfoBarContainer().setParentView(bottomContainer);
        }

        // Setting task title and icon to be null will preserve the client app's title and icon.
        ApiCompatibilityUtils.setTaskDescription(this, null, null, toolbarColor);
        showCustomButtonOnToolbar();
        mBottomBarDelegate = new CustomTabBottomBarDelegate(this, mIntentDataProvider,
                getFullscreenManager());
        mBottomBarDelegate.showBottomBarIfNecessary();
    }

    @Override
    protected TabModelSelector createTabModelSelector() {
        mTabPersistencePolicy = new CustomTabTabPersistencePolicy(
                getTaskId(), getSavedInstanceState() != null);

        return new TabModelSelectorImpl(this, this, mTabPersistencePolicy, false, false);
    }

    @Override
    protected Pair<CustomTabCreator, CustomTabCreator> createTabCreators() {
        return Pair.create(
                new CustomTabCreator(
                        this, getWindowAndroid(), false,
                        mIntentDataProvider.shouldEnableUrlBarHiding(),
                        mIntentDataProvider.isOpenedByChrome()),
                new CustomTabCreator(
                        this, getWindowAndroid(), true,
                        mIntentDataProvider.shouldEnableUrlBarHiding(),
                        mIntentDataProvider.isOpenedByChrome()));
    }

    @Override
    public void finishNativeInitialization() {
        if (!mIntentDataProvider.isInfoPage()) FirstRunSignInProcessor.start(this);

        // If extra headers have been passed, cancel any current prerender, as
        // prerendering doesn't support extra headers.
        if (IntentHandler.getExtraHeadersFromIntent(getIntent()) != null) {
            mConnection.cancelSpeculation(mSession);
        }

        getTabModelSelector().getModel(false).addObserver(mTabModelObserver);

        boolean successfulStateRestore = false;
        // Attempt to restore the previous tab state if applicable.
        if (getSavedInstanceState() != null) {
            assert mMainTab == null;
            getTabModelSelector().loadState(true);
            getTabModelSelector().restoreTabs(true);
            mMainTab = getTabModelSelector().getCurrentTab();
            successfulStateRestore = mMainTab != null;
            if (successfulStateRestore) initializeMainTab(mMainTab);
        }

        // If no tab was restored, create a new tab.
        if (!successfulStateRestore) {
            if (mHasCreatedTabEarly) {
                // When the tab is created early, we don't have the TabContentManager connected,
                // since compositor related controllers were not initialized at that point.
                mMainTab.attachTabContentManager(getTabContentManager());
            } else {
                mMainTab = createMainTab();
            }
            getTabModelSelector().getModel(false).addTab(mMainTab, 0, mMainTab.getLaunchType());
        }

        // This cannot be done before because we want to do the reparenting only
        // when we have compositor related controllers.
        if (mUsingHiddenTab) {
            mMainTab.attachAndFinishReparenting(this,
                    new CustomTabDelegateFactory(mIntentDataProvider.shouldEnableUrlBarHiding(),
                            mIntentDataProvider.isOpenedByChrome(),
                            getFullscreenManager().getBrowserVisibilityDelegate()),
                    (TabReparentingParams) AsyncTabParamsManager.remove(mMainTab.getId()));
        }

        LayoutManagerDocument layoutDriver = new CustomTabLayoutManager(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content),
                (ToolbarControlContainer) findViewById(R.id.control_container));
        mFindToolbarManager = new FindToolbarManager(this,
                getToolbarManager().getActionModeController().getActionModeCallback());
        if (getContextualSearchManager() != null) {
            getContextualSearchManager().setFindToolbarManager(mFindToolbarManager);
        }
        getToolbarManager().initializeWithNative(
                getTabModelSelector(),
                getFullscreenManager().getBrowserVisibilityDelegate(),
                mFindToolbarManager, null, layoutDriver, null, null, null,
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        RecordUserAction.record("CustomTabs.CloseButtonClicked");
                        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()) {
                            RecordUserAction.record("CustomTabs.CloseButtonClicked.DownloadsUI");
                        }
                        finishAndClose(false);
                    }
                });

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
            public CustomTabsSessionToken getSession() {
                return mSession;
            }

            @Override
            public boolean shouldIgnoreIntent(Intent intent) {
                return mIntentHandler.shouldIgnoreIntent(intent);
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
        recordClientPackageName();
        mConnection.showSignInToastIfNecessary(mSession, getIntent());
        String url = getUrlToLoad();
        String packageName = mConnection.getClientPackageNameForSession(mSession);
        if (TextUtils.isEmpty(packageName)) {
            packageName = mConnection.extractCreatorPackage(getIntent());
        }
        DataUseTabUIManager.onCustomTabInitialNavigation(mMainTab, packageName, url);

        if (!mHasCreatedTabEarly && !successfulStateRestore && !mMainTab.isLoading()) {
            loadUrlInTab(mMainTab, new LoadUrlParams(url),
                    IntentHandler.getTimestampFromIntent(getIntent()));
        }

        // Put Sync in the correct state by calling tab state initialized. crbug.com/581811.
        getTabModelSelector().markTabStateInitialized();

        // Notify ServiceTabLauncher if this is an asynchronous tab launch.
        if (getIntent().hasExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA)) {
            ServiceTabLauncher.onWebContentsForRequestAvailable(
                    getIntent().getIntExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA, 0),
                    getActivityTab().getWebContents());
        }
        super.finishNativeInitialization();
    }

    /**
     * Encapsulates CustomTabsConnection#takeHiddenTab()
     * with additional initialization logic.
     */
    private Tab getHiddenTab() {
        String url = getUrlToLoad();
        String referrerUrl = mConnection.getReferrer(mSession, getIntent());
        Tab tab = mConnection.takeHiddenTab(mSession, url, referrerUrl);
        mUsingHiddenTab = tab != null;
        if (!mUsingHiddenTab) return null;
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.WebContentsStateOnLaunch",
                WEBCONTENTS_STATE_PRERENDERED_WEBCONTENTS, WEBCONTENTS_STATE_MAX);
        tab.setAppAssociatedWith(mConnection.getClientPackageNameForSession(mSession));
        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()) {
            tab.enableEmbeddedMediaExperience(true);
        }
        initializeMainTab(tab);
        return tab;
    }

    private Tab createMainTab() {
        WebContents webContents = takeWebContents();

        int assignedTabId = IntentUtils.safeGetIntExtra(
                getIntent(), IntentHandler.EXTRA_TAB_ID, Tab.INVALID_TAB_ID);
        int parentTabId = IntentUtils.safeGetIntExtra(
                getIntent(), IntentHandler.EXTRA_PARENT_TAB_ID, Tab.INVALID_TAB_ID);
        Tab tab = new Tab(assignedTabId, parentTabId, false, this, getWindowAndroid(),
                TabLaunchType.FROM_EXTERNAL_APP, null, null);
        tab.setAppAssociatedWith(mConnection.getClientPackageNameForSession(mSession));
        tab.initialize(
                webContents, getTabContentManager(),
                new CustomTabDelegateFactory(
                        mIntentDataProvider.shouldEnableUrlBarHiding(),
                        mIntentDataProvider.isOpenedByChrome(),
                        getFullscreenManager().getBrowserVisibilityDelegate()),
                false, false);

        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()) {
            tab.enableEmbeddedMediaExperience(true);
        }

        initializeMainTab(tab);
        return tab;
    }

    private WebContents takeWebContents() {
        mUsingPrerender = true;
        int webContentsStateOnLaunch = WEBCONTENTS_STATE_PRERENDERED_WEBCONTENTS;
        WebContents webContents = takePrerenderedWebContents();

        if (webContents == null) {
            mUsingPrerender = false;
            webContents = takeAsyncWebContents();
            if (webContents != null) {
                webContentsStateOnLaunch = WEBCONTENTS_STATE_TRANSFERRED_WEBCONTENTS;
                webContents.resumeLoadingCreatedWebContents();
            } else {
                webContents = WarmupManager.getInstance().takeSpareWebContents(false, false);
                if (webContents != null) {
                    webContentsStateOnLaunch = WEBCONTENTS_STATE_SPARE_WEBCONTENTS;
                } else {
                    webContents =
                            WebContentsFactory.createWebContentsWithWarmRenderer(false, false);
                    webContentsStateOnLaunch = WEBCONTENTS_STATE_NO_WEBCONTENTS;
                }
            }
        }

        RecordHistogram.recordEnumeratedHistogram("CustomTabs.WebContentsStateOnLaunch",
                webContentsStateOnLaunch, WEBCONTENTS_STATE_MAX);

        if (!mUsingPrerender) mConnection.resetPostMessageHandlerForSession(mSession, webContents);

        return webContents;
    }

    private WebContents takePrerenderedWebContents() {
        String url = getUrlToLoad();
        String referrerUrl = mConnection.getReferrer(mSession, getIntent());
        return mConnection.takePrerenderedUrl(mSession, url, referrerUrl);
    }

    private WebContents takeAsyncWebContents() {
        int assignedTabId = IntentUtils.safeGetIntExtra(
                getIntent(), IntentHandler.EXTRA_TAB_ID, Tab.INVALID_TAB_ID);
        AsyncTabParams asyncParams = AsyncTabParamsManager.remove(assignedTabId);
        if (asyncParams == null) return null;
        return asyncParams.getWebContents();
    }

    private void initializeMainTab(Tab tab) {
        tab.getTabRedirectHandler().updateIntent(getIntent());
        tab.getView().requestFocus();
        mTabObserver = new CustomTabObserver(
                getApplication(), mSession, mIntentDataProvider.isOpenedByChrome());

        mMetricsObserver = new PageLoadMetricsObserver(mConnection, mSession, tab);
        tab.addObserver(mTabObserver);

        prepareTabBackground(tab);
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        getTabModelSelector().onNativeLibraryReady(getTabContentManager());
    }

    private void recordClientPackageName() {
        String clientName = mConnection.getClientPackageNameForSession(mSession);
        if (TextUtils.isEmpty(clientName)) clientName = mIntentDataProvider.getClientPackageName();
        final String packageName = clientName;
        if (TextUtils.isEmpty(packageName) || packageName.contains(getPackageName())) return;
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RapporServiceBridge.sampleString(
                        "CustomTabs.ServiceClient.PackageName", packageName);
                if (GSAState.isGsaPackageName(packageName)) return;
                RapporServiceBridge.sampleString(
                        "CustomTabs.ServiceClient.PackageNameThirdParty", packageName);
            }
        });
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        setActiveContentHandler(mCustomTabContentHandler);
        if (mHasCreatedTabEarly && !mMainTab.isLoading()) postDeferredStartupIfNeeded();
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();

        if (getSavedInstanceState() != null || !mIsInitialResume) {
            if (mIntentDataProvider.isOpenedByChrome()) {
                RecordUserAction.record("ChromeGeneratedCustomTab.StartedReopened");
            } else {
                RecordUserAction.record("CustomTabs.StartedReopened");
            }
        } else {
            SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
            String lastUrl = preferences.getString(LAST_URL_PREF, null);
            if (lastUrl != null && lastUrl.equals(getUrlToLoad())) {
                RecordUserAction.record("CustomTabsMenuOpenSameUrl");
            } else {
                preferences.edit().putString(LAST_URL_PREF, getUrlToLoad()).apply();
            }

            if (mIntentDataProvider.isOpenedByChrome()) {
                RecordUserAction.record("ChromeGeneratedCustomTab.StartedInitially");
            } else {
                ExternalAppId externalId =
                        IntentHandler.determineExternalIntentSource(getPackageName(), getIntent());
                RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId",
                        externalId.ordinal(), ExternalAppId.INDEX_BOUNDARY.ordinal());

                RecordUserAction.record("CustomTabs.StartedInitially");
            }
        }
        mIsInitialResume = false;
    }

    @Override
    public void onPauseWithNative() {
        super.onPauseWithNative();
        mConnection.notifyNavigationEvent(mSession, CustomTabsCallback.TAB_HIDDEN);
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        setActiveContentHandler(null);
        if (mIsClosing) {
            getTabModelSelector().closeAllTabs(true);
            mTabPersistencePolicy.deleteMetadataStateFileAsync();
        } else {
            getTabModelSelector().saveState();
        }
    }

    /**
     * Loads the current tab with the given load params while taking client
     * referrer and extra headers into account.
     */
    private void loadUrlInTab(final Tab tab, final LoadUrlParams params, long timeStamp) {
        Intent intent = getIntent();
        String url = getUrlToLoad();

        // Caching isFirstLoad value to deal with multiple return points.
        boolean isFirstLoad = mIsFirstLoad;
        mIsFirstLoad = false;

        // The following block is a hack that deals with urls preloaded with
        // the wrong fragment. Does an extra pageload and replaces history.
        if (mHasSpeculated && isFirstLoad
                && UrlUtilities.urlsFragmentsDiffer(mSpeculatedUrl, url)) {
            if (mUsingPrerender) {
                LoadUrlParams temporaryParams = new LoadUrlParams(mSpeculatedUrl);
                IntentHandler.addReferrerAndHeaders(temporaryParams, intent);
                tab.loadUrl(temporaryParams);
            }
            params.setShouldReplaceCurrentEntry(true);
        }

        mTabObserver.trackNextPageLoadFromTimestamp(tab, timeStamp);

        // Manually generating metrics in case the hidden tab has completely finished loading.
        if (mUsingHiddenTab && !tab.isLoading() && !tab.isShowingErrorPage()) {
            mTabObserver.onPageLoadStarted(tab, params.getUrl());
            mTabObserver.onPageLoadFinished(tab);
        }

        // No actual load to do if tab already has the exact correct url.
        if (TextUtils.equals(mSpeculatedUrl, params.getUrl()) && mUsingHiddenTab && isFirstLoad) {
            return;
        }

        IntentHandler.addReferrerAndHeaders(params, intent);
        if (params.getReferrer() == null) {
            params.setReferrer(mConnection.getReferrerForSession(mSession));
        }
        // See ChromeTabCreator#getTransitionType(). This marks the navigation chain as starting
        // from an external intent (unless otherwise specified by an extra in the intent).
        params.setTransitionType(IntentHandler.getTransitionTypeFromIntent(intent,
                PageTransition.LINK | PageTransition.FROM_API));
        tab.loadUrl(params);
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
        return new CustomTabAppMenuPropertiesDelegate(this,
                mIntentDataProvider.getUiType(),
                mIntentDataProvider.getMenuTitles(),
                mIntentDataProvider.isOpenedByChrome(),
                mIntentDataProvider.shouldShowShareMenuItem(),
                mIntentDataProvider.shouldShowStarButton(),
                mIntentDataProvider.shouldShowDownloadButton());
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
    protected int getToolbarLayoutId() {
        return R.layout.custom_tabs_toolbar;
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
        } else if (mIntentDataProvider != null && mIntentDataProvider.isOpenedByChrome()) {
            overridePendingTransition(R.anim.no_anim, R.anim.activity_close_exit);
        }
    }

    /**
     * Finishes the activity and removes the reference from the Android recents.
     *
     * @param reparenting true iff the activity finishes due to tab reparenting.
     */
    public final void finishAndClose(boolean reparenting) {
        if (mIsClosing) return;
        mIsClosing = true;
        if (!reparenting) {
            // Closing the activity destroys the renderer as well. Re-create a spare renderer some
            // time after, so that we have one ready for the next tab open. This does not increase
            // memory consumption, as the current renderer goes away. We create a renderer as a lot
            // of users open several Custom Tabs in a row. The delay is there to avoid jank in the
            // transition animation when closing the tab.
            ThreadUtils.postOnUiThreadDelayed(new Runnable() {
                @Override
                public void run() {
                    WarmupManager.getInstance().createSpareWebContents();
                }
            }, 500);
        }

        handleFinishAndClose();
    }

    /**
     * Internal implementation that finishes the activity and removes the references from Android
     * recents.
     */
    protected void handleFinishAndClose() {
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
                finishAndClose(false);
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
                        if (getActivityTab() == null) return;
                        mIntentDataProvider.sendButtonPendingIntentWithUrl(
                                getApplicationContext(), getActivityTab().getUrl());
                        RecordUserAction.record("CustomTabsCustomActionButtonClick");
                        if (mIntentDataProvider.shouldEnableEmbeddedMediaExperience()
                                && TextUtils.equals(
                                           params.getDescription(), getString(R.string.share))) {
                            RecordUserAction.record(
                                    "CustomTabsCustomActionButtonClick.DownloadsUI.Share");
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
                || id == R.id.help_id || id == R.id.recent_tabs_menu_id
                || id == R.id.new_incognito_tab_menu_id || id == R.id.new_tab_menu_id
                || id == R.id.open_history_menu_id) {
            return true;
        } else if (id == R.id.bookmark_this_page_id) {
            addOrEditBookmark(getActivityTab());
            RecordUserAction.record("MobileMenuAddToBookmarks");
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
        } else if (id == R.id.open_in_browser_id) {
            openCurrentUrlInBrowser(false);
            RecordUserAction.record("CustomTabsMenuOpenInChrome");
            return true;
        } else if (id == R.id.info_menu_id) {
            if (getTabModelSelector().getCurrentTab() == null) return false;
            PageInfoPopup.show(this, getTabModelSelector().getCurrentTab(),
                    getToolbarManager().getContentPublisher(), PageInfoPopup.OPENED_FROM_MENU);
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
     * @return The tab persistence policy for this activity.
     */
    @VisibleForTesting
    CustomTabTabPersistencePolicy getTabPersistencePolicyForTest() {
        return mTabPersistencePolicy;
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
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            willChromeHandleIntent |= ExternalNavigationDelegateImpl
                    .willChromeHandleIntent(intent, true);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        Bundle startActivityOptions = ActivityOptionsCompat.makeCustomAnimation(
                this, R.anim.abc_fade_in, R.anim.abc_fade_out).toBundle();
        if (willChromeHandleIntent || forceReparenting) {
            Runnable finalizeCallback = new Runnable() {
                @Override
                public void run() {
                    finishAndClose(true);
                }
            };

            mMainTab = null;
            // mHasCreatedTabEarly == true => mMainTab != null in the rest of the code.
            mHasCreatedTabEarly = false;
            mConnection.resetPostMessageHandlerForSession(mSession, null);
            tab.detachAndStartReparenting(intent, startActivityOptions, finalizeCallback);
        } else {
            // Temporarily allowing disk access while fixing. TODO: http://crbug.com/581860
            StrictMode.allowThreadDiskWrites();
            try {
                if (mIntentDataProvider.isInfoPage()) {
                    IntentHandler.startChromeLauncherActivityForTrustedIntent(intent);
                } else {
                    startActivity(intent, startActivityOptions);
                }
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
        return true;
    }

    /**
     * @return The URL that should be used from this intent. If it is a WebLite url, it may be
     *         overridden if the Data Reduction Proxy is using Lo-Fi previews.
     */
    private String getUrlToLoad() {
        String url = IntentHandler.getUrlFromIntent(getIntent());

        // Intents fired for media viewers have an additional file:// URI passed along so that the
        // tab can display the actual filename to the user when it is loaded.
        if (mIntentDataProvider.isMediaViewer()) {
            String mediaViewerUrl = mIntentDataProvider.getMediaViewerUrl();
            if (!TextUtils.isEmpty(mediaViewerUrl)) {
                Uri mediaViewerUri = Uri.parse(mediaViewerUrl);
                if (UrlConstants.FILE_SCHEME.equals(mediaViewerUri.getScheme())) {
                    url = mediaViewerUrl;
                }
            }
        }

        if (!TextUtils.isEmpty(url)) {
            url = DataReductionProxySettings.getInstance().maybeRewriteWebliteUrl(url);
        }

        return url;
    }

    /** Sets the initial background color for the Tab, shown before the page content is ready. */
    private void prepareTabBackground(final Tab tab) {
        if (!IntentHandler.isIntentChromeOrFirstParty(getIntent())) return;

        int backgroundColor = mIntentDataProvider.getInitialBackgroundColor();
        if (backgroundColor == Color.TRANSPARENT) return;

        // Set the background color.
        tab.getView().setBackgroundColor(backgroundColor);

        // Unset the background when the page has rendered.
        EmptyTabObserver mediaObserver = new EmptyTabObserver() {
            @Override
            public void didFirstVisuallyNonEmptyPaint(final Tab tab) {
                tab.removeObserver(this);

                // Blink has rendered the page by this point, but Android asynchronously shows it.
                // Introduce a small delay, then actually show the page.
                new Handler().postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        if (!tab.isInitialized() || isActivityDestroyed()) return;
                        tab.getView().setBackgroundResource(0);
                    }
                }, 50);
            }
        };

        tab.addObserver(mediaObserver);
    }

    @Override
    protected void initializeToolbar() {
        super.initializeToolbar();
        if (mIntentDataProvider.isMediaViewer()) getToolbarManager().disableShadow();
    }

    /**
     * Show the web page with CustomTabActivity, without any navigation control.
     * Used in showing the terms of services page or help pages for Chrome.
     * @param context The current activity context.
     * @param url The url of the web page.
     */
    public static void showInfoPage(Context context, String url) {
        // TODO(xingliu): The title text will be the html document title, figure out if we want to
        // use Chrome strings here as EmbedContentViewActivity does.
        CustomTabsIntent customTabIntent = new CustomTabsIntent.Builder()
                .setShowTitle(true)
                .setToolbarColor(ApiCompatibilityUtils.getColor(
                        context.getResources(),
                        R.color.dark_action_bar_color))
                .build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent = ChromeLauncherActivity.createCustomTabActivityIntent(
                context, customTabIntent.intent, false);
        intent.setPackage(context.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CUSTOM_TABS_UI_TYPE_INFO_PAGE);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentHandler.addTrustedIntentExtras(intent);

        context.startActivity(intent);
    }

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // Custom Tabs can be used to open Chrome help pages before the ToS has been accepted.
        if (IntentHandler.isIntentChromeOrFirstParty(intent)
                && IntentUtils.safeGetIntExtra(intent, CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                           CUSTOM_TABS_UI_TYPE_DEFAULT) == CUSTOM_TABS_UI_TYPE_INFO_PAGE) {
            return false;
        }

        return super.requiresFirstRunToBeCompleted(intent);
    }
}
