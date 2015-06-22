// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.annotation.TargetApi;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeMobileApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.appmenu.ChromeAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocument;
import org.chromium.chrome.browser.document.DocumentTab.DocumentTabObserver;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkUtils;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.bandwidth.BandwidthReductionPreferences;
import org.chromium.chrome.browser.preferences.bandwidth.DataReductionPromoScreen;
import org.chromium.chrome.browser.signin.SigninPromoScreen;
import org.chromium.chrome.browser.ssl.ConnectionSecurityLevel;
import org.chromium.chrome.browser.tabmodel.SingleTabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.InitializationObserver;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.components.service_tab_launcher.ServiceTabLauncher;
import org.chromium.content.browser.ContentVideoView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.navigation_controller.LoadURLType;
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
    protected static final String KEY_INITIAL_URL = "DocumentActivity.KEY_INITIAL_URL";

    private static final String TAG = "DocumentActivity";

    private static final int APP_ICON_MIN_SIZE_DP = 32;
    private static final int APP_ICON_SIZE_DP = 64;
    private static final int APP_ICON_CORNER_RADIUS_DP = 3;
    private static final int APP_ICON_TEXT_SIZE_DP = 30;
    private static final int APP_ICON_DEFAULT_BACKGROUND_COLOR = Color.rgb(50, 50, 50);

    // Animation exit duration defined in chrome/android/java/res/anim/menu_exit.xml as 150ms,
    // plus add another 20ms for a re-layout.
    private static final int MENU_EXIT_ANIMATION_WAIT_MS = 170;

    private DocumentTabModel mTabModel;
    private InitializationObserver mTabInitializationObserver;

    private Bitmap mIcon;
    // Indicates whether mIcon was generated by RoundedIconGenerator.
    private boolean mIsUsingGeneratedIcon;

    private Integer mThemeColor;
    private int mDefaultThemeColor;

    private DocumentTab mDocumentTab;
    private ToolbarManager mToolbarManager;

    private ChromeAppMenuPropertiesDelegate mChromeAppMenuPropertiesDelegate;
    private AppMenuHandler mAppMenuHandler;
    private RoundedIconGenerator mDocumentAppIconGenerator;
    private FindToolbarManager mFindToolbarManager;
    private boolean mRecordedStartupUma;

    // Whether the page has started loading in this (browser) process once already. Must only be
    // used from the UI thread.
    private static boolean sIsFirstPageLoadStart = true;

    /** Whether the DocumentTab has already been added to the TabModel. */
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

        mTabModel = ChromeMobileApplication.getDocumentTabModelSelector().getModel(isIncognito());
        mTabModel.startTabStateLoad();
        updateLastTabId();

        SingleTabModelSelector selector = new SingleTabModelSelector(this, isIncognito(), false) {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent,
                    boolean incognito) {
                // Triggered via open in new tab context menu on NTP.
                return ChromeMobileApplication.getDocumentTabModelSelector().openNewTab(
                        loadUrlParams, type, parent, incognito);
            }
        };
        setTabModelSelector(selector);
        setTabCreators(ChromeMobileApplication.getDocumentTabModelSelector().getTabCreator(false),
                ChromeMobileApplication.getDocumentTabModelSelector().getTabCreator(true));

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

        ToolbarControlContainer controlContainer =
                ((ToolbarControlContainer) findViewById(R.id.control_container));
        mChromeAppMenuPropertiesDelegate = new ChromeAppMenuPropertiesDelegate(this);
        mAppMenuHandler = new AppMenuHandler(this, mChromeAppMenuPropertiesDelegate,
                R.menu.main_menu);
        mToolbarManager = new ToolbarManager(this, controlContainer, mAppMenuHandler,
                mChromeAppMenuPropertiesDelegate, getCompositorViewHolder().getInvalidator());

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
    @VisibleForTesting
    public AppMenuHandler getAppMenuHandler() {
        return mAppMenuHandler;
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
                    ChromeMobileApplication.getDocumentTabModelSelector().getModel(false)) {
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
        ChromeMobileApplication.getDocumentTabModelSelector().onNativeLibraryReady();
        mTabInitializationObserver.runWhenReady();

        if (mNeedsToBeAddedToTabModel) {
            mNeedsToBeAddedToTabModel = false;
            mTabModel.addTab(getIntent(), mDocumentTab);
            getTabModelSelector().setTab(mDocumentTab);
        }

        super.finishNativeInitialization();
    }

    @Override
    protected void onDeferredStartup() {
        super.onDeferredStartup();
        mToolbarManager.onDeferredStartup(getOnCreateTimestampMs(), getClass().getSimpleName());
    }

    @Override
    public boolean hasDoneFirstDraw() {
        return mToolbarManager.hasDoneFirstDraw();
    }

    /**
     * @return The ID of the Tab.
     */
    protected final int determineTabId() {
        int tabId = ActivityDelegate.getTabIdFromIntent(getIntent());
        if (tabId == Tab.INVALID_TAB_ID && mDocumentTab != null) tabId = mDocumentTab.getId();
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
        if (TextUtils.isEmpty(initialUrl) && mDocumentTab != null
                && mDocumentTab.getWebContents() != null) {
            NavigationEntry entry =
                    mDocumentTab.getWebContents().getNavigationController().getEntryAtIndex(0);
            if (entry != null) initialUrl = entry.getOriginalUrl();
        }

        return initialUrl;
    }

    @Override
    public CharSequence onCreateDescription() {
        return mDocumentTab != null ? mDocumentTab.getTitle() : "";
    }

    @Override
    public final DocumentTab getActivityTab() {
        return mDocumentTab;
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        handleDocumentUma();
        ChromeLauncherActivity.sendExceptionCount();
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
        if (isFinishing() && mDocumentTab != null && mDocumentTab.getWebContents() != null) {
            mDocumentTab.getWebContents().releaseMediaPlayers();
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
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

        if (mDocumentTab != null) {
            PendingDocumentData pendingData = ChromeMobileApplication.getDocumentTabModelSelector()
                    .removePendingDocumentData(ActivityDelegate.getTabIdFromIntent(getIntent()));
            if (pendingData != null && pendingData.url != null) {
                loadLastKnownUrl(pendingData);
            }
            mDocumentTab.show(TabSelectionType.FROM_USER);
        }
    }

    @Override
    protected void onDestroyInternal() {
        if (mToolbarManager != null) mToolbarManager.destroy();

        super.onDestroyInternal();
    }

    @Override
    public void onStopWithNative() {
        if (mAppMenuHandler != null) mAppMenuHandler.hideAppMenu();
        super.onStopWithNative();
    }

    @Override
    public SingleTabModelSelector getTabModelSelector() {
        return (SingleTabModelSelector) super.getTabModelSelector();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mAppMenuHandler != null) mAppMenuHandler.hideAppMenu();
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public void onOrientationChange(int orientation) {
        super.onOrientationChange(orientation);
        mToolbarManager.onOrientationChange();
    }

    @Override
    protected void onAccessibilityModeChanged(boolean enabled) {
        super.onAccessibilityModeChanged(enabled);
        mToolbarManager.onAccessibilityStatusChanged(enabled);
    }

    private void loadLastKnownUrl(PendingDocumentData pendingData) {
        Intent intent = getIntent();
        if (pendingData != null && pendingData.originalIntent != null) {
            intent = pendingData.originalIntent;
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

        String url = (pendingData != null && pendingData.url != null) ? pendingData.url :
                determineLastKnownUrl();

        LoadUrlParams loadUrlParams = new LoadUrlParams(url, transitionType);
        if (getIntent() != null) {
            loadUrlParams.setIntentReceivedTimestamp(getOnCreateTimestampUptimeMs());
        }

        IntentHandler.addReferrerAndHeaders(loadUrlParams, intent, this);

        if (pendingData != null) {
            if (pendingData.postData != null) {
                loadUrlParams.setPostData(pendingData.postData);
                loadUrlParams.setLoadType(LoadURLType.BROWSER_INITIATED_HTTP_POST);
            }
            if (pendingData.extraHeaders != null) {
                loadUrlParams.setVerbatimHeaders(pendingData.extraHeaders);
            }
            if (pendingData.referrer != null) {
                loadUrlParams.setReferrer(pendingData.referrer);
            }
            mDocumentTab.getTabRedirectHandler().updateIntent(pendingData.originalIntent);
        } else {
            if (getIntent() != null) {
                try {
                    intent = IntentUtils.safeGetParcelableExtra(getIntent(),
                            IntentHandler.EXTRA_ORIGINAL_INTENT);
                } catch (Throwable t) {
                    // Ignore exception.
                }
            }
            mDocumentTab.getTabRedirectHandler().updateIntent(intent);
        }

        mDocumentTab.loadUrl(loadUrlParams);
        if (pendingData != null && pendingData.requestId > 0) {
            ServiceTabLauncher.onWebContentsForRequestAvailable(
                    pendingData.requestId, mDocumentTab.getWebContents());
        }

        if (getIntent() != null && IntentUtils.safeGetBooleanExtra(getIntent(),
                IntentHandler.EXTRA_USE_DESKTOP_USER_AGENT, false)) {
            // The desktop user agent can't be carried over without the Tab already having a URL,
            // so we're forced to set the user agent and then reload the page.
            mDocumentTab.setUseDesktopUserAgent(true, true);
        }
    }

    @Override
    public void terminateIncognitoSession() {
        ChromeMobileApplication.getDocumentTabModelSelector().getModel(true).closeAllTabs();
    }

    private void initializeUI() {
        mDefaultThemeColor = isIncognito()
                ? getResources().getColor(R.color.incognito_primary_color)
                : getResources().getColor(R.color.default_primary_color);
        PendingDocumentData pendingData = ChromeMobileApplication.getDocumentTabModelSelector()
                .removePendingDocumentData(ActivityDelegate.getTabIdFromIntent(getIntent()));
        int tabId = determineTabId();
        TabState tabState = mTabModel.getTabStateForDocument(tabId);
        mDocumentTab = DocumentTab.create(DocumentActivity.this, isIncognito(),
                getWindowAndroid(), determineLastKnownUrl(),
                pendingData != null ? pendingData.webContents : null,
                pendingData != null ? pendingData.webContentsPaused : false, tabState);
        if (mTabModel.isNativeInitialized()) {
            mTabModel.addTab(getIntent(), mDocumentTab);
        } else {
            mNeedsToBeAddedToTabModel = true;
        }

        mAppMenuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (!isVisible) {
                    mChromeAppMenuPropertiesDelegate.onMenuDismissed();
                }
            }
        });

        getTabModelSelector().setTab(mDocumentTab);

        if (!mDocumentTab.didRestoreState() || (pendingData != null && pendingData.url != null)) {
            if (!mDocumentTab.isCreatedWithWebContents()) {
                // Don't load tabs in the background on low end devices. We will call
                // loadLastKnownUrl() in onResumeWithNative() next time activity is resumed.
                int launchMode = IntentUtils.safeGetIntExtra(getIntent(),
                        ChromeLauncherActivity.EXTRA_LAUNCH_MODE,
                        ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND);
                if (SysUtils.isLowEndDevice()
                        && launchMode == ChromeLauncherActivity.LAUNCH_MODE_AFFILIATED
                        && pendingData != null) {
                    // onResumeWithNative() wants pendingData.url to be non-null
                    if (pendingData.url == null) {
                        pendingData.url = determineLastKnownUrl();
                    }
                    ChromeMobileApplication.getDocumentTabModelSelector().addPendingDocumentData(
                            ActivityDelegate.getTabIdFromIntent(getIntent()), pendingData);
                    // Use the URL as the document title until tab is loaded
                    updateTaskDescription(pendingData.url, null);
                } else {
                    loadLastKnownUrl(pendingData);
                }
            }
            mDocumentTab.setShouldPreserve(IntentUtils.safeGetBooleanExtra(getIntent(),
                    IntentHandler.EXTRA_PRESERVE_TASK, false));
        }

        ToolbarControlContainer controlContainer =
                (ToolbarControlContainer) findViewById(R.id.control_container);
        LayoutManagerDocument layoutDriver = new LayoutManagerDocument(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content), controlContainer);

        mFindToolbarManager = new FindToolbarManager(this, getTabModelSelector(),
                mToolbarManager.getContextualMenuBar()
                        .getCustomSelectionActionModeCallback());

        mToolbarManager.initializeWithNative(getTabModelSelector(), getFullscreenManager(),
                mFindToolbarManager, null, layoutDriver, null, null, null, null);

        mDocumentTab.setFullscreenManager(getFullscreenManager());

        mDocumentTab.addObserver(new DocumentTabObserver() {
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
                mThemeColor = tab.getWebContents().getThemeColor(mDefaultThemeColor);
                mIcon = null;
                updateTaskDescription();
            }

            @Override
            protected void onFaviconReceived(Bitmap image) {
                super.onFaviconReceived(image);
                int newHeight = image.getHeight();
                int newWidth = image.getWidth();
                int minSize =
                        (int) getResources().getDisplayMetrics().density * APP_ICON_MIN_SIZE_DP;
                if (newHeight < minSize || newWidth < minSize) return;
                if (mIcon != null && !mIsUsingGeneratedIcon && mIcon.getWidth() >= newWidth
                        && mIcon.getHeight() >= newHeight) {
                    return;
                }
                mIcon = image;
                mIsUsingGeneratedIcon = false;
                updateTaskDescription();
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                assert mDocumentTab == tab;

                updateTaskDescription();
                mTabModel.updateEntry(getIntent(), mDocumentTab);
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                super.onTitleUpdated(tab);
                updateTaskDescription();
            }

            @Override
            public void onSSLStateUpdated(Tab tab) {
                if (hasSecurityWarningOrError(tab)) resetThemeColorAndIcon();
            }

            @Override
            public void onDidNavigateMainFrame(Tab tab, String url, String baseUrl,
                    boolean isNavigationToDifferentPage, boolean isFragmentNavigation,
                    int statusCode) {
                if (!isNavigationToDifferentPage) return;
                mIcon = null;
            }

            @Override
            public void onLoadStopped(Tab tab) {
                assert mDocumentTab == tab;

                updateTaskDescription();
                mTabModel.updateEntry(getIntent(), mDocumentTab);
            }

            @Override
            public void onDidChangeThemeColor(int color) {
                if (hasSecurityWarningOrError(mDocumentTab)) return;
                if (color == Color.TRANSPARENT) color = mDefaultThemeColor;

                // Ignore any transparency value.
                color |= 0xFF000000;

                mThemeColor = Integer.valueOf(color);
                updateTaskDescription();
            }

            @Override
            public void onDidAttachInterstitialPage(Tab tab) {
                resetThemeColorAndIcon();
            }

            @Override
            public void onDidDetachInterstitialPage(Tab tab) {
                mThemeColor = tab.getWebContents().getThemeColor(mDefaultThemeColor);
                mIcon = null;
                updateTaskDescription();
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

            @Override
            public void onSetCoveredByChildActivity() {
                mTabModel.updateEntry(getIntent(), mDocumentTab);
            }

            private boolean hasSecurityWarningOrError(Tab tab) {
                int securityLevel = tab.getSecurityLevel();
                return securityLevel == ConnectionSecurityLevel.SECURITY_ERROR
                        || securityLevel == ConnectionSecurityLevel.SECURITY_WARNING
                        || securityLevel == ConnectionSecurityLevel.SECURITY_POLICY_WARNING;
            }
        });

        removeWindowBackground();

        if (mDocumentTab != null) {
            BandwidthReductionPreferences.launchDataReductionSSLInfoBar(
                    DocumentActivity.this, mDocumentTab.getWebContents());
        }
    }

    private void resetThemeColorAndIcon() {
        mThemeColor = null;
        mIcon = null;
        updateTaskDescription();
    }

    private void updateLastTabId() {
        ChromeMobileApplication.getDocumentTabModelSelector().selectModel(isIncognito());
        int tabId = mDocumentTab == null
                 ? ActivityDelegate.getTabIdFromIntent(getIntent()) : mDocumentTab.getId();
        mTabModel.setLastShownId(tabId);
    }

    @Override
    public boolean handleBackPressed() {
        if (mDocumentTab == null) return false;

        View view = ContentVideoView.getContentVideoView();
        if (view != null && view.getContext() == this) {
            ContentVideoView.getContentVideoView().exitFullscreen(false);
            return true;
        }

        if (getFullscreenManager().getPersistentFullscreenMode()) {
            getFullscreenManager().setPersistentFullscreenMode(false);
            return true;
        }

        if (mDocumentTab.canGoBack()) {
            mDocumentTab.goBack();
        } else if (!mDocumentTab.shouldPreserve()) {
            finishAndRemoveTask();
        } else {
            moveTaskToBack(true);
        }
        return true;
    }

    @Override
    public boolean createContextualSearchTab(ContentViewCore searchContentViewCore) {
        NavigationEntry entry =
                searchContentViewCore.getWebContents().getNavigationController().getPendingEntry();
        String url = entry != null
                ? entry.getUrl() : searchContentViewCore.getWebContents().getUrl();
        PendingDocumentData documentData = new PendingDocumentData();
        ChromeLauncherActivity.launchDocumentInstance(this, false,
                ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, url,
                DocumentMetricIds.STARTED_BY_CONTEXTUAL_SEARCH,
                PageTransition.LINK, documentData);
        return false;
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
            if (!EnhancedBookmarkUtils.showEnhancedBookmarkIfEnabled(this)) {
                NewTabPage.launchBookmarksDialog(this, mDocumentTab, getTabModelSelector());
            }
            RecordUserAction.record("MobileMenuAllBookmarks");
        } else if (id == R.id.recent_tabs_menu_id) {
            NewTabPage.launchRecentTabsDialog(this, mDocumentTab, false);
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
        } else if (id == R.id.show_menu) {
            if (mToolbarManager.isInitialized()) {
                mAppMenuHandler.showAppMenu(mToolbarManager.getMenuAnchor(), true,
                        false);
            }
        } else if (id == R.id.focus_url_bar) {
            if (mToolbarManager.isInitialized()) mToolbarManager.setUrlBarFocus(true);
        } else {
            return super.onMenuOrKeyboardAction(id, fromMenu);
        }
        return true;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(event, this,
                mToolbarManager.isInitialized());
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!mToolbarManager.isInitialized()) return false;
        return KeyboardShortcuts.onKeyDown(event, this, true, false)
                || super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean shouldShowAppMenu() {
        if (mDocumentTab == null || !mToolbarManager.isInitialized()) {
            return false;
        }

        return super.shouldShowAppMenu();
    }

    private void updateTaskDescription() {
        if (mDocumentTab == null) {
            updateTaskDescription(null, null);
            return;
        }

        if (isNewTabPage() && !isIncognito()) {
            // NTP needs a new color in recents, but uses the default application title and icon;
            updateTaskDescription(null, null);
            return;
        }

        String label = mDocumentTab.getTitle();
        String domain = UrlUtilities.getDomainAndRegistry(mDocumentTab.getUrl(), false);
        if (TextUtils.isEmpty(label)) {
            label = domain;
        }
        if (mIcon == null && TextUtils.isEmpty(label)) {
            updateTaskDescription(null, null);
            return;
        }

        if (isIncognito()) {
            mIcon = null;
        } else if (mIcon == null) {
            if (mDocumentAppIconGenerator == null) {
                mDocumentAppIconGenerator = new RoundedIconGenerator(this,
                        APP_ICON_SIZE_DP, APP_ICON_SIZE_DP,
                        APP_ICON_CORNER_RADIUS_DP, APP_ICON_DEFAULT_BACKGROUND_COLOR,
                        APP_ICON_TEXT_SIZE_DP);
            }
            mIcon = mDocumentAppIconGenerator.generateIconForUrl(mDocumentTab.getUrl());
            mIsUsingGeneratedIcon = true;
        }

        updateTaskDescription(label, mIcon);
    }

    protected int getThemeColor() {
        if (isIncognito()) {
            return mDefaultThemeColor;
        } else {
            return mThemeColor != null ? mThemeColor.intValue() : mDefaultThemeColor;
        }
    }

    private boolean shouldUseDefaultStatusBarColor() {
        return isIncognito() || mThemeColor == null || mThemeColor == mDefaultThemeColor;
    }

    protected void updateTaskDescription(String label, Bitmap icon) {
        int color = getThemeColor();
        DocumentUtils.updateTaskDescription(this, label, icon, color,
                shouldUseDefaultStatusBarColor());
        mToolbarManager.updatePrimaryColor(color);

        ControlContainer controlContainer =
                (ControlContainer) findViewById(R.id.control_container);
        controlContainer.getToolbarResourceAdapter().invalidate(null);
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
        if (mDocumentTab == null) {
            // If the Tab hasn't been created yet, then we're really early in initialization.
            // Use a combination of the original URL from the Intent and whether or not the Tab is
            // retargetable to know whether or not the user navigated away from the NTP.
            // If the Entry doesn't exist, then the DocumentActivity never got a chance to add
            // itself to the TabList and is likely to be retargetable.
            int tabId = ActivityDelegate.getTabIdFromIntent(getIntent());
            url = IntentHandler.getUrlFromIntent(getIntent());
            if (mTabModel.hasEntryForTabId(tabId) && !mTabModel.isRetargetable(tabId)) return false;
        } else {
            url = mDocumentTab.getUrl();
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
                || TextUtils.equals(className, DocumentActivity.class.getName());
    }

    /**
     * Launch a new DocumentActivity showing the new tab page.
     * @param incognito Whether the new NTP should be in incognito mode.
     */
    private void launchNtp(boolean incognito) {
        if (incognito && !PrefServiceBridge.getInstance().isIncognitoModeEnabled()) return;
        ChromeLauncherActivity.launchDocumentInstance(this, incognito,
                ChromeLauncherActivity.LAUNCH_MODE_RETARGET, UrlConstants.NTP_URL,
                DocumentMetricIds.STARTED_BY_OPTIONS_MENU, PageTransition.AUTO_TOPLEVEL, null);
    }
}
