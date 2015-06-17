// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BookmarksBridge;
import org.chromium.chrome.browser.ChromeBrowserProviderClient;
import org.chromium.chrome.browser.CustomSelectionActionModeCallback;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager.HomepageStateListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrl;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.chrome.browser.tab.ChromeTab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

import java.util.List;

/**
 * Contains logic for managing the toolbar visual component.  This class manages the interactions
 * with the rest of the application to ensure the toolbar is always visually up to date.
 */
class ToolbarManager implements ToolbarTabController, UrlFocusChangeListener {

    /**
     * Handle UI updates of menu icons. Only applicable for phones.
     */
    public interface MenuDelegatePhone {

        /**
         * Called when current tab's loading status changes.
         *
         * @param isLoading Whether the current tab is loading.
         */
        public void updateReloadButtonState(boolean isLoading);
    }

    /**
     * The minimum load progress that can be shown when a page is loading.  This is not 0 so that
     * it's obvious to the user that something is attempting to load.
     */
    public static final int MINIMUM_LOAD_PROGRESS = 5;

    private final ToolbarLayout mToolbar;
    private final ToolbarControlContainer mControlContainer;

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabModelObserver mTabModelObserver;
    private MenuDelegatePhone mMenuDelegatePhone;
    private final ToolbarModelImpl mToolbarModel;
    private Profile mCurrentProfile;
    private BookmarksBridge mBookmarksBridge;
    private TemplateUrlServiceObserver mTemplateUrlObserver;
    private final LocationBar mLocationBar;
    private FindToolbarManager mFindToolbarManager;

    private final TabObserver mTabObserver;
    private final BookmarksBridge.BookmarkModelObserver mBookmarksObserver;
    private final FindToolbarObserver mFindToolbarObserver;
    private final OverviewModeObserver mOverviewModeObserver;
    private final SceneChangeObserver mSceneChangeObserver;

    private final LoadProgressSimulator mLoadProgressSimulator;

    private ChromeFullscreenManager mFullscreenManager;
    private int mFullscreenFocusToken = FullscreenManager.INVALID_TOKEN;
    private int mFullscreenFindInPageToken = FullscreenManager.INVALID_TOKEN;
    private int mFullscreenMenuToken = FullscreenManager.INVALID_TOKEN;

    private int mPreselectedTabId = Tab.INVALID_TAB_ID;

    private boolean mNativeLibraryReady;
    private boolean mTabRestoreCompleted;

    private AppMenuButtonHelper mAppMenuButtonHelper;

    private HomepageStateListener mHomepageStateListener;

    /**
     * Creates a ToolbarManager object.
     * @param controlContainer The container of the toolbar.
     * @param menuHandler The handler for interacting with the menu.
     */
    ToolbarManager(ToolbarControlContainer controlContainer, AppMenuHandler menuHandler) {
        mToolbarModel = new ToolbarModelImpl();
        mControlContainer = controlContainer;
        assert mControlContainer != null;

        mToolbar = (ToolbarLayout) controlContainer.findViewById(R.id.toolbar);
        mLocationBar = mToolbar.getLocationBar();
        mLocationBar.setToolbarDataProvider(mToolbarModel);
        mLocationBar.setUrlFocusChangeListener(this);

        setMenuHandler(menuHandler);
        mToolbar.initialize(mToolbarModel, this, mAppMenuButtonHelper);

        mHomepageStateListener = new HomepageStateListener() {
            @Override
            public void onHomepageStateUpdated() {
                mToolbar.onHomeButtonUpdate(
                        HomepageManager.isHomepageEnabled(mToolbar.getContext()));
            }
        };
        HomepageManager.getInstance(mToolbar.getContext()).addListener(mHomepageStateListener);

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                refreshSelectedTab();
                updateTabCount();
                mControlContainer.invalidateBitmap();
            }

