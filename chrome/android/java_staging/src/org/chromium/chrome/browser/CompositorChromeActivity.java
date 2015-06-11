// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.support.v7.app.AlertDialog;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;

import com.google.android.apps.chrome.R;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.bookmark.ManageBookmarkActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerDocument;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager.ContextualSearchTabPromotionDelegate;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.dom_distiller.DistilledPagePrefsView;
import org.chromium.chrome.browser.dom_distiller.ReaderModeActivityDelegate;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksModel;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkUtils;
import org.chromium.chrome.browser.feedback.FeedbackCollector;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.browser.snackbar.LoFiBarPopupController;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.webapps.AddToHomescreenDialog;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.content.browser.ContentReadbackHandler;
import org.chromium.content.browser.ContentReadbackHandler.GetBitmapCallback;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.readback_types.ReadbackResponse;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * A {@link ChromeActivity} that builds and manages a {@link CompositorViewHolder} and associated
 * classes.
 */
public abstract class CompositorChromeActivity extends ChromeActivity
        implements ContextualSearchTabPromotionDelegate, SnackbarManageable, SceneChangeObserver {
    /**
     * No control container to inflate during initialization.
     */
    private static final int NO_CONTROL_CONTAINER = -1;

    private static final String TAG = "CompositorChromeActivity";

    private ActivityWindowAndroid mWindowAndroid;
    private ChromeFullscreenManager mFullscreenManager;
    private CompositorViewHolder mCompositorViewHolder;
    private ContextualSearchManager mContextualSearchManager;
    private ReaderModeActivityDelegate mReaderModeActivityDelegate;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private SnackbarManager mSnackbarManager;
    private LoFiBarPopupController mLoFiBarPopupController;

    // Time in ms that it took took us to inflate the initial layout
    private long mInflateInitialLayoutDurationMs;

    private OnPreDrawListener mFirstDrawListener;

    private final Locale mCurrentLocale = Locale.getDefault();

    @Override
    public void postInflationStartup() {
        mWindowAndroid = new ChromeWindow(this);
        mWindowAndroid.restoreInstanceState(getSavedInstanceState());
        mSnackbarManager = new SnackbarManager(findViewById(android.R.id.content));
        mLoFiBarPopupController = new LoFiBarPopupController(this, getSnackbarManager());
        super.postInflationStartup();

        // Set up the animation placeholder to be the SurfaceView. This disables the
        // SurfaceView's 'hole' clipping during animations that are notified to the window.
        mWindowAndroid.setAnimationPlaceholderView(mCompositorViewHolder.getSurfaceView());

        // Inform the WindowAndroid of the keyboard accessory view.
        mWindowAndroid.setKeyboardAccessoryView((ViewGroup) findViewById(R.id.keyboard_accessory));
        final View controlContainer = findViewById(R.id.control_container);
        if (controlContainer != null) {
            mFirstDrawListener = new ViewTreeObserver.OnPreDrawListener() {
                @Override
                public boolean onPreDraw() {
                    controlContainer.getViewTreeObserver()
                            .removeOnPreDrawListener(mFirstDrawListener);
                    mFirstDrawListener = null;
                    onFirstDrawComplete();
                    return true;
                }
            };
            controlContainer.getViewTreeObserver().addOnPreDrawListener(mFirstDrawListener);
        }
    }

    /**
     * This function builds the {@link CompositorViewHolder}.  Subclasses *must* call
     * super.setContentView() before using {@link #getTabModelSelector()} or
     * {@link #getCompositorViewHolder()}.
     */
    @Override
    protected final void setContentView() {
        final long begin = SystemClock.elapsedRealtime();
        TraceEvent.begin("onCreate->setContentView()");
        if (WarmupManager.getInstance().hasBuiltViewHierarchy()) {
            View placeHolderView = new View(this);
            setContentView(placeHolderView);
            ViewGroup contentParent = (ViewGroup) placeHolderView.getParent();
            WarmupManager.getInstance().transferViewHierarchyTo(contentParent);
            contentParent.removeView(placeHolderView);
        } else {
            setContentView(R.layout.main);
            if (getControlContainerLayoutId() != NO_CONTROL_CONTAINER) {
                ViewStub toolbarContainerStub =
                        ((ViewStub) findViewById(R.id.control_container_stub));
                toolbarContainerStub.setLayoutResource(getControlContainerLayoutId());
                toolbarContainerStub.inflate();
            }
        }
        TraceEvent.end("onCreate->setContentView()");
        mInflateInitialLayoutDurationMs = SystemClock.elapsedRealtime() - begin;

        // Set the status bar color to black by default. This is an optimization for
        // Chrome not to draw under status and navigation bars when we use the default
        // black status bar
        ApiCompatibilityUtils.setStatusBarColor(getWindow(), Color.BLACK);

        mCompositorViewHolder = (CompositorViewHolder) findViewById(R.id.compositor_view_holder);
        mCompositorViewHolder.setRootView(getWindow().getDecorView().getRootView());
    }

    /**
     * @return The resource id for the layout to use for {@link ControlContainer}. 0 by default.
     */
    protected int getControlContainerLayoutId() {
        return NO_CONTROL_CONTAINER;
    }

    /**
     * @return Whether contextual search is allowed for this activity or not.
     */
    protected boolean isContextualSearchAllowed() {
        return true;
    }

    @Override
    public void initializeCompositor() {
        TraceEvent.begin("CompositorChromeActivity:CompositorInitialization");
        super.initializeCompositor();

        setTabContentManager(new TabContentManager(this, getContentOffsetProvider(),
                DeviceClassManager.enableSnapshots()));
        mCompositorViewHolder.onNativeLibraryReady(mWindowAndroid, getTabContentManager());

        if (isContextualSearchAllowed() && ContextualSearchFieldTrial.isEnabled(this)) {
            mContextualSearchManager = new ContextualSearchManager(this, mWindowAndroid, this);
        }

        if (ReaderModeManager.isEnabled(this)) {
            mReaderModeActivityDelegate = new ReaderModeActivityDelegate(this);
        }

        TraceEvent.end("CompositorChromeActivity:CompositorInitialization");
    }

    @Override
    protected void setTabModelSelector(TabModelSelector tabModelSelector) {
        super.setTabModelSelector(tabModelSelector);

        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(tabModelSelector) {
            @Override
            public void onDidNavigateMainFrame(Tab tab, String url, String baseUrl,
                    boolean isNavigationToDifferentPage, boolean isFragmentNavigation,
                    int statusCode) {
                if (!tab.isNativePage()
                        && DataReductionProxySettings.getInstance().wasLoFiModeActiveOnMainFrame()
                        && DataReductionProxySettings.getInstance().canUseDataReductionProxy(
                                url)) {
                    mLoFiBarPopupController.showLoFiBar(tab);
                }
            }

            @Override
            public void onHidden(Tab tab) {
                mLoFiBarPopupController.dismissLoFiBar();
            }

            @Override
            public void onDestroyed(Tab tab) {
                mLoFiBarPopupController.dismissLoFiBar();
            }
        };
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        mCompositorViewHolder.resetFlags();
    }

    @Override
    protected void onDeferredStartup() {
        super.onDeferredStartup();
        RecordHistogram.recordTimesHistogram("MobileStartup.ToolbarInflationTime",
                mInflateInitialLayoutDurationMs, TimeUnit.MILLISECONDS);
    }

    @Override
    public void onStart() {
        super.onStart();
        if (mCompositorViewHolder != null) mCompositorViewHolder.onStart();
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mCompositorViewHolder != null) mCompositorViewHolder.onStop();
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mSnackbarManager != null) mSnackbarManager.dismissSnackbar(false);
    }

    @Override
    public long getOnCreateTimestampMs() {
        return super.getOnCreateTimestampMs();
    }

    /**
     * This cannot be overridden in order to preserve destruction order.  Override
     * {@link #onDestroyInternal()} instead to perform clean up tasks.
     */
    @Override
    protected final void onDestroy() {
        if (mReaderModeActivityDelegate != null) mReaderModeActivityDelegate.destroy();
        if (mContextualSearchManager != null) mContextualSearchManager.destroy();
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
        if (mCompositorViewHolder != null) {
            if (mCompositorViewHolder.getLayoutManager() != null) {
                mCompositorViewHolder.getLayoutManager().removeSceneChangeObserver(this);
            }
            mCompositorViewHolder.shutDown();
        }
        onDestroyInternal();

        TabModelSelector selector = getTabModelSelector();
        if (selector != null) selector.destroy();

        if (mWindowAndroid != null) mWindowAndroid.destroy();
        if (!Locale.getDefault().equals(mCurrentLocale)) {
            // This is a hack to relaunch renderer processes. Killing the main process also kills
            // its dependent (renderer) processes, and Android's activity manager service seems to
            // still relaunch the activity even when process dies in onDestroy().
            // This shouldn't be moved to ChromeActivity since it may cause a crash if
            // you kill the process from EmbedContentViewActivity since Preferences looks up
            // ChildAccountManager#hasChildAccount() when it is not set.
            // TODO(changwan): Implement a more generic and safe relaunch mechanism such as
            // killing dependent processes on onDestroy and launching them at onCreate().
            Log.w(TAG, "Forcefully killing process...");
            Process.killProcess(Process.myPid());
        }
        super.onDestroy();
    }

    /**
     * Override this to perform destruction tasks.  Note that by the time this is called, the
     * {@link CompositorViewHolder} will be destroyed, but the {@link WindowAndroid} and
     * {@link TabModelSelector} will not.
     * <p>
     * After returning from this, the {@link TabModelSelector} will be destroyed followed
     * by the {@link WindowAndroid}.
     */
    protected void onDestroyInternal() {
    }

    /**
     * This will handle passing {@link Intent} results back to the {@link WindowAndroid}.  It will
     * return whether or not the {@link WindowAndroid} has consumed the event or not.
     */
    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        if (super.onActivityResultWithNative(requestCode, resultCode, intent)) return true;
        return mWindowAndroid.onActivityResult(requestCode, resultCode, intent);
    }

    // @Override[ANDROID-M]
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
            int[] grantResults) {
        if (mWindowAndroid != null) {
            mWindowAndroid.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
        //super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mWindowAndroid.saveInstanceState(outState);
    }

    /**
     * @return The unified manager for all snackbar related operations.
     */
    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    /**
     * Add the specified tab to bookmarks or allows to edit the bookmark if the specified tab is
     * already bookmarked. If a new bookmark is added, a snackbar will be shown.
     * @param tabToBookmark The tab that needs to be bookmarked.
     */
    public void addOrEditBookmark(final Tab tabToBookmark) {
        if (tabToBookmark == null || tabToBookmark.isFrozen()) {
            return;
        }

        // Managed bookmarks can't be edited. If the current URL is only bookmarked by managed
        // bookmarks then fall back on adding a new bookmark instead.
        final long bookmarkId = tabToBookmark.getUserBookmarkId();

        if (EnhancedBookmarkUtils.isEnhancedBookmarkEnabled(tabToBookmark.getProfile())) {
            final EnhancedBookmarksModel bookmarkModel = new EnhancedBookmarksModel();

            BookmarkModelObserver modelObserver = new BookmarkModelObserver() {
                @Override
                public void bookmarkModelChanged() {}

                @Override
                public void bookmarkModelLoaded() {
                    if (bookmarkId == ChromeBrowserProviderClient.INVALID_BOOKMARK_ID) {
                        EnhancedBookmarkUtils.addBookmarkAndShowSnackbar(bookmarkModel,
                                tabToBookmark, getSnackbarManager(), CompositorChromeActivity.this);
                    } else {
                        EnhancedBookmarkUtils.startDetailActivity(CompositorChromeActivity.this,
                                new BookmarkId(bookmarkId, BookmarkType.NORMAL));
                    }
                    bookmarkModel.removeModelObserver(this);
                }
            };

            if (bookmarkModel.isBookmarkModelLoaded()) {
                modelObserver.bookmarkModelLoaded();
            } else {
                bookmarkModel.addModelObserver(modelObserver);
            }
        } else {
            Intent intent = new Intent(this, ManageBookmarkActivity.class);
            // Managed bookmarks can't be edited. Fallback on adding a new bookmark if the current
            // URL is bookmarked by a managed bookmark.

            if (bookmarkId == ChromeBrowserProviderClient.INVALID_BOOKMARK_ID) {
                intent.putExtra(ManageBookmarkActivity.BOOKMARK_INTENT_IS_FOLDER, false);
                intent.putExtra(ManageBookmarkActivity.BOOKMARK_INTENT_TITLE,
                        tabToBookmark.getTitle());
                intent.putExtra(ManageBookmarkActivity.BOOKMARK_INTENT_URL, tabToBookmark.getUrl());
            } else {
                intent.putExtra(ManageBookmarkActivity.BOOKMARK_INTENT_IS_FOLDER, false);
                intent.putExtra(ManageBookmarkActivity.BOOKMARK_INTENT_ID, bookmarkId);
            }
            startActivity(intent);
        }
    }

    /**
     * @return A {@link WindowAndroid} instance.
     */
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * @return A {@link CompositorViewHolder} instance.
     */
    public CompositorViewHolder getCompositorViewHolder() {
        return mCompositorViewHolder;
    }

    @Override
    public ChromeFullscreenManager getFullscreenManager() {
        return mFullscreenManager;
    }

    @Override
    public ContentOffsetProvider getContentOffsetProvider() {
        return mCompositorViewHolder.getContentOffsetProvider();
    }

    @Override
    public ContentReadbackHandler getContentReadbackHandler() {
        return mCompositorViewHolder.getContentReadbackHandler();
    }

    /**
     * Starts asynchronously taking the compositor activity screenshot.
     * @param getBitmapCallback The callback to call once the screenshot is taken, or when failed.
     */
    public void startTakingCompositorActivityScreenshot(GetBitmapCallback getBitmapCallback) {
        ContentReadbackHandler readbackHandler = getContentReadbackHandler();
        if (readbackHandler == null || getWindowAndroid() == null) {
            getBitmapCallback.onFinishGetBitmap(null, ReadbackResponse.SURFACE_UNAVAILABLE);
        } else {
            readbackHandler.getCompositorBitmapAsync(getWindowAndroid(), getBitmapCallback);
        }
    }

    @Override
    public ContextualSearchManager getContextualSearchManager() {
        return mContextualSearchManager;
    }

    /**
     * @return A {@link ReaderModeActivityDelegate} instance or {@code null} if reader mode is
     *         not enabled.
     */
    public ReaderModeActivityDelegate getReaderModeActivityDelegate() {
        return mReaderModeActivityDelegate;
    }

    /**
     * Create a full-screen manager to be used by this activity.
     * @param controlContainer The control container that will be controlled by the full-screen
     *                         manager.
     * @return A {@link ChromeFullscreenManager} instance that's been created.
     */
    protected ChromeFullscreenManager createFullscreenManager(View controlContainer) {
        return new ChromeFullscreenManager(this, controlContainer, getTabModelSelector(),
                getControlContainerHeightResource(), true);
    }

    /**
     * Initializes the {@link CompositorViewHolder} with the relevant content it needs to properly
     * show content on the screen.
     * @param layoutManager             A {@link LayoutManagerDocument} instance.  This class is
     *                                  responsible for driving all high level screen content and
     *                                  determines which {@link Layout} is shown when.
     * @param urlBar                    The {@link View} representing the URL bar (must be
     *                                  focusable) or {@code null} if none exists.
     * @param contentContainer          A {@link ViewGroup} that can have content attached by
     *                                  {@link Layout}s.
     * @param controlContainer          A {@link ControlContainer} instance to draw.
     */
    protected void initializeCompositorContent(
            LayoutManagerDocument layoutManager, View urlBar, ViewGroup contentContainer,
            ControlContainer controlContainer) {
        CommandLine commandLine = CommandLine.getInstance();
        boolean enableFullscreen = !commandLine.hasSwitch(ChromeSwitches.DISABLE_FULLSCREEN);

        if (enableFullscreen && controlContainer != null) {
            mFullscreenManager = createFullscreenManager((View) controlContainer);
        }

        if (mContextualSearchManager != null) {
            mContextualSearchManager.initialize(contentContainer);
            mContextualSearchManager.setSearchContentViewDelegate(layoutManager);
        }

        if (mReaderModeActivityDelegate != null) {
            mReaderModeActivityDelegate.initialize(contentContainer);
            mReaderModeActivityDelegate.setDynamicResourceLoader(
                    mCompositorViewHolder.getDynamicResourceLoader());
        }

        layoutManager.addSceneChangeObserver(this);
        mCompositorViewHolder.setLayoutManager(layoutManager);
        mCompositorViewHolder.setFocusable(false);
        mCompositorViewHolder.setControlContainer(controlContainer);
        mCompositorViewHolder.setFullscreenHandler(mFullscreenManager);
        mCompositorViewHolder.setUrlBar(urlBar);
        mCompositorViewHolder.onFinishNativeInitialization(getTabModelSelector(), this,
                getTabContentManager(), contentContainer, mContextualSearchManager);

        if (controlContainer != null
                && DeviceClassManager.enableToolbarSwipe(FeatureUtilities.isDocumentMode(this))) {
            controlContainer.setSwipeHandler(
                    getCompositorViewHolder().getLayoutManager().getTopSwipeHandler());
        }
    }

    /**
     * Called when the back button is pressed.
     * @return Whether or not the back button was handled.
     */
    protected abstract boolean handleBackPressed();

    @Override
    public void onOrientationChange(int orientation) {
        if (mContextualSearchManager != null) {
            mContextualSearchManager.onOrientationChange();
        }
    }

    @Override
    public final void onBackPressed() {
        if (mCompositorViewHolder != null) {
            LayoutManager layoutManager = mCompositorViewHolder.getLayoutManager();
            boolean layoutConsumed = layoutManager != null && layoutManager.onBackPressed();
            if (layoutConsumed || mContextualSearchManager != null
                    && mContextualSearchManager.onBackPressed()) {
                RecordUserAction.record("SystemBack");
                return;
            }
        }
        if (!isSelectActionBarShowing() && handleBackPressed()) {
            return;
        }
        // This will close the select action bar if it is showing, otherwise close the activity.
        super.onBackPressed();
    }

    private boolean isSelectActionBarShowing() {
        Tab tab = getActivityTab();
        if (tab == null) return false;
        ContentViewCore contentViewCore = tab.getContentViewCore();
        if (contentViewCore == null) return false;
        return contentViewCore.isSelectActionBarShowing();
    }

    @Override
    public boolean createContextualSearchTab(ContentViewCore searchContentViewCore) {
        Tab currentTab = getActivityTab();
        if (currentTab == null) return false;

        TabCreator tabCreator = getTabCreator(currentTab.isIncognito());
        if (tabCreator == null) return false;

        tabCreator.createTabWithWebContents(searchContentViewCore.getWebContents(),
                currentTab.getId(), TabLaunchType.FROM_LONGPRESS_FOREGROUND);
        return true;
    }

    @VisibleForTesting
    public AppMenuHandler getAppMenuHandler() {
        return null;
    }

    @Override
    public boolean shouldShowAppMenu() {
        // Do not show the menu if Contextual Search Panel is opened.
        if (mContextualSearchManager != null && mContextualSearchManager.isSearchPanelOpened()) {
            return false;
        }

        return true;
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.preferences_id) {
            PreferencesLauncher.launchSettingsPage(this, null);
            RecordUserAction.record("MobileMenuSettings");
        }

        // All the code below assumes currentTab is not null, so return early if it is null.
        final Tab currentTab = getActivityTab();
        if (currentTab == null) {
            return false;
        } else if (id == R.id.forward_menu_id) {
            if (currentTab.canGoForward()) {
                currentTab.goForward();
                RecordUserAction.record("MobileMenuForward");
                RecordUserAction.record("MobileTabClobbered");
            }
        } else if (id == R.id.bookmark_this_page_id) {
            addOrEditBookmark(currentTab);
            RecordUserAction.record("MobileMenuAddToBookmarks");
        } else if (id == R.id.reload_menu_id) {
            if (currentTab.isLoading()) {
                currentTab.stopLoading();
            } else {
                currentTab.reload();
                RecordUserAction.record("MobileToolbarReload");
            }
        } else if (id == R.id.info_menu_id) {
            WebsiteSettingsPopup.show(this, currentTab.getProfile(), currentTab.getWebContents());
        } else if (id == R.id.open_history_menu_id) {
            currentTab.loadUrl(
                    new LoadUrlParams(UrlConstants.HISTORY_URL, PageTransition.AUTO_TOPLEVEL));
            RecordUserAction.record("MobileMenuHistory");
        } else if (id == R.id.share_menu_id || id == R.id.direct_share_menu_id) {
            onShareMenuItemSelected(currentTab, getWindowAndroid(),
                    id == R.id.direct_share_menu_id, getCurrentTabModel().isIncognito());
        } else if (id == R.id.print_id) {
            PrintingController printingController = getChromeApplication().getPrintingController();
            if (printingController != null && !printingController.isBusy()
                    && PrefServiceBridge.getInstance().isPrintingEnabled()) {
                printingController.startPrint(new TabPrinter(currentTab),
                        new PrintManagerDelegateImpl(this));
                RecordUserAction.record("MobileMenuPrint");
            }
        } else if (id == R.id.add_to_homescreen_id) {
            AddToHomescreenDialog.show(this, currentTab);
            RecordUserAction.record("MobileMenuAddToHomescreen");
        } else if (id == R.id.request_desktop_site_id) {
            final boolean reloadOnChange = !currentTab.isNativePage();
            final boolean usingDesktopUserAgent = currentTab.getUseDesktopUserAgent();
            currentTab.setUseDesktopUserAgent(!usingDesktopUserAgent, reloadOnChange);
            RecordUserAction.record("MobileMenuRequestDesktopSite");
        } else if (id == R.id.reader_mode_prefs_id) {
            if (currentTab.getWebContents() != null) {
                RecordUserAction.record("DomDistiller_DistilledPagePrefsOpened");
                AlertDialog.Builder builder =
                        new AlertDialog.Builder(this, R.style.AlertDialogTheme);
                builder.setView(DistilledPagePrefsView.create(this));
                builder.show();
            }
        } else if (id == R.id.help_id) {
            // Since reading back the compositor is asynchronous, we need to do the readback
            // before starting the GoogleHelp.
            final String helpContextId = HelpAndFeedback.getHelpContextIdFromUrl(
                    this, currentTab.getUrl(), getCurrentTabModel().isIncognito());
            final Activity mainActivity = this;
            final FeedbackCollector collector =
                    FeedbackCollector.create(currentTab.getProfile(), currentTab.getUrl());
            startTakingCompositorActivityScreenshot(new GetBitmapCallback() {
                @Override
                public void onFinishGetBitmap(Bitmap bitmap, int response) {
                    HelpAndFeedback.getInstance(mainActivity)
                            .show(mainActivity, helpContextId, bitmap, collector);
                    RecordUserAction.record("MobileMenuFeedback");
                }
            });
        } else {
            return false;
        }
        return true;
    }

    @Override
    public void onTabSelectionHinted(int tabId) { }

    @Override
    public void onSceneChange(Layout layout) { }
}
