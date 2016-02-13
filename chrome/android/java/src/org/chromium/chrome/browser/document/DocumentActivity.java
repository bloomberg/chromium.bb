// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.annotation.TargetApi;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.Window;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocument;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocumentTabSwitcher;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.metrics.StartupMetrics;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPreferences;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPromoScreen;
import org.chromium.chrome.browser.signin.SigninPromoScreen;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUma.TabCreationState;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.SingleTabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.InitializationObserver;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.service_tab_launcher.ServiceTabLauncher;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

/**
 * This is the main activity for Chrome while running in document mode.  Each activity
 * instance represents a single web page at a time while providing Chrome functionality.
 *
 * <p>
 * Tab switching is handled via the system wide Android task management system.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class DocumentActivity extends ChromeActivity {
    // Legacy class names to match Chrome pre-44 activity names. See crbug.com/503807
    public static final String LEGACY_CLASS_NAME =
            "com.google.android.apps.chrome.document.DocumentActivity";
    public static final String LEGACY_INCOGNITO_CLASS_NAME =
            "com.google.android.apps.chrome.document.IncognitoDocumentActivity";

    protected static final String KEY_INITIAL_URL = "DocumentActivity.KEY_INITIAL_URL";

    private static final String TAG = "DocumentActivity";

    // Animation exit duration defined in chrome/android/java/res/anim/menu_exit.xml as 150ms,
    // plus add another 20ms for a re-layout.
    private static final int MENU_EXIT_ANIMATION_WAIT_MS = 170;

    private class DocumentTabObserver extends EmptyTabObserver {
        @Override
        public void onPageLoadStarted(Tab tab, String url) {
            // Discard startup navigation measurements when the user interfered and started the
            // 2nd navigation (in activity lifetime) in parallel.
            if (!sIsFirstPageLoadStart) {
                UmaUtils.setRunningApplicationStart(false);
            } else {
                sIsFirstPageLoadStart = false;
            }
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            if (!didStartLoad) return;
            resetIcon();
        }

        @Override
        public void onFaviconUpdated(Tab tab, Bitmap icon) {
            if (icon == null) return;
            if (mLargestFavicon == null || icon.getWidth() > mLargestFavicon.getWidth()
                    || icon.getHeight() > mLargestFavicon.getHeight()) {
                mLargestFavicon = icon;
                updateTaskDescription();
            }
        }

        @Override
        public void onUrlUpdated(Tab tab) {
            assert mTab == tab;

            updateTaskDescription();
            mTabModel.updateEntry(getIntent(), mTab);
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            super.onTitleUpdated(tab);
            updateTaskDescription();
        }

        @Override
        public void onSSLStateUpdated(Tab tab) {
            if (hasSecurityWarningOrError(tab)) resetIcon();
        }

        @Override
        public void onDidNavigateMainFrame(Tab tab, String url, String baseUrl,
                boolean isNavigationToDifferentPage, boolean isFragmentNavigation,
                int statusCode) {
            if (!isNavigationToDifferentPage) return;
            mLargestFavicon = null;
            updateTaskDescription();
        }

        @Override
        public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
            assert mTab == tab;

            updateTaskDescription();
            mTabModel.updateEntry(getIntent(), mTab);
        }

        @Override
        public void onDidChangeThemeColor(Tab tab, int color) {
            updateTaskDescription();
        }

        @Override
        public void onDidAttachInterstitialPage(Tab tab) {
            resetIcon();
        }

        @Override
        public void onDidDetachInterstitialPage(Tab tab) {
            resetIcon();
        }

        @Override
        public void onCrash(Tab tab, boolean sadTabShown) {
            int currentState = ApplicationStatus.getStateForActivity(DocumentActivity.this);
            if (currentState != ActivityState.STOPPED) return;

            if (!isTaskRoot() || IntentUtils.safeGetBooleanExtra(getIntent(),
                    IntentHandler.EXTRA_APPEND_TASK, false)) {
                return;
            }

            // Finishing backgrounded Activities whose renderers have crashed allows us to
            // destroy them and return resources sooner than if we wait for Android to destroy
            // the Activities themselves.  Problematically, this also removes
            // IncognitoDocumentActivity instances from Android's Recents menu and auto-closes
            // the tab.  Instead, take a hit and keep the Activities alive -- Android will
            // eventually destroy the Activities, anyway (crbug.com/450292).
            if (!isIncognito()) finish();
        }

        private boolean hasSecurityWarningOrError(Tab tab) {
            int securityLevel = tab.getSecurityLevel();
            return securityLevel == ConnectionSecurityLevel.SECURITY_ERROR
                    || securityLevel == ConnectionSecurityLevel.SECURITY_WARNING
                    || securityLevel == ConnectionSecurityLevel.SECURITY_POLICY_WARNING;
        }
    }
    private DocumentTabModel mTabModel;
    private InitializationObserver mTabInitializationObserver;

    /**
     * Generates the icon to use in the recent task list.
     */
    private DocumentActivityIcon mIcon;

    /**
     * The tab's largest favicon.
     */
    private Bitmap mLargestFavicon;

    private int mDefaultThemeColor;

    private Tab mTab;
    private FindToolbarManager mFindToolbarManager;
    private boolean mRecordedStartupUma;

    // Whether the page has started loading in this (browser) process once already. Must only be
    // used from the UI thread.
    private static boolean sIsFirstPageLoadStart = true;

    /** Whether the Tab has already been added to the TabModel. */
    private boolean mNeedsToBeAddedToTabModel;

    @Override
    protected boolean isStartedUpCorrectly(Intent intent) {
        int tabId = ActivityDelegate.getTabIdFromIntent(getIntent());
        boolean isDocumentMode = FeatureUtilities.isDocumentMode(this);
        boolean isStartedUpCorrectly = tabId != Tab.INVALID_TAB_ID && isDocumentMode;
        if (!isStartedUpCorrectly) {
            Log.e(TAG, "Discarding Intent: Tab = " + tabId + ", Document mode = " + isDocumentMode);
        }
        return isStartedUpCorrectly;
    }

    @Override
    public void preInflationStartup() {
        // Decide whether to record startup UMA histograms. This is done  early in the main
        // Activity.onCreate() to avoid recording navigation delays when they require user input to
        // proceed. See more details in ChromeTabbedActivity.preInflationStartup().
        if (!LibraryLoader.isInitialized()) {
            UmaUtils.setRunningApplicationStart(true);
        }

        DocumentTabModelSelector.setPrioritizedTabId(
                ActivityDelegate.getTabIdFromIntent(getIntent()));

        mTabModel = ChromeApplication.getDocumentTabModelSelector().getModel(isIncognito());
        mTabModel.startTabStateLoad();
        updateLastTabId();

        SingleTabModelSelector selector = new SingleTabModelSelector(this, isIncognito(), false) {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
                    boolean incognito) {
                // Triggered via open in new tab context menu on NTP.
                return ChromeApplication.getDocumentTabModelSelector().openNewTab(
                        loadUrlParams, type, parent, incognito);
            }
        };
        setTabModelSelector(selector);
        setTabCreators(ChromeApplication.getDocumentTabModelSelector().getTabCreator(false),
                ChromeApplication.getDocumentTabModelSelector().getTabCreator(true));

        super.preInflationStartup();

        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.control_container;
    }

    @Override
    public void postInflationStartup() {
        super.postInflationStartup();

        final int tabId = ActivityDelegate.getTabIdFromIntent(getIntent());
        mTabInitializationObserver = new InitializationObserver(mTabModel) {
                @Override
                public boolean isSatisfied(int currentState) {
                    return currentState >= DocumentTabModelImpl.STATE_LOAD_TAB_STATE_BG_END
                            || mTabModel.isTabStateReady(tabId);
                }

                @Override
                public boolean isCanceled() {
                    return isDestroyed() || isFinishing();
                }

                @Override
                public void runImmediately() {
                    initializeUI();
                }
        };
    }

    @Override
    public void prepareMenu(Menu menu) {
        if (isNewTabPage() && !isIncognito()) {
            menu.findItem(R.id.new_tab_menu_id).setVisible(false);
        }
    }

    @Override
    public void finishNativeInitialization() {
        ChromePreferenceManager preferenceManager =
                ChromePreferenceManager.getInstance(this);
        if (isNewTabPage() && FirstRunStatus.getFirstRunFlowComplete(this)) {
            // Only show promos on second NTP.
            if (preferenceManager.getPromosSkippedOnFirstStart()) {
                // Data reduction promo should be temporarily suppressed if the sign in promo is
                // shown to avoid nagging users too much.
                if (!SigninPromoScreen.launchSigninPromoIfNeeded(this)) {
                    DataReductionPromoScreen.launchDataReductionPromo(this);
                }
            } else {
                preferenceManager.setPromosSkippedOnFirstStart(true);
            }
        }

        FirstRunSignInProcessor.start(this);

        if (!preferenceManager.hasAttemptedMigrationOnUpgrade()) {
            InitializationObserver observer = new InitializationObserver(
                    ChromeApplication.getDocumentTabModelSelector().getModel(false)) {
                @Override
                protected void runImmediately() {
                    DocumentMigrationHelper.migrateTabsToDocumentForUpgrade(DocumentActivity.this,
                            DocumentMigrationHelper.FINALIZE_MODE_NO_ACTION);
                }

                @Override
                public boolean isSatisfied(int currentState) {
                    return currentState == DocumentTabModelImpl.STATE_FULLY_LOADED;
                }

                @Override
                public boolean isCanceled() {
                    return false;
                }
            };

            observer.runWhenReady();
        }

        getWindow().setFeatureInt(Window.FEATURE_INDETERMINATE_PROGRESS,
                Window.PROGRESS_VISIBILITY_OFF);

        // Initialize the native-side TabModels so that the Profile is available when necessary.
        ChromeApplication.getDocumentTabModelSelector().onNativeLibraryReady();
        mTabInitializationObserver.runWhenReady();

        if (mNeedsToBeAddedToTabModel) {
            mNeedsToBeAddedToTabModel = false;
            mTabModel.addTab(getIntent(), mTab);
            getTabModelSelector().setTab(mTab);
        }

        super.finishNativeInitialization();
    }

    /**
     * @return The ID of the Tab.
     */
    protected final int determineTabId() {
        int tabId = ActivityDelegate.getTabIdFromIntent(getIntent());
        if (tabId == Tab.INVALID_TAB_ID && mTab != null) tabId = mTab.getId();
        return tabId;
    }

    /**
     * Determine the last known URL that this Document was displaying when it was stopped.
     * @return URL that the user was last known to have seen, or null if one couldn't be found.
     */
    private String determineLastKnownUrl() {
        int tabId = determineTabId();
        String url = mTabModel.getCurrentUrlForDocument(tabId);
        if (TextUtils.isEmpty(url)) url = determineInitialUrl(tabId);
        return url;
    }

    /**
     * Determine the URL that this Document was initially opened for.  It can be stashed in multiple
     * locations because IncognitoDocumentActivities are disallowed from passing URLs in the Intent.
     * @param tabId ID of the Tab owned by this Activity.
     * @return Initial URL, or null if it couldn't be determined.
     */
    protected String determineInitialUrl(int tabId) {
        String initialUrl = null;

        // Check if the TabModel has it.
        if (mTabModel != null) {
            initialUrl = mTabModel.getInitialUrlForDocument(tabId);
        }

        // Check if the URL was passed through the Intent.
        if (TextUtils.isEmpty(initialUrl) && getIntent() != null) {
            initialUrl = IntentHandler.getUrlFromIntent(getIntent());
        }

        // Check the Tab's history.
        if (TextUtils.isEmpty(initialUrl) && mTab != null
                && mTab.getWebContents() != null) {
            NavigationEntry entry =
                    mTab.getWebContents().getNavigationController().getEntryAtIndex(0);
            if (entry != null) initialUrl = entry.getOriginalUrl();
        }

        return initialUrl;
    }

    @Override
    public CharSequence onCreateDescription() {
        return mTab != null ? mTab.getTitle() : "";
    }

    @Override
    public final Tab getActivityTab() {
        return mTab;
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        handleDocumentUma();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        StartupMetrics.getInstance().recordHistogram(true);
    }

    private void handleDocumentUma() {
        if (mRecordedStartupUma) {
            DocumentUma.recordStartedBy(
                    DocumentMetricIds.STARTED_BY_ACTIVITY_BROUGHT_TO_FOREGROUND);
        } else {
            mRecordedStartupUma = true;

            if (getSavedInstanceState() == null) {
                DocumentUma.recordStartedBy(getPackageName(), getIntent());
            } else {
                DocumentUma.recordStartedBy(DocumentMetricIds.STARTED_BY_ACTIVITY_RESTARTED);
            }
        }
        DocumentUma.recordInDocumentMode(true);
    }

    @Override
    public void onPause() {
        // If finishing, release all the active media players as we don't know when onStop()
        // will get called.
        super.onPause();
        if (isFinishing() && mTab != null && mTab.getWebContents() != null) {
            mTab.getWebContents().suspendAllMediaPlayers();
            mTab.getWebContents().setAudioMuted(true);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (hasWindowFocus) updateLastTabId();
    }

    @Override
    public void onResume() {
        super.onResume();

        if (getIntent() != null) {
            DocumentUtils.finishOtherTasksWithTabID(
                    ActivityDelegate.getTabIdFromIntent(getIntent()), getTaskId());
        }
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();

        if (mTab != null) {
            AsyncTabParams asyncParams = AsyncTabParamsManager.remove(
                    ActivityDelegate.getTabIdFromIntent(getIntent()));
            if (asyncParams != null && asyncParams.getLoadUrlParams().getUrl() != null) {
                loadLastKnownUrl(asyncParams);
            }
        }
        StartupMetrics.getInstance().recordHistogram(false);
    }

    @Override
    public SingleTabModelSelector getTabModelSelector() {
        return (SingleTabModelSelector) super.getTabModelSelector();
    }

    private void loadLastKnownUrl(AsyncTabParams asyncParams) {
        Intent intent = getIntent();
        if (asyncParams != null && asyncParams.getOriginalIntent() != null) {
            intent = asyncParams.getOriginalIntent();
        }

        boolean isIntentChromeOrFirstParty = IntentHandler.isIntentChromeOrFirstParty(intent,
                getApplicationContext());
        int transitionType = intent == null ? PageTransition.LINK
                : IntentUtils.safeGetIntExtra(intent, IntentHandler.EXTRA_PAGE_TRANSITION_TYPE,
                        PageTransition.LINK);
        if ((transitionType != PageTransition.TYPED)
                && (transitionType != PageTransition.LINK)
                && !isIntentChromeOrFirstParty) {
            // Only 1st party authenticated applications can set the transition type to be
            // anything other than TYPED for security reasons.
            transitionType = PageTransition.LINK;
        }
        if (transitionType == PageTransition.LINK) {
            transitionType |= PageTransition.FROM_API;
        }

        String url = (asyncParams != null && asyncParams.getLoadUrlParams().getUrl() != null)
                ? asyncParams.getLoadUrlParams().getUrl() : determineLastKnownUrl();

        LoadUrlParams loadUrlParams =
                asyncParams == null ? new LoadUrlParams(url) : asyncParams.getLoadUrlParams();
        loadUrlParams.setTransitionType(transitionType);
        if (getIntent() != null) {
            loadUrlParams.setIntentReceivedTimestamp(getOnCreateTimestampUptimeMs());
        }

        if (intent != null) {
            IntentHandler.addReferrerAndHeaders(loadUrlParams, intent, this);
        }

        if (asyncParams != null && asyncParams.getOriginalIntent() != null) {
            mTab.getTabRedirectHandler().updateIntent(asyncParams.getOriginalIntent());
        } else {
            if (getIntent() != null) {
                try {
                    intent = IntentUtils.safeGetParcelableExtra(getIntent(),
                            IntentHandler.EXTRA_ORIGINAL_INTENT);
                } catch (Throwable t) {
                    // Ignore exception.
                }
            }
            mTab.getTabRedirectHandler().updateIntent(intent);
        }

        mTab.loadUrl(loadUrlParams);
        if (asyncParams != null && asyncParams.getRequestId() != null
                && asyncParams.getRequestId() > 0) {
            ServiceTabLauncher.onWebContentsForRequestAvailable(
                    asyncParams.getRequestId(), mTab.getWebContents());
        }
    }

    @Override
    public void terminateIncognitoSession() {
        ChromeApplication.getDocumentTabModelSelector().getModel(true).closeAllTabs();
    }

    private void initializeUI() {
        mIcon = new DocumentActivityIcon(this);
        mDefaultThemeColor = isIncognito()
                ? ApiCompatibilityUtils.getColor(getResources(), R.color.incognito_primary_color)
                : ApiCompatibilityUtils.getColor(getResources(), R.color.default_primary_color);
        AsyncTabParams params = AsyncTabParamsManager.remove(
                ActivityDelegate.getTabIdFromIntent(getIntent()));
        AsyncTabCreationParams asyncParams = params instanceof AsyncTabCreationParams
                ? (AsyncTabCreationParams) params : null;
        boolean isAffiliated = asyncParams != null ? asyncParams.isAffiliated() : false;
        boolean isCreatedWithWebContents = asyncParams != null
                && asyncParams.getWebContents() != null;

        mTab = createActivityTab(asyncParams);

        if (asyncParams != null && asyncParams.getWebContents() != null) {
            Intent parentIntent = IntentUtils.safeGetParcelableExtra(getIntent(),
                    IntentHandler.EXTRA_PARENT_INTENT);
            mTab.setParentIntent(parentIntent);
        }

        if (mTabModel.isNativeInitialized()) {
            mTabModel.addTab(getIntent(), mTab);
        } else {
            mNeedsToBeAddedToTabModel = true;
        }

        getTabModelSelector().setTab(mTab);

        if (mTab.didFailToRestore()
                || (asyncParams != null && asyncParams.getLoadUrlParams().getUrl() != null)) {
            if (!isCreatedWithWebContents) {
                // Don't load tabs in the background on low end devices. We will call
                // loadLastKnownUrl() in onResumeWithNative() next time activity is resumed.
                if (SysUtils.isLowEndDevice() && isAffiliated) {
                    // onResumeWithNative() wants asyncParams's URL to be non-null.
                    LoadUrlParams loadUrlParams = asyncParams.getLoadUrlParams();
                    if (loadUrlParams.getUrl() == null) {
                        loadUrlParams.setUrl(determineLastKnownUrl());
                    }

                    AsyncTabParamsManager.add(
                            ActivityDelegate.getTabIdFromIntent(getIntent()), asyncParams);

                    // Use the URL as the document title until tab is loaded.
                    updateTaskDescription(loadUrlParams.getUrl(), null);
                } else {
                    loadLastKnownUrl(asyncParams);
                }
            }
            mTab.setShouldPreserve(IntentUtils.safeGetBooleanExtra(getIntent(),
                    IntentHandler.EXTRA_PRESERVE_TASK, false));
        }

        ToolbarControlContainer controlContainer =
                (ToolbarControlContainer) findViewById(R.id.control_container);
        LayoutManagerDocument layoutDriver = null;
        OverviewModeBehavior overviewModeBehavior = null;
        OnClickListener tabSwitcherClickHandler = null;
        if (FeatureUtilities.isTabSwitchingEnabledInDocumentMode(getApplicationContext())) {
            LayoutManagerDocumentTabSwitcher layoutDriverTabSwitcher =
                    new LayoutManagerDocumentTabSwitcher(getCompositorViewHolder());
            layoutDriverTabSwitcher.addOverviewModeObserver(new OverviewModeObserver() {
                @Override
                public void onOverviewModeStartedShowing(boolean showToolbar) {
                    if (mFindToolbarManager != null) mFindToolbarManager.hideToolbar();
                    if (getAssistStatusHandler() != null) {
                        getAssistStatusHandler().updateAssistState();
                    }
                    ApiCompatibilityUtils.setStatusBarColor(getWindow(), Color.BLACK);
                    StartupMetrics.getInstance().recordOpenedTabSwitcher();
                }

                @Override
                public void onOverviewModeFinishedShowing() {}

                @Override
                public void onOverviewModeStartedHiding(
                        boolean showToolbar, boolean delayAnimation) {}

                @Override
                public void onOverviewModeFinishedHiding() {
                    if (getAssistStatusHandler() != null) {
                        getAssistStatusHandler().updateAssistState();
                    }
                    if (getActivityTab() != null) {
                        LayoutManagerDocumentTabSwitcher manager =
                                (LayoutManagerDocumentTabSwitcher) getCompositorViewHolder()
                                .getLayoutManager();
                        boolean isInOverviewMode = manager != null && manager.overviewVisible();
                        setStatusBarColor(getActivityTab(),
                                isInOverviewMode ? Color.BLACK : getActivityTab().getThemeColor());
                    }
                }
            });
            layoutDriver = layoutDriverTabSwitcher;
            overviewModeBehavior = layoutDriverTabSwitcher;

            tabSwitcherClickHandler = new OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (getFullscreenManager() != null
                            && getFullscreenManager().getPersistentFullscreenMode()) {
                        return;
                    }
                    LayoutManagerDocumentTabSwitcher manager =
                            (LayoutManagerDocumentTabSwitcher) getCompositorViewHolder()
                            .getLayoutManager();
                    manager.toggleOverview();
                }
            };
        } else {
            layoutDriver = new LayoutManagerDocument(getCompositorViewHolder());
        }
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content), controlContainer);

        mFindToolbarManager = new FindToolbarManager(this, getTabModelSelector(),
                getToolbarManager().getActionModeController()
                        .getActionModeCallback());

        if (getContextualSearchManager() != null) {
            getContextualSearchManager().setFindToolbarManager(mFindToolbarManager);
        }

        getToolbarManager().initializeWithNative(getTabModelSelector(), getFullscreenManager(),
                mFindToolbarManager, overviewModeBehavior, layoutDriver, tabSwitcherClickHandler,
                null, null, null);

        mTab.setFullscreenManager(getFullscreenManager());

        mTab.addObserver(new DocumentTabObserver());

        removeWindowBackground();

        if (mTab != null) {
            DataReductionPreferences.launchDataReductionSSLInfoBar(
                    DocumentActivity.this, mTab.getWebContents());
        }
    }

    private void resetIcon() {
        mLargestFavicon = null;
        updateTaskDescription();
    }

    private void updateLastTabId() {
        ChromeApplication.getDocumentTabModelSelector().selectModel(isIncognito());
        int tabId = mTab == null
                 ? ActivityDelegate.getTabIdFromIntent(getIntent()) : mTab.getId();
        mTabModel.setLastShownId(tabId);
    }

    @Override
    public boolean handleBackPressed() {
        if (mTab == null) return false;

        if (exitFullscreenIfShowing()) return true;

        if (mTab.canGoBack()) {
            mTab.goBack();
        } else if (!mTab.shouldPreserve()) {
            finishAndRemoveTask();
        } else {
            moveTaskToBack(true);
        }
        return true;
    }


    @Override
    public TabDelegate getTabCreator(boolean incognito) {
        return (TabDelegate) super.getTabCreator(incognito);
    }

    private Tab createActivityTab(AsyncTabCreationParams asyncParams) {
        boolean isAffiliated = asyncParams != null ? asyncParams.isAffiliated() : false;
        boolean isCreatedWithWebContents = asyncParams != null
                && asyncParams.getWebContents() != null;
        int tabId = determineTabId();
        int parentTabId = getIntent().getIntExtra(
                IntentHandler.EXTRA_PARENT_TAB_ID, Tab.INVALID_TAB_ID);
        TabState tabState = mTabModel.getTabStateForDocument(tabId);
        boolean hasTabState = tabState != null;
        String url = determineLastKnownUrl();

        Tab tab = new Tab(tabId, parentTabId, isIncognito(), this, getWindowAndroid(),
                getTabLaunchType(url, hasTabState, isAffiliated),
                getTabCreationState(hasTabState, isAffiliated), tabState);

        // Initialize tab and web contents.
        WebContents webContents = isCreatedWithWebContents ? asyncParams.getWebContents() : null;
        tab.initialize(webContents, getTabContentManager(),
                new DocumentTabDelegateFactory(), isAffiliated, hasTabState);
        tab.getView().requestFocus();
        if (isCreatedWithWebContents) webContents.resumeLoadingCreatedWebContents();

        return tab;
    }

    /**
     * This cannot return {@link TabCreationState#FROZEN_ON_RESTORE_FAILED} since the Tab has
     * to be created first to even attempt restore.
     */
    private TabCreationState getTabCreationState(boolean hasTabState, boolean isAffiliated) {
        if (hasTabState) return TabCreationState.FROZEN_ON_RESTORE;
        if (isAffiliated) {
            if (SysUtils.isLowEndDevice()) return TabCreationState.FROZEN_FOR_LAZY_LOAD;
            return TabCreationState.LIVE_IN_BACKGROUND;
        }
        return TabCreationState.LIVE_IN_FOREGROUND;
    }

    private TabLaunchType getTabLaunchType(
            String url, boolean hasTabState, boolean isAffiliated) {
        if (hasTabState) return TabLaunchType.FROM_RESTORE;
        if (isAffiliated) return TabLaunchType.FROM_LONGPRESS_BACKGROUND;
        if (!TextUtils.isEmpty(url) && url.equals(UrlConstants.NTP_URL)) {
            return TabLaunchType.FROM_MENU_OR_OVERVIEW;
        }
        return TabLaunchType.FROM_EXTERNAL_APP;
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        AsyncTabCreationParams asyncParams =
                new AsyncTabCreationParams(new LoadUrlParams(searchUrl, PageTransition.LINK));
        asyncParams.setDocumentStartedBy(DocumentMetricIds.STARTED_BY_CONTEXTUAL_SEARCH);
        getTabCreator(false).createNewTab(
                asyncParams, TabLaunchType.FROM_MENU_OR_OVERVIEW, getActivityTab().getId());
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.new_tab_menu_id) {
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    launchNtp(false);
                }
            }, MENU_EXIT_ANIMATION_WAIT_MS);
            RecordUserAction.record("MobileMenuNewTab");
            RecordUserAction.record("MobileNewTabOpened");
        } else if (id == R.id.new_incognito_tab_menu_id) {
            // This action must be recorded before opening the incognito tab since UMA actions
            // are dropped when an incognito tab is open.
            RecordUserAction.record("MobileMenuNewIncognitoTab");
            RecordUserAction.record("MobileNewTabOpened");
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    launchNtp(true);
                }
            }, MENU_EXIT_ANIMATION_WAIT_MS);
        } else if (id == R.id.all_bookmarks_menu_id) {
            StartupMetrics.getInstance().recordOpenedBookmarks();
            BookmarkUtils.showBookmarkManager(this);
            RecordUserAction.record("MobileMenuAllBookmarks");
        } else if (id == R.id.recent_tabs_menu_id) {
            NewTabPage.launchRecentTabsDialog(this, mTab);
            RecordUserAction.record("MobileMenuOpenTabs");
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
        } else if (id == R.id.focus_url_bar) {
            if (getToolbarManager().isInitialized()) getToolbarManager().setUrlBarFocus(true);
        } else {
            return super.onMenuOrKeyboardAction(id, fromMenu);
        }
        return true;
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
    public boolean shouldShowAppMenu() {
        if (mTab == null || !getToolbarManager().isInitialized()) {
            return false;
        }

        return super.shouldShowAppMenu();
    }

    @Override
    protected void showAppMenuForKeyboardEvent() {
        if (!getToolbarManager().isInitialized()) return;
        super.showAppMenuForKeyboardEvent();
    }

    private void updateTaskDescription() {
        if (mTab == null) {
            updateTaskDescription(null, null);
            return;
        }

        if (isNewTabPage() && !isIncognito()) {
            // NTP needs a new color in recents, but uses the default application title and icon;
            updateTaskDescription(null, null);
            return;
        }

        String label = mTab.getTitle();
        String domain = UrlUtilities.getDomainAndRegistry(mTab.getUrl(), false);
        if (TextUtils.isEmpty(label)) {
            label = domain;
        }
        if (mLargestFavicon == null && TextUtils.isEmpty(label)) {
            updateTaskDescription(null, null);
            return;
        }

        Bitmap bitmap = null;
        if (!isIncognito()) {
            bitmap = mIcon.getBitmap(mTab.getUrl(), mLargestFavicon);
        }

        updateTaskDescription(label, bitmap);
    }

    private void updateTaskDescription(String label, Bitmap icon) {
        int color = mDefaultThemeColor;
        if (getActivityTab() != null && !getActivityTab().isDefaultThemeColor()) {
            color = getActivityTab().getThemeColor();
        }
        ApiCompatibilityUtils.setTaskDescription(this, label, icon, color);
    }

    /**
     * @return Whether the {@link DocumentTab} this activity uses is incognito.
     */
    protected boolean isIncognito() {
        return false;
    }

    /**
     * @return Whether this activity contains a tab that is showing the new tab page.
     */
    boolean isNewTabPage() {
        String url;
        if (mTab == null) {
            // If the Tab hasn't been created yet, then we're really early in initialization.
            // Use a combination of the original URL from the Intent and whether or not the Tab is
            // retargetable to know whether or not the user navigated away from the NTP.
            // If the Entry doesn't exist, then the DocumentActivity never got a chance to add
            // itself to the TabList and is likely to be retargetable.
            int tabId = ActivityDelegate.getTabIdFromIntent(getIntent());
            url = IntentHandler.getUrlFromIntent(getIntent());
            if (mTabModel.hasEntryForTabId(tabId) && !mTabModel.isRetargetable(tabId)) return false;
        } else {
            url = mTab.getUrl();
        }
        return NewTabPage.isNTPUrl(url);
    }

    /**
     * Determines whether the given class can be classified as a DocumentActivity (this includes
     * both regular document activity and incognito document activity).
     * @param className The class name to inspect.
     * @return Whether the className is that of a document activity.
     */
    public static boolean isDocumentActivity(String className) {
        return TextUtils.equals(className, IncognitoDocumentActivity.class.getName())
                || TextUtils.equals(className, DocumentActivity.class.getName())
                || TextUtils.equals(className, LEGACY_CLASS_NAME)
                || TextUtils.equals(className, LEGACY_INCOGNITO_CLASS_NAME);
    }

    /**
     * Launch a new DocumentActivity showing the new tab page.
     * @param incognito Whether the new NTP should be in incognito mode.
     */
    private void launchNtp(boolean incognito) {
        if (incognito && !PrefServiceBridge.getInstance().isIncognitoModeEnabled()) return;
        getTabCreator(incognito).launchNTP();
    }
}