            @Override
            public void onTabStateInitialized() {
                mTabRestoreCompleted = true;
                handleTabRestoreCompleted();
            }
        };

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, TabLaunchType type) {
                updateTabCount();
            }

            @Override
            public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
                mPreselectedTabId = Tab.INVALID_TAB_ID;
                refreshSelectedTab();
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateTabCount();
                refreshSelectedTab();
            }

            @Override
            public void didCloseTab(Tab tab) {
                updateTabCount();
                refreshSelectedTab();
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                updateTabCount();
                refreshSelectedTab();
            }

            @Override
            public void allTabsPendingClosure(List<Integer> tabIds) {
                updateTabCount();
                refreshSelectedTab();
            }
        };

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onSSLStateUpdated(Tab tab) {
                super.onSSLStateUpdated(tab);
                assert tab == mToolbarModel.getTab();
                mLocationBar.updateSecurityIcon(tab.getSecurityLevel());
            }

            @Override
            public void onWebContentsInstantSupportDisabled() {
                mLocationBar.setUrlToPageUrl();
            }

            @Override
            public void onDidNavigateMainFrame(Tab tab, String url, String baseUrl,
                    boolean isNavigationToDifferentPage, boolean isFragmentNavigation,
                    int statusCode) {
                if (isNavigationToDifferentPage) {
                    mToolbar.onNavigatedToDifferentPage();
                }
            }

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                updateButtonStatus();
                updateTabLoadingState(true, true);
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                ToolbarManager.this.onPageLoadFinished();
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                ToolbarManager.this.onPageLoadFailed();
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                mLocationBar.setTitleToPageTitle();
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                // Update the SSL security state as a result of this notification as it will
                // sometimes be the only update we receive.
                updateTabLoadingState(false, true);

                // A URL update is a decent enough indicator that the toolbar widget is in
                // a stable state to capture its bitmap for use in fullscreen.
                mControlContainer.setReadyForBitmapCapture(true);
            }

            @Override
            public void onCrash(Tab tab, boolean sadTabShown) {
                onTabCrash();
            }

            @Override
            public void onLoadProgressChanged(Tab tab, int progress) {
                updateLoadProgress(progress);
            }

            @Override
            public void onContentChanged(Tab tab) {
                mToolbar.onTabContentViewChanged();
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                if (!didStartLoad) return;

                ChromeTab chromeTab = ChromeTab.fromTab(tab);
                if (!chromeTab.getBackgroundContentViewHelper().isPageSwappingInProgress()
                        && didFinishLoad) {
                    mLoadProgressSimulator.start();
                }
            }

            @Override
            public void onDidStartNavigationToPendingEntry(Tab tab, String url) {
                // Update URL as soon as it becomes available when it's a new tab.
                // But we want to update only when it's a new tab. So we check whether the current
                // navigation entry is initial, meaning whether it has the same target URL as the
                // initial URL of the tab.
                WebContents webContents = tab.getWebContents();
                if (webContents == null) return;
                NavigationController navigationController = webContents.getNavigationController();
                if (navigationController == null) return;
                if (navigationController.isInitialNavigation()) {
                    mLocationBar.setUrlToPageUrl();
                }
            }

            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                NewTabPage ntp = mToolbarModel.getNewTabPageForCurrentTab();
                if (ntp == null) return;
                if (!NewTabPage.isNTPUrl(params.getUrl())
                        && loadType != TabLoadStatus.PAGE_LOAD_FAILED) {
                    ntp.setUrlFocusAnimationsDisabled(true);
                    mToolbar.onTabOrModelChanged();
                }
            }

            @Override
            public void onDidFailLoad(Tab tab, boolean isProvisionalLoad, boolean isMainFrame,
                    int errorCode, String description, String failingUrl) {
                NewTabPage ntp = mToolbarModel.getNewTabPageForCurrentTab();
                if (ntp == null) return;
                if (isProvisionalLoad && isMainFrame) {
                    ntp.setUrlFocusAnimationsDisabled(false);
                    mToolbar.onTabOrModelChanged();
                }
            }
        };

        mBookmarksObserver = new BookmarksBridge.BookmarkModelObserver() {
            @Override
            public void bookmarkModelChanged() {
                updateBookmarkButtonStatus();
            }
        };

        mFindToolbarObserver = new FindToolbarObserver() {
            @Override
            public void onFindToolbarShown() {
                mToolbar.handleFindToolbarStateChange(true);
                if (mFullscreenManager != null) {
                    mFullscreenFindInPageToken =
                            mFullscreenManager.showControlsPersistentAndClearOldToken(
                                    mFullscreenFindInPageToken);
                }
            }

            @Override
            public void onFindToolbarHidden() {
                mToolbar.handleFindToolbarStateChange(false);
                if (mFullscreenManager != null) {
                    mFullscreenManager.hideControlsPersistent(mFullscreenFindInPageToken);
                    mFullscreenFindInPageToken = FullscreenManager.INVALID_TOKEN;
                }
            }
        };

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mToolbar.setTabSwitcherMode(true, showToolbar, false);
                updateButtonStatus();
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mToolbar.setTabSwitcherMode(false, showToolbar, delayAnimation);
                updateButtonStatus();
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                mToolbar.onTabSwitcherTransitionFinished();
            }
        };

        mSceneChangeObserver = new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {
                mPreselectedTabId = tabId;
                refreshSelectedTab();
            }

            @Override
            public void onSceneChange(Layout layout) {
                mToolbar.setContentAttached(layout.shouldDisplayContentOverlay());
            }
        };

        mLoadProgressSimulator = new LoadProgressSimulator(this);
    }

    /**
     * Initialize the manager with the components that had native initialization dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     *
     * @param tabModelSelector     The selector that handles tab management.
     * @param fullscreenManager    The manager in charge of interacting with the fullscreen feature.
     * @param findToolbarManager   The manager for find in page.
     * @param overviewModeBehavior The overview mode manager.
     * @param layoutDriver         A {@link LayoutManager} instance used to watch for scene changes.
     */
    public void initializeWithNative(TabModelSelector tabModelSelector,
            ChromeFullscreenManager fullscreenManager,
            final FindToolbarManager findToolbarManager,
            final OverviewModeBehavior overviewModeBehavior,
            final LayoutManager layoutDriver) {
        mTabModelSelector = tabModelSelector;
        mToolbarModel.initializeWithNative();
        mToolbar.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewDetachedFromWindow(View v) {
                Context context = mToolbar.getContext();
                HomepageManager.getInstance(context).removeListener(mHomepageStateListener);
                mTabModelSelector.removeObserver(mTabModelSelectorObserver);
                for (TabModel model : mTabModelSelector.getModels()) {
                    model.removeObserver(mTabModelObserver);
                }
                if (mBookmarksBridge != null) {
                    mBookmarksBridge.destroy();
                    mBookmarksBridge = null;
                }
                if (mTemplateUrlObserver != null) {
                    TemplateUrlService.getInstance().removeObserver(mTemplateUrlObserver);
                    mTemplateUrlObserver = null;
                }

                findToolbarManager.removeObserver(mFindToolbarObserver);
                if (overviewModeBehavior != null) {
                    overviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
                }
                if (layoutDriver != null) {
                    layoutDriver.removeSceneChangeObserver(mSceneChangeObserver);
                }
            }

            @Override
            public void onViewAttachedToWindow(View v) {
                // As we have only just registered for notifications, any that were sent prior to
                // this may have been missed.
                // Calling refreshSelectedTab in case we missed the initial selection notification.
                refreshSelectedTab();
            }
        });

        mFindToolbarManager = findToolbarManager;

        assert fullscreenManager != null;
        mFullscreenManager = fullscreenManager;

        mNativeLibraryReady = false;

        findToolbarManager.addObserver(mFindToolbarObserver);
        if (overviewModeBehavior != null) {
            overviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        }
        if (layoutDriver != null) layoutDriver.addSceneChangeObserver(mSceneChangeObserver);

        onNativeLibraryReady();
    }

    /**
     * @return The toolbar interface that this manager handles.
     */
    public Toolbar getToolbar() {
        return mToolbar;
    }

    /**
     * Called when the accessibility enabled state changes.
     * @param enabled Whether accessibility is enabled.
     */
    public void onAccessibilityStatusChanged(boolean enabled) {
        mToolbar.onAccessibilityStatusChanged(enabled);
    }

    private void registerTemplateUrlObserver() {
        final TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
        assert mTemplateUrlObserver == null;
        mTemplateUrlObserver = new TemplateUrlServiceObserver() {
            private TemplateUrl mSearchEngine =
                    templateUrlService.getDefaultSearchEngineTemplateUrl();

            @Override
            public void onTemplateURLServiceChanged() {
                TemplateUrl searchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
                if ((mSearchEngine == null && searchEngine == null)
                        || (mSearchEngine != null && mSearchEngine.equals(searchEngine))) {
                    return;
                }

                mSearchEngine = searchEngine;
                mToolbar.onDefaultSearchEngineChanged();
            }
        };
        templateUrlService.addObserver(mTemplateUrlObserver);
    }

    private void onNativeLibraryReady() {
        mNativeLibraryReady = true;
        mToolbar.onNativeLibraryReady();

        final TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
        TemplateUrlService.LoadListener mTemplateServiceLoadListener =
                new TemplateUrlService.LoadListener() {
                    @Override
                    public void onTemplateUrlServiceLoaded() {
                        registerTemplateUrlObserver();
                        templateUrlService.unregisterLoadListener(this);
                    }
                };
        templateUrlService.registerLoadListener(mTemplateServiceLoadListener);
        if (templateUrlService.isLoaded()) {
            mTemplateServiceLoadListener.onTemplateUrlServiceLoaded();
        } else {
            templateUrlService.load();
        }

        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        for (TabModel model : mTabModelSelector.getModels()) model.addObserver(mTabModelObserver);

        refreshSelectedTab();
        if (mTabModelSelector.isTabStateInitialized()) mTabRestoreCompleted = true;
        handleTabRestoreCompleted();
    }

    private void handleTabRestoreCompleted() {
        if (!mTabRestoreCompleted || !mNativeLibraryReady) return;
        mToolbar.onStateRestored();
        updateTabCount();
    }

    /**
     * Sets the handler for any special case handling related with the menu button.
     * @param menuHandler The handler to be used.
     */
    private void setMenuHandler(AppMenuHandler menuHandler) {
        menuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (mFullscreenManager == null) return;
                if (isVisible) {
                    mFullscreenMenuToken =
                            mFullscreenManager.showControlsPersistentAndClearOldToken(
                                    mFullscreenMenuToken);
                } else {
                    mFullscreenManager.hideControlsPersistent(mFullscreenMenuToken);
                    mFullscreenMenuToken = FullscreenManager.INVALID_TOKEN;
                }
            }
        });
        mAppMenuButtonHelper = new AppMenuButtonHelper(menuHandler);
        mAppMenuButtonHelper.setOnAppMenuShownListener(new Runnable() {
            @Override
            public void run() {
                RecordUserAction.record("MobileToolbarShowMenu");
            }
        });
        mLocationBar.setMenuButtonHelper(mAppMenuButtonHelper);
    }

    /**
     * Set the delegate that will handle updates from toolbar driven state changes.
     * @param menuDelegatePhone The menu delegate to be updated (only applicable to phones).
     */
    public void setMenuDelegatePhone(MenuDelegatePhone menuDelegatePhone) {
        mMenuDelegatePhone = menuDelegatePhone;
    }

    /**
     * Sets a custom ActionMode.Callback instance to the LocationBar's UrlBar.  This lets us
     * get notified when the user tries to do copy, paste, etc. on the UrlBar.
     * @param callback The ActionMode.Callback instance to be notified when selection ActionMode
     * is triggered.
     */
    public void setDefaultActionModeCallbackForTextEdit(
            CustomSelectionActionModeCallback callback) {
        mLocationBar.setDefaultTextEditActionModeCallback(callback);
    }

    @Override
    public boolean back() {
        Tab tab = mToolbarModel.getTab();
        if (tab != null && tab.canGoBack()) {
            tab.goBack();
            updateButtonStatus();
            return true;
        }
        return false;
    }

    @Override
    public boolean forward() {
        Tab tab = mToolbarModel.getTab();
        if (tab != null && tab.canGoForward()) {
            tab.goForward();
            updateButtonStatus();
            return true;
        }
        return false;
    }

    @Override
    public void stopOrReloadCurrentTab() {
        Tab currentTab = mToolbarModel.getTab();
        if (currentTab != null) {
            if (currentTab.isLoading()) {
                currentTab.stopLoading();
            } else {
                currentTab.reload();
                RecordUserAction.record("MobileToolbarReload");
            }
        }
        updateButtonStatus();
    }

    @Override
    public void openHomepage() {
        Tab currentTab = mToolbarModel.getTab();
        assert currentTab != null;
        Context context = mToolbar.getContext();
        String homePageUrl = HomepageManager.getHomepageUri(context);
        if (TextUtils.isEmpty(homePageUrl)) {
            homePageUrl = UrlConstants.NTP_URL;
        }
        currentTab.loadUrl(new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE));
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mToolbar.onUrlFocusChange(hasFocus);

        if (mFindToolbarManager != null && hasFocus) mFindToolbarManager.hideToolbar();

        if (mFullscreenManager == null) return;
        if (hasFocus) {
            mFullscreenFocusToken = mFullscreenManager.showControlsPersistentAndClearOldToken(
                    mFullscreenFocusToken);
        } else {
            mFullscreenManager.hideControlsPersistent(mFullscreenFocusToken);
            mFullscreenFocusToken = FullscreenManager.INVALID_TOKEN;
        }
    }

    /**
     * Update the primary color used by the model to the given color.
     * @param color The primary color for the current tab.
     */
    public void updatePrimaryColor(int color) {
        boolean colorChanged = mToolbarModel.getPrimaryColor() != color;
        if (!colorChanged) return;

        mToolbarModel.setPrimaryColor(color);
        mToolbar.onPrimaryColorChanged();
    }

    /**
     * Updates the current number of Tabs based on the TabModel this Toolbar contains.
     */
    private void updateTabCount() {
        if (!mTabRestoreCompleted) return;
        mToolbar.updateTabCountVisuals(mTabModelSelector.getCurrentModel().getCount());
    }

    /**
     * Updates the current button states and calls appropriate abstract visibility methods, giving
     * inheriting classes the chance to update the button visuals as well.
     */
    private void updateButtonStatus() {
        Tab currentTab = mToolbarModel.getTab();
        boolean tabCrashed = currentTab != null && currentTab.isShowingSadTab();

        mToolbar.updateBackButtonVisibility(currentTab != null && currentTab.canGoBack());
        mToolbar.updateForwardButtonVisibility(currentTab != null && currentTab.canGoForward());
        updateReloadState(tabCrashed);
        updateBookmarkButtonStatus();

        mToolbar.getMenuButton().setVisibility(
                mToolbar.shouldShowMenuButton() ? View.VISIBLE : View.GONE);
    }

    private void updateBookmarkButtonStatus() {
        Tab currentTab = mToolbarModel.getTab();
        boolean isBookmarked = currentTab != null
                && currentTab.getBookmarkId() != ChromeBrowserProviderClient.INVALID_BOOKMARK_ID;
        mToolbar.updateBookmarkButtonVisibility(isBookmarked);
    }

    private void updateReloadState(boolean tabCrashed) {
        Tab currentTab = mToolbarModel.getTab();
        boolean isLoading = false;
        if (!tabCrashed) {
            isLoading = (currentTab != null && currentTab.isLoading()) || !mNativeLibraryReady;
        }
        mToolbar.updateReloadButtonVisibility(isLoading);
        if (mMenuDelegatePhone != null) mMenuDelegatePhone.updateReloadButtonState(isLoading);
    }

    /**
     * Triggered when the selected tab has changed.
     */
    private void refreshSelectedTab() {
        ChromeTab tab = null;
        if (mPreselectedTabId != Tab.INVALID_TAB_ID) {
            tab = ChromeTab.fromTab(mTabModelSelector.getTabById(mPreselectedTabId));
        }
        if (tab == null) tab = ChromeTab.fromTab(mTabModelSelector.getCurrentTab());

        boolean wasIncognito = mToolbarModel.isIncognito();
        ChromeTab previousTab = ChromeTab.fromTab(mToolbarModel.getTab());

        boolean isIncognito =
                tab != null ? tab.isIncognito() : mTabModelSelector.isIncognitoSelected();
        mToolbarModel.setTab(tab, isIncognito);

        updateCurrentTabDisplayStatus();
        if (previousTab != tab || wasIncognito != isIncognito) {
            if (previousTab != tab) {
                if (previousTab != null) previousTab.removeObserver(mTabObserver);
                if (tab != null) tab.addObserver(mTabObserver);
            }
            int defaultPrimaryColor = isIncognito
                    ? mToolbar.getResources().getColor(R.color.incognito_primary_color)
                    : mToolbar.getResources().getColor(R.color.default_primary_color);
            int primaryColor = (tab != null && tab.getWebContents() != null)
                    ? tab.getWebContents().getThemeColor(defaultPrimaryColor) : defaultPrimaryColor;
            updatePrimaryColor(primaryColor);

            mToolbar.onTabOrModelChanged();

            if (tab != null && tab.getWebContents() != null
                    && tab.getWebContents().isLoadingToDifferentDocument()) {
                mToolbar.onNavigatedToDifferentPage();
            }
        }

        Profile profile = mTabModelSelector.getModel(isIncognito).getProfile();
        if (mCurrentProfile != profile) {
            if (mBookmarksBridge != null) mBookmarksBridge.destroy();
            mBookmarksBridge = new BookmarksBridge(profile);
            mBookmarksBridge.addObserver(mBookmarksObserver);
            mLocationBar.setAutocompleteProfile(profile);
            mCurrentProfile = profile;
        }
    }

    private void updateCurrentTabDisplayStatus() {
        Tab currentTab = mToolbarModel.getTab();
        mLocationBar.setUrlToPageUrl();
        if (currentTab == null) {
            updateLoadProgressInternal(0);
            updateButtonStatus();
            return;
        }
        boolean isLoading = currentTab.isLoading();
        updateTabLoadingState(isLoading, true);

        if (currentTab.getProgress() == 100 || currentTab.isShowingInterstitialPage()) {
            // We are switching to a tab that is fully loaded. Don't set the load progress to 1.0,
            // that would cause the load progress bar to show briefly.
            updateLoadProgressInternal(0);
        } else {
            updateLoadProgress(currentTab.getProgress());
        }
        updateButtonStatus();
    }

    private void onTabCrash() {
        updateTabLoadingState(false, false);
        updateLoadProgressInternal(0);
        updateButtonStatus();
    }

    private void onPageLoadFinished() {
        Tab currentTab = mToolbarModel.getTab();
        updateTabLoadingState(false, true);
        int currentProgress = currentTab.getProgress();
        if (currentProgress != 100) {
            // If we made some progress, fast-forward to complete, otherwise just dismiss any
            // MINIMUM_LOAD_PROGRESS that had been set.
            if (currentProgress > MINIMUM_LOAD_PROGRESS) {
                updateLoadProgress(100);
            } else {
                updateLoadProgressInternal(0);
            }
        }
        updateButtonStatus();
    }

    private void onPageLoadFailed() {
        updateTabLoadingState(false, true);
        updateButtonStatus();
        updateLoadProgressInternal(0);
    }

    private void updateTabLoadingState(boolean isLoading, boolean updateUrl) {
        Tab currentTab = mToolbarModel.getTab();
        mLocationBar.updateLoadingState(updateUrl);
        if (isLoading) updateLoadProgress(currentTab.getProgress());
        if (updateUrl) updateButtonStatus();
    }

    private void updateLoadProgressInternal(int progress) {
        if (progress == mToolbarModel.getLoadProgress()) return;
        mToolbarModel.setLoadProgress(progress);
        mToolbar.setLoadProgress(progress);
        if (progress == 0) mLoadProgressSimulator.cancel();
    }

    private void updateLoadProgress(int progress) {
        mLoadProgressSimulator.cancel();
        progress = Math.max(progress, MINIMUM_LOAD_PROGRESS);
        Tab tab = mToolbarModel.getTab();
        if (tab != null
                && NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) {
            progress = 0;
        }
        updateLoadProgressInternal(progress);
        if (progress == 100 || progress == 0) {
            updateButtonStatus();
        } else {
            // Update the reload state regardless or whether or not the progress is 100.
            updateReloadState(false);
        }
    }

    private static class LoadProgressSimulator {
        private static final int MSG_ID_UPDATE_PROGRESS = 1;

        private static final int PROGRESS_INCREMENT = 10;
        private static final int PROGRESS_INCREMENT_DELAY_MS = 10;

        private final ToolbarManager mToolbar;
        private final Handler mHandler;

        private int mProgress;

        public LoadProgressSimulator(ToolbarManager toolbar) {
            mToolbar = toolbar;
            mHandler = new Handler(Looper.getMainLooper()) {
                @Override
                public void handleMessage(Message msg) {
                    assert msg.what == MSG_ID_UPDATE_PROGRESS;
                    mProgress = Math.min(100, mProgress += PROGRESS_INCREMENT);
                    mToolbar.updateLoadProgressInternal(mProgress);

                    if (mProgress >= 100) return;
                    sendEmptyMessageDelayed(MSG_ID_UPDATE_PROGRESS, PROGRESS_INCREMENT_DELAY_MS);
                }
            };
        }

        /**
         * Start simulating load progress from a baseline of 0.
         */
        public void start() {
            mProgress = 0;
            mHandler.sendEmptyMessage(MSG_ID_UPDATE_PROGRESS);
        }

        /**
         * Cancels simulating load progress.
         */
        public void cancel() {
            mHandler.removeMessages(MSG_ID_UPDATE_PROGRESS);
        }
    }
}
