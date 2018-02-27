// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActionModeCallback;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.FrozenNativePage;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.TabState.WebContentsState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchTabHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.download.ChromeDownloadDelegate;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.media.ui.MediaSessionTabHelper;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.metrics.StartupPageLoadMetricsObserver;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.ntp.NativePageAssassin;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ssl.SecurityStateModel;
import org.chromium.chrome.browser.tab.TabUma.TabCreationState;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.vr_shell.VrShellDelegate;
import org.chromium.chrome.browser.widget.PulseDrawable;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.content_view.ContentView;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.sync.SyncConstants;
import org.chromium.content.browser.crypto.CipherFactory;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.content_public.common.Referrer;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.util.List;

/**
 * The basic Java representation of a tab.  Contains and manages a {@link ContentView}.
 * <p>
 * This class is intended to be extended either on Java or both Java and C++, with ownership managed
 * by this base class.
 * <p>
 * Extending just Java:
 *  - Just extend the class normally.  Do not override initializeNative().
 * Extending Java and C++:
 *  - Because of the inner-workings of JNI, the subclass is responsible for constructing the native
 *    subclass, which in turn constructs TabAndroid (the native counterpart to Tab), which in
 *    turn sets the native pointer for Tab.  For destruction, subclasses in Java must clear
 *    their own native pointer reference, but Tab#destroy() will handle deleting the native
 *    object.
 */
public class Tab
        implements ViewGroup.OnHierarchyChangeListener, View.OnSystemUiVisibilityChangeListener {
    public static final int INVALID_TAB_ID = -1;

    /** Return value from {@link #getBookmarkId()} if this tab is not bookmarked. */
    public static final long INVALID_BOOKMARK_ID = -1;

    private static final long INVALID_TIMESTAMP = -1;

    /**
     * The required page load percentage for the page to be considered ready assuming the
     * TextureView is also ready.
     */
    private static final int CONSIDERED_READY_LOAD_PERCENTAGE = 100;

    /** Used for logging. */
    private static final String TAG = "Tab";

    private static final String PRODUCT_VERSION = ChromeVersionInfo.getProductVersion();

    private long mNativeTabAndroid;

    /** Unique id of this tab (within its container). */
    private final int mId;

    /** Whether or not this tab is an incognito tab. */
    private final boolean mIncognito;

    /**
     * An Application {@link Context}.  Unlike {@link #mActivity}, this is the only one that is
     * publicly exposed to help prevent leaking the {@link Activity}.
     */
    private final Context mThemedApplicationContext;

    /** Gives {@link Tab} a way to interact with the Android window. */
    private WindowAndroid mWindowAndroid;

    /** Whether or not this {@link Tab} is initialized and should be interacted with. */
    private boolean mIsInitialized;

    /** The current native page (e.g. chrome-native://newtab), or {@code null} if there is none. */
    private NativePage mNativePage;

    /** InfoBar container to show InfoBars for this tab. */
    private InfoBarContainer mInfoBarContainer;

    /** Controls overscroll pull-to-refresh behavior for this tab. */
    private SwipeRefreshHandler mSwipeRefreshHandler;

    /** The sync id of the Tab if session sync is enabled. */
    private int mSyncId = SyncConstants.INVALID_TAB_NODE_ID;

    /** {@link ContentViewCore} showing the current page, or {@code null} if the tab is frozen. */
    private ContentViewCore mContentViewCore;

    /** Listens to gesture events fired by the ContentViewCore. */
    private GestureStateListener mGestureStateListener;

    /** The parent view of the ContentView and the InfoBarContainer. */
    private View mContentView;

    /** A list of Tab observers.  These are used to broadcast Tab events to listeners. */
    private final ObserverList<TabObserver> mObservers = new ObserverList<>();

    // Content layer Observers and Delegates
    private TabWebContentsObserver mWebContentsObserver;
    private TabWebContentsDelegateAndroid mWebContentsDelegate;
    private StartupPageLoadMetricsObserver mStartupPageLoadMetricsObserver;

    /**
     * If this tab was opened from another tab, store the id of the tab that
     * caused it to be opened so that we can activate it when this tab gets
     * closed.
     */
    private int mParentId = INVALID_TAB_ID;

    /**
     * If this tab was opened from another tab in another Activity, this is the Intent that can be
     * fired to bring the parent Activity back.
     * TODO(dfalcantara): Remove this mechanism when we have a global TabManager.
     */
    private Intent mParentIntent;

    /**
     * Whether the tab should be grouped with its parent tab.
     */
    private boolean mGroupedWithParent = true;

    private boolean mIsClosing;
    private boolean mIsShowingErrorPage;
    private boolean mIsShowingTabModalDialog;

    private Bitmap mFavicon;

    private String mFaviconUrl;

    /**
     * The size in pixels at which favicons will be drawn. Ideally mFavicon will have this size to
     * avoid scaling artifacts.
     */
    private int mIdealFaviconSize;

    /** Whether or not the TabState has changed. */
    private boolean mIsTabStateDirty = true;

    /**
     * Saves how this tab was launched (from a link, external app, etc) so that
     * we can determine the different circumstances in which it should be
     * closed. For example, a tab opened from an external app should be closed
     * when the back stack is empty and the user uses the back hardware key. A
     * standard tab however should be kept open and the entire activity should
     * be moved to the background.
     */
    private final TabLaunchType mLaunchType;

    /**
     * Navigation state of the WebContents as returned by nativeGetContentsStateAsByteBuffer(),
     * stored to be inflated on demand using unfreezeContents(). If this is not null, there is no
     * WebContents around. Upon tab switch WebContents will be unfrozen and the variable will be set
     * to null.
     */
    private WebContentsState mFrozenContentsState;

    /**
     * Whether the restoration from frozen state failed.
     */
    private boolean mFailedToRestore;

    /**
     * URL load to be performed lazily when the Tab is next shown.
     */
    private LoadUrlParams mPendingLoadParams;

    /**
     * URL of the page currently loading. Used as a fall-back in case tab restore fails.
     */
    private String mUrl;

    /**
     * The external application that this Tab is associated with (null if not associated with any
     * app). Allows reusing of tabs opened from the same application.
     */
    private String mAppAssociatedWith;

    /**
     * Keeps track of whether the Tab should be kept in the TabModel after the user hits "back".
     * Used by Document mode to keep track of whether we want to remove the tab when user hits back.
     */
    private boolean mShouldPreserve;

    /**
     * Indicates if the tab needs to be reloaded upon next display. This is set to true when the tab
     * crashes while in background, or if a speculative restore in background gets cancelled before
     * it completes.
     */
    private boolean mNeedsReload;

    /**
     * True while a page load is in progress.
     */
    private boolean mIsLoading;

    /**
     * True while a restore page load is in progress.
     */
    private boolean mIsBeingRestored;

    /**
     * Whether or not the Tab is currently visible to the user.
     */
    private boolean mIsHidden = true;

    /**
     * Importance of the WebContents currently attached to this tab. Note the key difference from
     * |mIsHidden| is that a tab is hidden when the application is hidden, but the importance is
     * not affected by this signal.
     */
    private @ChildProcessImportance int mImportance = ChildProcessImportance.NORMAL;

    /** Whether the renderer is currently unresponsive. */
    private boolean mIsRendererUnresponsive;

    /**
     * The last time this tab was shown or the time of its initialization if it wasn't yet shown.
     */
    private long mTimestampMillis = INVALID_TIMESTAMP;

    /**
     * Title of the ContentViews webpage.Always update mTitle through updateTitle() so that it also
     * updates mIsTitleDirectionRtl correctly.
     */
    private String mTitle;

    /**
     * Indicates if mTitle should be displayed from right to left.
     */
    private boolean mIsTitleDirectionRtl;

    /**
     * The mInterceptNavigationDelegate will be consulted for top-level frame navigations. This
     * allows presenting the intent picker to the user so that a native Android application can be
     * used if available.
     */
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;

    /**
     * Whether didCommitProvisionalLoadForFrame() hasn't yet been called for the current native page
     * (page A). To decrease latency, we show native pages in both loadUrl() and
     * didCommitProvisionalLoadForFrame(). However, we mustn't show a new native page (page B) in
     * loadUrl() if the current native page hasn't yet been committed. Otherwise, we'll show each
     * page twice (A, B, A, B): the first two times in loadUrl(), the second two times in
     * didCommitProvisionalLoadForFrame().
     */
    private boolean mIsNativePageCommitPending;

    private TabRedirectHandler mTabRedirectHandler;

    private FullscreenManager mFullscreenManager;
    private float mPreviousTopControlsOffsetY = Float.NaN;
    private float mPreviousBottomControlsOffsetY = Float.NaN;
    private float mPreviousContentOffsetY = Float.NaN;

    /**
     * Indicates whether this tab is detached from any activity and its corresponding
     * {@link WindowAndroid}.
     */
    private boolean mIsDetached;

    /**
     * The UMA object used to report stats for this tab. Note that this may be null under certain
     * conditions, such as incognito mode.
     */
    private TabUma mTabUma;

    /**
     * Reference to the current sadTabView if one is defined.
     */
    private View mSadTabView;

    /**
     * Counts the number of successive refreshes on the sad tab page. The count is is reset after a
     * successful page load.
     */
    private int mSadTabSuccessiveRefreshCounter;

    /**
     * Stores total data saved at the start of a page load. Used to calculate delta at the end of
     * page load, which is just an estimate of the data saved for the current page load since there
     * may be multiple pages loading at the same time. This estimate is used to get an idea of how
     * widely used the data saver feature is for a particular user at a time (i.e. not since the
     * user started using Chrome).
     */
    private long mDataSavedOnStartPageLoad;

    private int mDefaultThemeColor;
    private int mThemeColor;

    private ChromeDownloadDelegate mDownloadDelegate;

    /**
     * The Text bubble used to display In Product help widget for download feature on videos.
     */
    private TextBubble mDownloadIPHBubble;

    /**
     * The popup used to display the pulse around the download button on videos.
     */
    private PopupWindow mPulsePopupWindow;

    /** Whether or not the tab closing the tab can send the user back to the app that opened it. */
    private boolean mIsAllowedToReturnToExternalApp;

    private int mTopControlsHeight;
    private int mBottomControlsHeight;
    private boolean mControlsResizeView;

    private GestureStateListener createGestureStateListener() {
        return new GestureStateListener() {
            @Override
            public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            private void onScrollingStateChanged() {
                FullscreenManager fullscreenManager = getFullscreenManager();
                if (fullscreenManager == null) return;
                fullscreenManager.onContentViewScrollingStateChanged(
                        getContentViewCore() != null && getContentViewCore().isScrollInProgress());
            }
        };
    }

    // TODO(dtrainor): Port more methods to the observer.
    private final TabObserver mTabObserver = new EmptyTabObserver() {
        /** A runnable to delay the enabling of fullscreen mode if necessary. */
        private Runnable mEnterFullscreenRunnable;

        @Override
        public void onSSLStateUpdated(Tab tab) {
            PolicyAuditor auditor = AppHooks.get().getPolicyAuditor();
            auditor.notifyCertificateFailure(
                    PolicyAuditor.nativeGetCertificateFailure(getWebContents()),
                    getApplicationContext());
            updateFullscreenEnabledState();
            updateThemeColorIfNeeded(false);
        }

        @Override
        public void onUrlUpdated(Tab tab) {
            updateThemeColorIfNeeded(false);
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            if (!didStartLoad) return;

            String url = tab.getUrl();
            // Simulate the PAGE_LOAD_STARTED notification that we did not get.
            didStartPageLoad(url, false);

            if (didFinishLoad) {
                // Simulate the PAGE_LOAD_FINISHED notification that we did not get.
                didFinishPageLoad();
            }
        }

        @Override
        public void onInteractabilityChanged(boolean interactable) {
            if (interactable && mEnterFullscreenRunnable != null) mEnterFullscreenRunnable.run();
        }

        @Override
        public void onToggleFullscreenMode(Tab tab, boolean enable) {
            if (!isUserInteractable() && enable) {
                mEnterFullscreenRunnable = new Runnable() {
                    @Override
                    public void run() {
                        toggleFullscreenInternal(true);
                        mEnterFullscreenRunnable = null;
                    }
                };
                return;
            } else if (!enable && mEnterFullscreenRunnable != null) {
                mEnterFullscreenRunnable = null;
                return;
            }

            toggleFullscreenInternal(enable);
        }

        /**
         * Do the actual enter/exit of fullscreen mode.
         * @param enable Whether or not fullscreen is enabled.
         */
        private void toggleFullscreenInternal(boolean enable) {
            if (mFullscreenManager != null) {
                mFullscreenManager.setPersistentFullscreenMode(enable);
            }

            if (enable && getWebContents() != null) {
                SelectionPopupController controller =
                        SelectionPopupController.fromWebContents(getWebContents());
                controller.destroySelectActionMode();
            }

            // When going into fullscreen, we want to remove any cached thumbnail of the Tab.
            if (enable && mNativeTabAndroid != 0) {
                nativeClearThumbnailPlaceholder(mNativeTabAndroid);
            }
        }
    };

    private TabDelegateFactory mDelegateFactory;

    private BrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;

    /** Listens for views related to the tab to be attached or detached. */
    private OnAttachStateChangeListener mAttachStateChangeListener;

    /** Whether the tab can currently be interacted with. */
    private boolean mInteractableState;

    /** Whether or not the tab's active view is attached to the window. */
    private boolean mIsViewAttachedToWindow;

    /**
     * Creates an instance of a {@link Tab}.
     *
     * This constructor may be called before the native library has been loaded, so any additions
     * must be vetted for library calls.
     *
     * @param id        The id this tab should be identified with.
     * @param incognito Whether or not this tab is incognito.
     * @param window    An instance of a {@link WindowAndroid}.
     */
    public Tab(int id, boolean incognito, WindowAndroid window) {
        this(id, INVALID_TAB_ID, incognito, window, null, null, null);
    }

    /**
     * Creates an instance of a {@link Tab}.
     *
     * This constructor can be called before the native library has been loaded, so any additions
     * must be vetted for library calls.
     *
     * @param id          The id this tab should be identified with.
     * @param parentId    The id id of the tab that caused this tab to be opened.
     * @param incognito   Whether or not this tab is incognito.
     * @param window      An instance of a {@link WindowAndroid}.
     * @param creationState State in which the tab is created, needed to initialize TabUma
     *                      accounting. When null, TabUma will not be initialized.
     * @param frozenState State containing information about this Tab, if it was persisted.
     */
    @SuppressLint("HandlerLeak")
    public Tab(int id, int parentId, boolean incognito, WindowAndroid window, TabLaunchType type,
            TabCreationState creationState, TabState frozenState) {
        mId = TabIdManager.getInstance().generateValidId(id);
        mParentId = parentId;
        mIncognito = incognito;
        mThemedApplicationContext = new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), ChromeActivity.getThemeId());
        mWindowAndroid = window;
        mLaunchType = type;
        mIsDetached = getActivity() == null;

        Resources resources = mThemedApplicationContext.getResources();
        mIdealFaviconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
        mDefaultThemeColor = calculateDefaultThemeColor();
        mThemeColor = calculateThemeColor(false);

        // Restore data from the TabState, if it existed.
        if (frozenState != null) {
            assert type == TabLaunchType.FROM_RESTORE;
            restoreFieldsFromState(frozenState);
        }

        mTabRedirectHandler = new TabRedirectHandler(mThemedApplicationContext);
        addObserver(mTabObserver);

        if (incognito) {
            CipherFactory.getInstance().triggerKeyGeneration();
        }

        ContextualSearchTabHelper.createForTab(this);
        MediaSessionTabHelper.createForTab(this);

        if (creationState != null) {
            mTabUma = new TabUma(creationState);
            if (frozenState == null) {
                assert creationState != TabCreationState.FROZEN_ON_RESTORE;
            } else {
                assert type == TabLaunchType.FROM_RESTORE
                        && creationState == TabCreationState.FROZEN_ON_RESTORE;
            }
        }

        mAttachStateChangeListener = new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {
                mIsViewAttachedToWindow = true;
                updateInteractableState();
            }

            @Override
            public void onViewDetachedFromWindow(View view) {
                mIsViewAttachedToWindow = false;
                updateInteractableState();
            }
        };
    }

    private int calculateDefaultThemeColor() {
        boolean useModernDesign = getActivity() != null && getActivity().supportsModernDesign()
                && FeatureUtilities.isChromeModernDesignEnabled();
        Resources resources = mThemedApplicationContext.getResources();
        return ColorUtils.getDefaultThemeColor(resources, useModernDesign, mIncognito);
    }

    /**
     * Restores member fields from the given TabState.
     * @param state TabState containing information about this Tab.
     */
    private void restoreFieldsFromState(TabState state) {
        assert state != null;
        mAppAssociatedWith = state.openerAppId;
        mFrozenContentsState = state.contentsState;
        mSyncId = (int) state.syncId;
        mShouldPreserve = state.shouldPreserve;
        mTimestampMillis = state.timestampMillis;
        mUrl = state.getVirtualUrlFromState();

        mThemeColor = state.hasThemeColor() ? state.getThemeColor() : getDefaultThemeColor();

        mTitle = state.getDisplayTitleFromState();
        mIsTitleDirectionRtl = mTitle != null
                && LocalizationUtils.getFirstStrongCharacterDirection(mTitle)
                        == LocalizationUtils.RIGHT_TO_LEFT;
    }

    /**
     * Adds a {@link TabObserver} to be notified on {@link Tab} changes.
     * @param observer The {@link TabObserver} to add.
     */
    public void addObserver(TabObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes a {@link TabObserver}.
     * @param observer The {@link TabObserver} to remove.
     */
    public void removeObserver(TabObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Whether or not this tab has a previous navigation entry.
     */
    public boolean canGoBack() {
        return getWebContents() != null && getWebContents().getNavigationController().canGoBack();
    }

    /**
     * @return Whether or not this tab has a navigation entry after the current one.
     */
    public boolean canGoForward() {
        return getWebContents() != null
                && getWebContents().getNavigationController().canGoForward();
    }

    /**
     * Goes to the navigation entry before the current one.
     */
    public void goBack() {
        if (getWebContents() != null) getWebContents().getNavigationController().goBack();
    }

    /**
     * Goes to the navigation entry after the current one.
     */
    public void goForward() {
        if (getWebContents() != null) getWebContents().getNavigationController().goForward();
    }

    /**
     * Loads the current navigation if there is a pending lazy load (after tab restore).
     */
    public void loadIfNecessary() {
        if (getWebContents() != null) getWebContents().getNavigationController().loadIfNecessary();
    }

    /**
     * Requests the current navigation to be loaded upon the next call to loadIfNecessary().
     */
    protected void requestRestoreLoad() {
        if (getWebContents() != null) {
            getWebContents().getNavigationController().requestRestoreLoad();
        }
    }

    /**
     * Causes this tab to navigate to the specified URL.
     * @param params parameters describing the url load. Note that it is important to set correct
     *               page transition as it is used for ranking URLs in the history so the omnibox
     *               can report suggestions correctly.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrl(LoadUrlParams params) {
        try {
            TraceEvent.begin("Tab.loadUrl");
            // TODO(tedchoc): When showing the android NTP, delay the call to nativeLoadUrl until
            //                the android view has entirely rendered.
            if (!mIsNativePageCommitPending) {
                mIsNativePageCommitPending = maybeShowNativePage(params.getUrl(), false);
            }

            removeSadTabIfPresent();

            // Clear the app association if the user navigated to a different page from the omnibox.
            if ((params.getTransitionType() & PageTransition.FROM_ADDRESS_BAR)
                    == PageTransition.FROM_ADDRESS_BAR) {
                mAppAssociatedWith = null;
                setIsAllowedToReturnToExternalApp(false);
            }
            if ("chrome://java-crash/".equals(params.getUrl())) {
                return handleJavaCrash();
            }

            if (mNativeTabAndroid == 0) {
                // if mNativeTabAndroid is invalid then we are going to crash anyways on the
                // native side. Lets crash on the java side so that we can have a better stack
                // trace. https://crbug.com/662877
                throw new RuntimeException("Please post this crash on crbug.com/662877");
            }

            // We load the URL from the tab rather than directly from the ContentView so the tab has
            // a chance of using a prerenderer page is any.
            int loadType = nativeLoadUrl(mNativeTabAndroid, params.getUrl(),
                    params.getVerbatimHeaders(), params.getPostData(), params.getTransitionType(),
                    params.getReferrer() != null ? params.getReferrer().getUrl() : null,
                    // Policy will be ignored for null referrer url, 0 is just a placeholder.
                    // TODO(ppi): Should we pass Referrer jobject and add JNI methods to read it
                    //            from the native?
                    params.getReferrer() != null ? params.getReferrer().getPolicy() : 0,
                    params.getIsRendererInitiated(), params.getShouldReplaceCurrentEntry(),
                    params.getIntentReceivedTimestamp(), params.getHasUserGesture(),
                    params.getShouldClearHistoryList());

            for (TabObserver observer : mObservers) {
                observer.onLoadUrl(this, params, loadType);
            }
            return loadType;
        } finally {
            TraceEvent.end("Tab.loadUrl");
        }
    }

    /**
     * Called when the contextual ActionBar is shown or hidden.
     * @param show {@code true} when the ActionBar is shown; {@code false} otherwise.
     */
    public void notifyContextualActionBarVisibilityChanged(boolean show) {
        for (TabObserver observer : mObservers) {
            observer.onContextualActionBarVisibilityChanged(this, show);
        }
    }

    /**
     * Throws a RuntimeException. Useful for testing crash reports with obfuscated Java stacktraces.
     */
    private int handleJavaCrash() {
        throw new RuntimeException("Intentional Java Crash");
    }

    /**
     * Load the original image (uncompressed by spdy proxy) in this tab.
     */
    void loadOriginalImage() {
        if (mNativeTabAndroid != 0) nativeLoadOriginalImage(mNativeTabAndroid);
    }

    /**
     * @return Whether or not the {@link Tab} is currently showing an interstitial page, such as
     *         a bad HTTPS page.
     */
    public boolean isShowingInterstitialPage() {
        return getWebContents() != null && getWebContents().isShowingInterstitialPage();
    }

    /**
     * @return Whether a tab modal dialog is showing.
     */
    public boolean isShowingTabModalDialog() {
        return mIsShowingTabModalDialog;
    }

    /**
     * @return Whether the {@link Tab} is currently showing an error page.
     */
    public boolean isShowingErrorPage() {
        return mIsShowingErrorPage;
    }

    /**
     * Sets whether the tab is showing an error page.  This is reset whenever the tab finishes a
     * navigation.
     *
     * @param isShowingErrorPage Whether the tab shows an error page.
     */
    public void setIsShowingErrorPage(boolean isShowingErrorPage) {
        mIsShowingErrorPage = isShowingErrorPage;
    }

    /**
     * @return Whether or not the tab has something valid to render.
     */
    public boolean isReady() {
        return mNativePage != null || (getWebContents() != null && getWebContents().isReady());
    }

    /**
     * @return The {@link View} displaying the current page in the tab. This can be {@code null}, if
     *         the tab is frozen or being initialized or destroyed.
     */
    public View getView() {
        return mNativePage != null ? mNativePage.getView() : mContentView;
    }

    /**
     * @return The width of the content of this tab.  Can be 0 if there is no content.
     */
    public int getWidth() {
        View view = getView();
        return view != null ? view.getWidth() : 0;
    }

    /**
     * @return The height of the content of this tab.  Can be 0 if there is no content.
     */
    public int getHeight() {
        View view = getView();
        return view != null ? view.getHeight() : 0;
    }

    /**
     * @return The application {@link Context} associated with this tab.
     */
    protected Context getApplicationContext() {
        return mThemedApplicationContext.getApplicationContext();
    }

    /**
     * @return {@link ChromeActivity} that currently contains this {@link Tab} in its
     *         {@link TabModel}.
     */
    public ChromeActivity getActivity() {
        if (getWindowAndroid() == null) return null;
        Activity activity = WindowAndroid.activityFromContext(
                getWindowAndroid().getContext().get());
        if (activity instanceof ChromeActivity) return (ChromeActivity) activity;
        return null;
    }

    /**
     * @return {@link TabModelSelector} that currently hosts the {@link TabModel} for this
     *         {@link Tab}.
     */
    public TabModelSelector getTabModelSelector() {
        if (getActivity() == null) return null;
        return getActivity().getTabModelSelector();
    }

    /**
     * @return The infobar container.
     */
    public final InfoBarContainer getInfoBarContainer() {
        return mInfoBarContainer;
    }

    /** @return An opaque "state" object that can be persisted to storage. */
    public TabState getState() {
        if (!isInitialized()) return null;
        TabState tabState = new TabState();
        tabState.contentsState = getWebContentsState();
        tabState.openerAppId = mAppAssociatedWith;
        tabState.parentId = mParentId;
        tabState.shouldPreserve = mShouldPreserve;
        tabState.syncId = mSyncId;
        tabState.timestampMillis = mTimestampMillis;
        tabState.themeColor = getThemeColor();
        return tabState;
    }

    /** @return WebContentsState representing the state of the WebContents (navigations, etc.) */
    public WebContentsState getFrozenContentsState() {
        return mFrozenContentsState;
    }

    /** Returns an object representing the state of the Tab's WebContents. */
    private TabState.WebContentsState getWebContentsState() {
        if (mFrozenContentsState != null) return mFrozenContentsState;

        // Native call returns null when buffer allocation needed to serialize the state failed.
        ByteBuffer buffer = getWebContentsStateAsByteBuffer();
        if (buffer == null) return null;

        TabState.WebContentsState state = new TabState.WebContentsStateNative(buffer);
        state.setVersion(TabState.CONTENTS_STATE_CURRENT_VERSION);
        return state;
    }

    /** Returns an ByteBuffer representing the state of the Tab's WebContents. */
    private ByteBuffer getWebContentsStateAsByteBuffer() {
        if (mPendingLoadParams == null) {
            return TabState.getContentsStateAsByteBuffer(this);
        } else {
            Referrer referrer = mPendingLoadParams.getReferrer();
            return TabState.createSingleNavigationStateAsByteBuffer(
                    mPendingLoadParams.getUrl(),
                    referrer != null ? referrer.getUrl() : null,
                    // Policy will be ignored for null referrer url, 0 is just a placeholder.
                    referrer != null ? referrer.getPolicy() : 0,
                    isIncognito());
        }
    }

    /**
     * Prints the current page.
     *
     * @return Whether the printing process is started successfully.
     **/
    public boolean print(int renderProcessId, int renderFrameId) {
        assert mNativeTabAndroid != 0;
        return nativePrint(mNativeTabAndroid, renderProcessId, renderFrameId);
    }

    @CalledByNative
    public void setPendingPrint(int renderProcessId, int renderFrameId) {
        PrintingController printingController = PrintingControllerImpl.getInstance();
        if (printingController == null) return;

        printingController.setPendingPrint(new TabPrinter(this),
                new PrintManagerDelegateImpl(getActivity()), renderProcessId, renderFrameId);
    }

    /**
     * Reloads the current page content.
     */
    public void reload() {
        // TODO(dtrainor): Should we try to rebuild the ContentView if it's frozen?
        if (OfflinePageUtils.isOfflinePage(this)) {
            // If current page is an offline page, reload it with custom behavior defined in extra
            // header respected.
            OfflinePageUtils.reload(this);
        } else {
            if (getWebContents() != null) getWebContents().getNavigationController().reload(true);
        }
    }

    /**
     * Reloads the current page content.
     * This version ignores the cache and reloads from the network.
     */
    public void reloadIgnoringCache() {
        if (getWebContents() != null) {
            getWebContents().getNavigationController().reloadBypassingCache(true);
        }
    }

    /**
     * @return Whether or not the loading and rendering of the page is done.
     */
    @VisibleForTesting
    public boolean isLoadingAndRenderingDone() {
        return isReady() && getProgress() >= CONSIDERED_READY_LOAD_PERCENTAGE;
    }

    /** Stop the current navigation. */
    public void stopLoading() {
        if (isLoading()) {
            RewindableIterator<TabObserver> observers = getTabObservers();
            while (observers.hasNext()) {
                observers.next().onPageLoadFinished(this);
            }
        }
        if (getWebContents() != null) getWebContents().stop();
    }

    /**
     * @return a value between 0 and 100 reflecting what percentage of the page load is complete.
     */
    public int getProgress() {
        if (!isLoading()) return 100;

        TabWebContentsDelegateAndroid delegate = getTabWebContentsDelegateAndroid();
        return delegate != null ? delegate.getMostRecentProgress() : 0;
    }

    /**
     * @return The background color of the tab.
     */
    public int getBackgroundColor() {
        if (mNativePage != null) return mNativePage.getBackgroundColor();
        if (getWebContents() != null) return getWebContents().getBackgroundColor();
        return Color.WHITE;
    }

    /**
     * @return The current theme color based on the value passed from the web contents and the
     *         security state.
     */
    public int getThemeColor() {
        return mThemeColor;
    }

    /**
     * Calculate the theme color based on if the page is native, the theme color changed, etc.
     * @param didWebContentsThemeColorChange If the theme color of the web contents is known to have
     *                                       changed.
     * @return The theme color that should be used for this tab.
     */
    private int calculateThemeColor(boolean didWebContentsThemeColorChange) {
        if (isNativePage()) return mNativePage.getThemeColor();

        // Start by assuming the current theme color is that one that should be used. This will
        // either be transparent, the last theme color, or the color restored from TabState.
        int themeColor = ColorUtils.isValidThemeColor(mThemeColor) || mThemeColor == 0
                ? mThemeColor
                : getDefaultThemeColor();

        // Only use the web contents for the theme color if it is known to have changed, This
        // corresponds to the didChangeThemeColor in WebContentsObserver.
        if (getWebContents() != null && didWebContentsThemeColorChange) {
            themeColor = getWebContents().getThemeColor();
            if (themeColor != 0 && !ColorUtils.isValidThemeColor(themeColor)) themeColor = 0;
        }

        // Do not apply the theme color if there are any security issues on the page.
        int securityLevel = getSecurityLevel();
        if (securityLevel == ConnectionSecurityLevel.DANGEROUS
                || securityLevel == ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT) {
            themeColor = getDefaultThemeColor();
        }

        if (isShowingInterstitialPage()) themeColor = getDefaultThemeColor();

        if (themeColor == Color.TRANSPARENT) themeColor = getDefaultThemeColor();
        if (isIncognito()) themeColor = getDefaultThemeColor();

        // Ensure there is no alpha component to the theme color as that is not supported in the
        // dependent UI.
        themeColor |= 0xFF000000;
        return themeColor;
    }

    /**
     * Determines if the theme color has changed and notifies the listeners if it has.
     * @param didWebContentsThemeColorChange If the theme color of the web contents is known to have
     *                                       changed.
     */
    void updateThemeColorIfNeeded(boolean didWebContentsThemeColorChange) {
        int themeColor = calculateThemeColor(didWebContentsThemeColorChange);
        if (themeColor == mThemeColor) return;
        mThemeColor = themeColor;
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            observers.next().onDidChangeThemeColor(this, themeColor);
        }
    }

    /**
     * @return The web contents associated with this tab.
     */
    @Nullable
    public WebContents getWebContents() {
        return mContentViewCore != null ? mContentViewCore.getWebContents() : null;
    }

    /**
     * @return The profile associated with this tab.
     */
    public Profile getProfile() {
        if (mNativeTabAndroid == 0) return null;
        return nativeGetProfileAndroid(mNativeTabAndroid);
    }

    /**
     * For more information about the uniqueness of {@link #getId()} see comments on {@link Tab}.
     * @see Tab
     * @return The id representing this tab.
     */
    @CalledByNative
    public int getId() {
        return mId;
    }

    public boolean isIncognito() {
        return mIncognito;
    }

    /**
     * @return The {@link ContentViewCore} associated with the current page.
     */
    public ContentViewCore getActiveContentViewCore() {
        return mContentViewCore;
    }

    /**
     * @return The {@link ContentViewCore} associated with the current page, or {@code null} if
     *         there is no current page or the current page is displayed using a native view.
     */
    public ContentViewCore getContentViewCore() {
        return mNativePage == null ? mContentViewCore : null;
    }

    /**
     * @return The {@link NativePage} associated with the current page, or {@code null} if there is
     *         no current page or the current page is displayed using something besides
     *         {@link NativePage}.
     */
    public NativePage getNativePage() {
        return mNativePage;
    }

    /**
     * @return Whether or not the {@link Tab} represents a {@link NativePage}.
     */
    @CalledByNative
    public boolean isNativePage() {
        return mNativePage != null;
    }

    /**
     * Set whether or not the {@link ContentViewCore} should be using a desktop user agent for the
     * currently loaded page.
     * @param useDesktop     If {@code true}, use a desktop user agent.  Otherwise use a mobile one.
     * @param reloadOnChange Reload the page if the user agent has changed.
     */
    public void setUseDesktopUserAgent(boolean useDesktop, boolean reloadOnChange) {
        if (getWebContents() != null) {
            getWebContents().getNavigationController().setUseDesktopUserAgent(
                    useDesktop, reloadOnChange);
        }
    }

    /**
     * @return Whether or not the {@link ContentViewCore} is using a desktop user agent.
     */
    public boolean getUseDesktopUserAgent() {
        return getWebContents() != null
                && getWebContents().getNavigationController().getUseDesktopUserAgent();
    }

    /**
     * @return The current {@link ConnectionSecurityLevel} for the tab.
     */
    // TODO(tedchoc): Remove this and transition all clients to use ToolbarModel directly.
    public int getSecurityLevel() {
        return SecurityStateModel.getSecurityLevelForWebContents(getWebContents());
    }

    /**
     * @return The sync id of the tab if session sync is enabled, {@code 0} otherwise.
     */
    @CalledByNative
    private int getSyncId() {
        return mSyncId;
    }

    /**
     * @param syncId The sync id of the tab if session sync is enabled.
     */
    @CalledByNative
    private void setSyncId(int syncId) {
        mSyncId = syncId;
    }

    /**
     * @return An {@link ObserverList.RewindableIterator} instance that points to all of
     *         the current {@link TabObserver}s on this class.  Note that calling
     *         {@link java.util.Iterator#remove()} will throw an
     *         {@link UnsupportedOperationException}.
     */
    protected ObserverList.RewindableIterator<TabObserver> getTabObservers() {
        return mObservers.rewindableIterator();
    }

    /**
     * Called on the foreground tab when the Activity showing the Tab gets started. This is called
     * on both cold and warm starts.
     */
    public void onActivityShown() {
        if (isHidden()) {
            show(TabSelectionType.FROM_USER);
        } else {
            // The visible Tab's renderer process may have died after the activity was paused.
            // Ensure that it's restored appropriately.
            loadIfNeeded();
        }

        // When resuming the activity, force an update to the fullscreen state to ensure a
        // subactivity did not change the fullscreen configuration of this ChromeTab's renderer in
        // the case where it was shared.
        updateFullscreenEnabledState();
    }

    /**
     * Called on the foreground tab when the Activity showing the Tab gets stopped.
     */
    public void onActivityHidden() {
        hide();

        if (mTabUma != null) mTabUma.onActivityHidden();
    }

    /**
     * @return Whether the tab is ready to display or it should be faded in as it loads.
     */
    public boolean shouldStall() {
        return (isFrozen() || needsReload())
                && !NativePageFactory.isNativePageUrl(getUrl(), isIncognito());
    }

    /**
     * Prepares the tab to be shown. This method is supposed to be called before the tab is
     * displayed. It restores the ContentView if it is not available after the cold start and
     * reloads the tab if its renderer has crashed.
     * @param type Specifies how the tab was selected.
     */
    public final void show(TabSelectionType type) {
        try {
            TraceEvent.begin("Tab.show");
            if (!isHidden()) return;
            // Keep unsetting mIsHidden above loadIfNeeded(), so that we pass correct visibility
            // when spawning WebContents in loadIfNeeded().
            mIsHidden = false;
            updateInteractableState();

            loadIfNeeded();
            assert !isFrozen();

            if (mContentViewCore != null) mContentViewCore.onShow();

            if (mTabUma != null) {
                mTabUma.onShow(type, getTimestampMillis(),
                        computeMRURank(this, getTabModelSelector().getModel(mIncognito)));
            }

            // If the NativePage was frozen while in the background (see NativePageAssassin),
            // recreate the NativePage now.
            NativePage nativePage = getNativePage();
            if (nativePage instanceof FrozenNativePage) {
                maybeShowNativePage(nativePage.getUrl(), true);
            }
            NativePageAssassin.getInstance().tabShown(this);

            // If the page is still loading, update the progress bar (otherwise it would not show
            // until the renderer notifies of new progress being made).
            if (getProgress() < 100 && !isShowingInterstitialPage()) {
                notifyLoadProgress(getProgress());
            }

            // Updating the timestamp has to happen after the showInternal() call since subclasses
            // may use it for logging.
            mTimestampMillis = System.currentTimeMillis();

            for (TabObserver observer : mObservers) observer.onShown(this);
        } finally {
            TraceEvent.end("Tab.show");
        }
    }

    /**
     * Triggers the hiding logic for the view backing the tab.
     */
    public final void hide() {
        try {
            TraceEvent.begin("Tab.hide");
            if (isHidden()) return;
            mIsHidden = true;
            updateInteractableState();

            if (mContentViewCore != null) mContentViewCore.onHide();

            // Clean up any fullscreen state that might impact other tabs.
            if (mFullscreenManager != null) {
                mFullscreenManager.setPersistentFullscreenMode(false);
            }

            if (mTabUma != null) mTabUma.onHide();

            mTabRedirectHandler.clear();

            // Allow this tab's NativePage to be frozen if it stays hidden for a while.
            NativePageAssassin.getInstance().tabHidden(this);

            for (TabObserver observer : mObservers) observer.onHidden(this);
        } finally {
            TraceEvent.end("Tab.hide");
        }
    }

    public final void setImportance(@ChildProcessImportance int importance) {
        if (mImportance == importance) return;
        mImportance = importance;
        WebContents webContents = getWebContents();
        if (webContents == null) return;
        webContents.setImportance(mImportance);
    }

    /**
     * Shows the given {@code nativePage} if it's not already showing.
     * @param nativePage The {@link NativePage} to show.
     */
    private void showNativePage(NativePage nativePage) {
        if (mNativePage == nativePage) return;
        NativePage previousNativePage = mNativePage;
        if (mNativePage != null && !(mNativePage instanceof FrozenNativePage)) {
            mNativePage.getView().removeOnAttachStateChangeListener(mAttachStateChangeListener);
        }
        mNativePage = nativePage;
        if (mNativePage != null && !(mNativePage instanceof FrozenNativePage)) {
            mNativePage.getView().addOnAttachStateChangeListener(mAttachStateChangeListener);
        }
        pushNativePageStateToNavigationEntry();
        // Notifying of theme color change before content change because some of
        // the observers depend on the theme information being correct in
        // onContentChanged().
        updateThemeColorIfNeeded(false);
        notifyContentChanged();
        destroyNativePageInternal(previousNativePage);
    }

    /**
     * Replaces the current NativePage with a empty stand-in for a NativePage. This can be used
     * to reduce memory pressure.
     */
    public void freezeNativePage() {
        if (mNativePage == null || mNativePage instanceof FrozenNativePage) return;
        assert mNativePage.getView().getParent() == null : "Cannot freeze visible native page";
        mNativePage = FrozenNativePage.freeze(mNativePage);
        updateInteractableState();
    }

    /**
     * Hides the current {@link NativePage}, if any, and shows the {@link ContentViewCore}'s view.
     */
    protected void showRenderedPage() {
        updateTitle();

        if (mNativePage == null) return;
        NativePage previousNativePage = mNativePage;
        if (!(mNativePage instanceof FrozenNativePage)) {
            mNativePage.getView().removeOnAttachStateChangeListener(mAttachStateChangeListener);
        }
        mNativePage = null;
        notifyContentChanged();
        destroyNativePageInternal(previousNativePage);
    }

    /**
     * Initializes {@link Tab} with {@code webContents}.  If {@code webContents} is {@code null} a
     * new {@link WebContents} will be created for this {@link Tab}.
     * @param webContents       A {@link WebContents} object or {@code null} if one should be
     *                          created.
     * @param tabContentManager A {@link TabContentManager} instance or {@code null} if the web
     *                          content will be managed/displayed manually.
     * @param delegateFactory   The {@link TabDelegateFactory} to be used for delegate creation.
     * @param initiallyHidden   Only used if {@code webContents} is {@code null}.  Determines
     *                          whether or not the newly created {@link WebContents} will be hidden
     *                          or not.
     * @param unfreeze          Whether there should be an attempt to restore state at the end of
     *                          the initialization.
     */
    public final void initialize(WebContents webContents, TabContentManager tabContentManager,
            TabDelegateFactory delegateFactory, boolean initiallyHidden, boolean unfreeze) {
        try {
            TraceEvent.begin("Tab.initialize");

            mDelegateFactory = delegateFactory;
            initializeNative();

            RevenueStats.getInstance().tabCreated(this);

            mBrowserControlsVisibilityDelegate =
                    mDelegateFactory.createBrowserControlsVisibilityDelegate(this);

            // Attach the TabContentManager if we have one.  This will bind this Tab's content layer
            // to this manager.
            // TODO(dtrainor): Remove this and move to a pull model instead of pushing the layer.
            attachTabContentManager(tabContentManager);

            // If there is a frozen WebContents state or a pending lazy load, don't create a new
            // WebContents.
            if (getFrozenContentsState() != null || getPendingLoadParams() != null) {
                if (unfreeze) unfreezeContents();
                return;
            }

            boolean creatingWebContents = webContents == null;
            if (creatingWebContents) {
                webContents = WarmupManager.getInstance().takeSpareWebContents(
                        isIncognito(), initiallyHidden);
                if (webContents == null) {
                    webContents =
                            WebContentsFactory.createWebContents(isIncognito(), initiallyHidden);
                }
            }

            ContentViewCore contentViewCore = ContentViewCore.fromWebContents(webContents);

            if (contentViewCore == null) {
                initContentViewCore(webContents);
            } else {
                setContentViewCore(contentViewCore);
            }

            if (!creatingWebContents && webContents.isLoadingToDifferentDocument()) {
                didStartPageLoad(webContents.getVisibleUrl(), false);
            }

        } finally {
            if (mTimestampMillis == INVALID_TIMESTAMP) {
                mTimestampMillis = System.currentTimeMillis();
            }

            TraceEvent.end("Tab.initialize");
        }
    }

    /**
     * Reparents this Tab to the provided Activity. Unlike {@link #detachAndStartReparenting} which
     * launches the target Activity which can then reparent the Tab with
     * {@link #attachAndFinishReparenting}, this method should be called from the target Activity on
     * a Tab that belongs to a different one.
     * @param activity - The ChromeActivity that will own the Tab.
     * @param tabDelegateFactory - The TabDelegateFactory from the Activity for that Tab.
     */
    public void reparent(ChromeActivity activity, TabDelegateFactory tabDelegateFactory) {
        detach();
        // TODO(peconn): Figure out why this is necessary - it is something to do with
        // Tab.mIsDetached being true and TabModelSelectorImpl#requestToShowTab not calling
        // |mVisibleTab.hide()| because of it.
        hide();
        attachAndFinishReparenting(activity, tabDelegateFactory, null);
    }

    /**
     * Begins the tab reparenting process. Detaches the tab from its current activity and fires
     * an Intent to reparent the tab into its new host activity.
     *
     * @param intent An optional intent with the desired component, flags, or extras to use when
     *               launching the new host activity. This intent's URI and action will be
     *               overriden. This may be null if no intent customization is needed.
     * @param startActivityOptions Options to pass to {@link Activity#startActivity(Intent, Bundle)}
     * @param finalizeCallback A callback that will be called after the tab is attached to the new
     *                         host activity in {@link #attachAndFinishReparenting}.
     * @return Whether reparenting succeeded. If false, the tab was not removed and the intent was
     *         not fired.
     */
    public boolean detachAndStartReparenting(Intent intent, Bundle startActivityOptions,
            Runnable finalizeCallback) {
        ChromeActivity activity = getActivity();
        if (activity == null) return false;

        if (intent == null) intent = new Intent();
        if (intent.getComponent() == null) {
            intent.setClass(mThemedApplicationContext, ChromeLauncherActivity.class);
        }
        intent.setAction(Intent.ACTION_VIEW);
        if (TextUtils.isEmpty(intent.getDataString())) intent.setData(Uri.parse(getUrl()));
        if (isIncognito()) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        }
        IntentHandler.addTrustedIntentExtras(intent);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_REPARENTING)) {
            // Add the tab to AsyncTabParamsManager before removing it from the current model to
            // ensure the global count of tabs is correct. See https://crbug.com/611806.
            intent.putExtra(IntentHandler.EXTRA_TAB_ID, mId);
            AsyncTabParamsManager.add(
                    mId, new TabReparentingParams(this, intent, finalizeCallback));

            detach();
        }

        activity.startActivity(intent, startActivityOptions);
        return true;
    }

    /**
     * Detaches a tab from its current activity if any.
     *
     * In details, this function:
     * - Tags the tab using mIsDetached.
     * - Removes the tab from its current {@link TabModelSelector}, effectively severing
     *   the {@link Activity} to {@link Tab} link.
     */
    private void detach() {
        mIsDetached = true;

        TabModelSelector tabModelSelector = getTabModelSelector();
        if (tabModelSelector != null) {
            tabModelSelector.getModel(mIncognito).removeTab(this);
        }
        // TODO(yusufo): We can't call updateWindowAndroid here and set mWindowAndroid to null
        // because many code paths (including navigation) expect the tab to always be associated
        // with an activity, and will crash. crbug.com/657007
        if (mContentViewCore != null) mContentViewCore.updateWindowAndroid(null);
        attachTabContentManager(null);

        for (TabObserver observer : mObservers) {
            observer.onActivityAttachmentChanged(this, false);
        }
    }

    /**
     * Finishes the tab reparenting process. Attaches the tab to the new activity, and updates the
     * tab and related objects to reference the new activity. This updates many delegates inside the
     * tab and {@link ContentViewCore} both on java and native sides.
     *
     * @param activity The new activity this tab should be associated with.
     * @param tabDelegateFactory The new delegate factory this tab should be using.
     * @param finalizeCallback A Callback to be called after the Tab has been reparented.
     */
    public void attachAndFinishReparenting(ChromeActivity activity,
            TabDelegateFactory tabDelegateFactory,
            @Nullable Runnable finalizeCallback) {
        // TODO(yusufo): Share these calls with the construction related calls.
        // crbug.com/590281
        activity.getCompositorViewHolder().prepareForTabReparenting();

        attach(activity, tabDelegateFactory);

        mIsTabStateDirty = true;

        if (finalizeCallback != null) finalizeCallback.run();
    }

    /**
     * Attaches the tab to the new activity and updates the tab and related objects to reference the
     * new activity. This updates many delegates inside the tab and {@link ContentViewCore} both on
     * java and native sides.
     * TODO(ltian:) explore calling this for all types of tabs.
     *
     * @param activity  The new activity this tab should be associated with.
     * @param tabDelegateFactory  The new delegate factory this tab should be using.
     */
    public void attach(ChromeActivity activity, TabDelegateFactory tabDelegateFactory) {
        assert mIsDetached;
        updateWindowAndroid(activity.getWindowAndroid());
        mDefaultThemeColor = calculateDefaultThemeColor();
        updateThemeColorIfNeeded(false);

        // Update for the controllers that need the Compositor from the new Activity.
        attachTabContentManager(activity.getTabContentManager());
        mFullscreenManager = activity.getFullscreenManager();
        // Update the delegate factory, then recreate and propagate all delegates.
        mDelegateFactory = tabDelegateFactory;
        mWebContentsDelegate = mDelegateFactory.createWebContentsDelegate(this);
        mBrowserControlsVisibilityDelegate =
                mDelegateFactory.createBrowserControlsVisibilityDelegate(this);

        mIsDetached = false;

        // Reload the NativePage (if any), since the old NativePage has a reference to the old
        // activity.
        maybeShowNativePage(getUrl(), true);

        nativeAttachDetachedTab(mNativeTabAndroid);

        if (getWebContents() != null) {
            nativeUpdateDelegates(mNativeTabAndroid, mWebContentsDelegate,
                    mDelegateFactory.createContextMenuPopulator(this));
            setInterceptNavigationDelegate(
                    mDelegateFactory.createInterceptNavigationDelegate(this));
            getAppBannerManager().setIsEnabledForTab(mDelegateFactory.canShowAppBanners(this));
        }

        for (TabObserver observer : mObservers) {
            observer.onActivityAttachmentChanged(this, true);
        }
    }

    /**
     * Update and propagate the new WindowAndroid.
     * @param windowAndroid The WindowAndroid to propagate.
     */
    public void updateWindowAndroid(WindowAndroid windowAndroid) {
        // TODO(yusufo): mWindowAndroid can never be null until crbug.com/657007 is fixed.
        assert windowAndroid != null;
        mWindowAndroid = windowAndroid;
        if (mContentViewCore != null) mContentViewCore.updateWindowAndroid(mWindowAndroid);
    }

    /**
     * @return Whether the tab is detached from any Activity and its {@link WindowAndroid}.
     * Certain functionalities will not work until it is attached to an activity
     * with {@link Tab#attachAndFinishReparenting}.
     */
    public boolean isDetached() {
        return mIsDetached;
    }

    /**
     * Attach the content layer for this tab to the given {@link TabContentManager}.
     * @param tabContentManager {@link TabContentManager} to attach to.
     */
    public void attachTabContentManager(TabContentManager tabContentManager) {
        if (mNativeTabAndroid == 0) return;
        nativeAttachToTabContentManager(mNativeTabAndroid, tabContentManager);
    }

    /**
     * @return The delegate factory for testing purposes only.
     */
    public TabDelegateFactory getDelegateFactory() {
        return mDelegateFactory;
    }

    public View getContentView() {
        if (!isNativePage()) {
            return getView();
        } else if (mContentViewCore != null) {
            return mContentViewCore.getContainerView();
        }
        return null;
    }

    /**
     * Called when a navigation begins and no navigation was in progress
     * @param toDifferentDocument Whether this navigation will transition between
     * documents (i.e., not a fragment navigation or JS History API call).
     */
    protected void onLoadStarted(boolean toDifferentDocument) {
        if (toDifferentDocument) mIsLoading = true;
        for (TabObserver observer : mObservers) observer.onLoadStarted(this, toDifferentDocument);
    }

    /**
     * Called when a navigation completes and no other navigation is in progress.
     */
    protected void onLoadStopped() {
        // mIsLoading should only be false if this is a same-document navigation.
        boolean toDifferentDocument = mIsLoading;
        mIsLoading = false;
        for (TabObserver observer : mObservers) observer.onLoadStopped(this, toDifferentDocument);
    }

    /**
     * Called when a page has started loading.
     * @param validatedUrl URL being loaded.
     * @param showingErrorPage Whether an error page is being shown.
     */
    protected void didStartPageLoad(String validatedUrl, boolean showingErrorPage) {
        updateTitle();
        removeSadTabIfPresent();

        mDataSavedOnStartPageLoad =
                DataReductionProxySettings.getInstance().getContentLengthSavedInHistorySummary();

        if (mIsRendererUnresponsive) handleRendererResponsive();

        if (mTabUma != null) mTabUma.onPageLoadStarted();

        for (TabObserver observer : mObservers) observer.onPageLoadStarted(this, validatedUrl);
    }

    /**
     * Called when a page has finished loading.
     */
    protected void didFinishPageLoad() {
        mIsTabStateDirty = true;
        updateTitle();
        updateFullscreenEnabledState();
        if (!isNativePage()) {
            RecordHistogram.recordBooleanHistogram(
                    "Navigation.IsMobileOptimized", mContentViewCore.getIsMobileOptimizedHint());
        }

        // Reset the succressiveRefresh counter after successfully loading a page.
        mSadTabSuccessiveRefreshCounter = 0;

        if (mTabUma != null) mTabUma.onPageLoadFinished();

        for (TabObserver observer : mObservers) observer.onPageLoadFinished(this);
        mIsBeingRestored = false;

        long dataSaved =
                DataReductionProxySettings.getInstance().getContentLengthSavedInHistorySummary()
                - mDataSavedOnStartPageLoad;

        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
        if (dataSaved > 0L) {
            tracker.notifyEvent(EventConstants.DATA_SAVED_ON_PAGE_LOAD);
        }

        // Don't show data saver footer if Chrome Home is enabled (temporary fix for M61)
        if (FeatureUtilities.isChromeHomeEnabled()) return;

        maybeShowDataSaverInProductHelp(tracker);
    }

    private void maybeShowDataSaverInProductHelp(final Tracker tracker) {
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.DATA_SAVER_DETAIL_FEATURE)) return;

        if (!(getActivity() instanceof ChromeTabbedActivity)) return;

        View anchorView = getActivity().getToolbarManager().getMenuButton();
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);
        TextBubble textBubble =
                new TextBubble(getActivity(), anchorView, R.string.iph_data_saver_detail_text,
                        R.string.iph_data_saver_detail_accessibility_text, rectProvider);
        textBubble.setDismissOnTouchInteraction(true);
        getActivity().getAppMenuHandler().setMenuHighlight(R.id.app_menu_footer);
        textBubble.addOnDismissListener(new OnDismissListener() {
            @Override
            public void onDismiss() {
                ThreadUtils.postOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        tracker.dismissed(FeatureConstants.DATA_SAVER_DETAIL_FEATURE);
                        getActivity().getAppMenuHandler().setMenuHighlight(null);
                    }
                });
            }
        });
        int yInsetPx = mThemedApplicationContext.getResources().getDimensionPixelOffset(
                R.dimen.text_bubble_menu_anchor_y_inset);
        rectProvider.setInsetPx(0, FeatureUtilities.isChromeHomeEnabled() ? yInsetPx : 0, 0,
                FeatureUtilities.isChromeHomeEnabled() ? 0 : yInsetPx);
        textBubble.show();
    }

    /**
     * Called when a page has failed loading.
     * @param errorCode The error code causing the page to fail loading.
     */
    protected void didFailPageLoad(int errorCode) {
        if (mTabUma != null) mTabUma.onLoadFailed(errorCode);
        for (TabObserver observer : mObservers) {
            observer.onPageLoadFailed(this, errorCode);
        }
        mIsBeingRestored = false;
    }

    /**
     * Builds the native counterpart to this class.  Meant to be overridden by subclasses to build
     * subclass native counterparts instead.  Subclasses should not call this via super and instead
     * rely on the native class to create the JNI association.
     *
     * TODO(dfalcantara): Make this function harder to access.
     */
    public void initializeNative() {
        if (mNativeTabAndroid == 0) nativeInit();
        assert mNativeTabAndroid != 0;
        mIsInitialized = true;
    }

    /**
     * Creates and initializes the {@link ContentViewCore}.
     *
     * @param webContents The WebContents object that will be used to build the
     *                    {@link ContentViewCore}.
     */
    protected void initContentViewCore(WebContents webContents) {
        setContentViewCore(createContentViewCore(webContents));
    }

    private ContentViewCore createContentViewCore(WebContents webContents) {
        ContentViewCore cvc = ContentViewCore.create(mThemedApplicationContext, PRODUCT_VERSION);
        ContentView cv = ContentView.createContentView(mThemedApplicationContext, cvc);
        cv.setContentDescription(mThemedApplicationContext.getResources().getString(
                R.string.accessibility_content_view));
        cvc.initialize(new TabViewAndroidDelegate(this, cv), cv, webContents, getWindowAndroid());
        SelectionPopupController controller = SelectionPopupController.fromWebContents(webContents);
        ChromeActionModeCallback actionModeCallback =
                new ChromeActionModeCallback(this, controller.getActionModeCallbackHelper());
        controller.setActionModeCallback(actionModeCallback);
        return cvc;
    }

    /**
     * Completes the {@link ContentViewCore} specific initialization around a native WebContents
     * pointer. {@link #getNativePage()} will still return the {@link NativePage} if there is one.
     * All initialization that needs to reoccur after a web contents swap should be added here.
     * <p />
     * NOTE: If you attempt to pass a native WebContents that does not have the same incognito
     * state as this tab this call will fail.
     *
     * @param cvc The content view core that needs to be set as active view for the tab.
     */
    private void setContentViewCore(ContentViewCore cvc) {
        try {
            TraceEvent.begin("ChromeTab.setContentViewCore");
            NativePage previousNativePage = mNativePage;
            mNativePage = null;
            destroyNativePageInternal(previousNativePage);

            WebContents oldWebContents = getWebContents();
            if (oldWebContents != null) {
                oldWebContents.setImportance(ChildProcessImportance.NORMAL);
                getWebContentsAccessibility(oldWebContents).setObscuredByAnotherView(false);
            }

            mContentViewCore = cvc;
            mContentViewCore.getWebContents().setImportance(mImportance);
            cvc.getContainerView().setOnHierarchyChangeListener(this);
            cvc.getContainerView().setOnSystemUiVisibilityChangeListener(this);

            mContentView = cvc.getContainerView();
            mContentView.addOnAttachStateChangeListener(mAttachStateChangeListener);
            updateInteractableState();
            mWebContentsDelegate = mDelegateFactory.createWebContentsDelegate(this);
            mWebContentsObserver =
                    new TabWebContentsObserver(mContentViewCore.getWebContents(), this);
            if (UmaUtils.isRunningApplicationStart()) {
                mStartupPageLoadMetricsObserver = new StartupPageLoadMetricsObserver();
                PageLoadMetrics.addObserver(mStartupPageLoadMetricsObserver);
            }

            mDownloadDelegate = new ChromeDownloadDelegate(mThemedApplicationContext, this);

            initWebContents(mContentViewCore.getWebContents());

            // In the case where restoring a Tab or showing a prerendered one we already have a
            // valid infobar container, no need to recreate one.
            if (mInfoBarContainer == null) {
                WindowAndroid windowAndroid = getWindowAndroid();
                Activity activity =
                        windowAndroid == null ? null : windowAndroid.getActivity().get();
                ViewGroup bottomContainer = activity == null ? null
                        : (ViewGroup) activity.findViewById(R.id.bottom_container);
                // The InfoBarContainer needs to be created after the ContentView has been natively
                // initialized.
                mInfoBarContainer = new InfoBarContainer(mThemedApplicationContext, bottomContainer,
                        this);
            }
            mInfoBarContainer.setWebContents(getWebContents());

            mSwipeRefreshHandler = new SwipeRefreshHandler(mThemedApplicationContext, this);

            updateThemeColorIfNeeded(false);
            notifyContentChanged();

            // For browser tabs, we want to set accessibility focus to the page
            // when it loads. This is not the default behavior for embedded
            // web views.
            getWebContentsAccessibility(getWebContents()).setShouldFocusOnPageLoad(true);

            ImeAdapter.fromWebContents(mContentViewCore.getWebContents())
                    .addEventObserver(new ImeEventObserver() {
                        @Override
                        public void onImeEvent() {
                            // Some text was set in the page. Don't reuse it if a tab is
                            // open from the same external application, we might lose some
                            // user data.
                            mAppAssociatedWith = null;
                        }

                        @Override
                        public void onNodeAttributeUpdated(boolean editable, boolean password) {
                            if (getFullscreenManager() == null) return;
                            updateFullscreenEnabledState();
                        }
                    });

            setInterceptNavigationDelegate(mDelegateFactory.createInterceptNavigationDelegate(
                    this));

            getAppBannerManager().setIsEnabledForTab(mDelegateFactory.canShowAppBanners(this));
        } finally {
            TraceEvent.end("ChromeTab.setContentViewCore");
        }
    }

    private void initWebContents(WebContents webContents) {
        assert mNativeTabAndroid != 0;
        nativeInitWebContents(mNativeTabAndroid, mIncognito, mIsDetached, webContents,
                mWebContentsDelegate,
                new TabContextMenuPopulator(
                        mDelegateFactory.createContextMenuPopulator(this), this));

        if (mGestureStateListener == null) {
            mGestureStateListener = createGestureStateListener();
        }
        GestureListenerManager.fromWebContents(webContents).addListener(mGestureStateListener);
    }

    private static WebContentsAccessibility getWebContentsAccessibility(WebContents webContents) {
        return webContents != null ? WebContentsAccessibility.fromWebContents(webContents) : null;
    }

    /**
     * Shows a native page for url if it's a valid chrome-native URL. Otherwise, does nothing.
     * @param url The url of the current navigation.
     * @param forceReload If true, the current native page (if any) will not be reused, even if it
     *                    matches the URL.
     * @return True, if a native page was displayed for url.
     */
    boolean maybeShowNativePage(String url, boolean forceReload) {
        // While detached for reparenting we don't have an owning Activity, or TabModelSelector,
        // so we can't create the native page. The native page will be created once reparenting is
        // completed.
        if (mIsDetached) return false;
        NativePage candidateForReuse = forceReload ? null : getNativePage();
        NativePage nativePage = NativePageFactory.createNativePageForURL(url, candidateForReuse,
                this, getTabModelSelector(), getActivity());
        if (nativePage != null) {
            showNativePage(nativePage);
            notifyPageTitleChanged();
            notifyFaviconChanged();
            return true;
        }
        return false;
    }

    /**
     * Update internal Tab state when provisional load gets committed.
     * @param url The URL that was loaded.
     * @param transitionType The transition type to the current URL.
     */
    void handleDidFinishNavigation(String url, Integer transitionType) {
        mIsNativePageCommitPending = false;
        boolean isReload = (transitionType != null
                && (transitionType & PageTransition.CORE_MASK) == PageTransition.RELOAD);
        if (!maybeShowNativePage(url, isReload)) {
            showRenderedPage();
        }

        if (getInterceptNavigationDelegate() != null) {
            getInterceptNavigationDelegate().maybeUpdateNavigationHistory();
        }
    }

    /**
     * Constructs and shows a sad tab (Aw, Snap!).
     */
    protected void showSadTab() {
        if (getContentViewCore() != null) {
            // If the tab has crashed twice in a row change the sad tab view to the "Send Feedback"
            // version and change the onClickListener.
            final boolean showSendFeedbackView = mSadTabSuccessiveRefreshCounter >= 1;

            Runnable suggestionAction = new Runnable() {
                @Override
                public void run() {
                    Activity activity = mWindowAndroid.getActivity().get();
                    assert activity != null;
                    HelpAndFeedback.getInstance(activity).show(activity,
                            activity.getString(R.string.help_context_sad_tab),
                            Profile.getLastUsedProfile(), null);
                }
            };

            Runnable buttonAction = new Runnable() {
                @Override
                public void run() {
                    if (showSendFeedbackView) {
                        getActivity().startHelpAndFeedback(
                                getUrl(), "MobileSadTabFeedback", getProfile());
                    } else {
                        reload();
                    }
                }
            };

            // Make sure we are not adding the "Aw, snap" view over an existing one.
            assert mSadTabView == null;

            mSadTabView = SadTabViewFactory.createSadTabView(mThemedApplicationContext,
                    suggestionAction, buttonAction, showSendFeedbackView, mIncognito);
            mSadTabSuccessiveRefreshCounter++;
            // Show the sad tab inside ContentView.
            getContentViewCore().getContainerView().addView(
                    mSadTabView, new FrameLayout.LayoutParams(
                            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
            notifyContentChanged();
        }
        FullscreenManager fullscreenManager = getFullscreenManager();
        if (fullscreenManager != null) {
            fullscreenManager.setPositionsForTabToNonFullscreen();
        }
    }

    /**
     * Removes the sad tab view if present.
     */
    private void removeSadTabIfPresent() {
        if (isShowingSadTab()) {
            getContentViewCore().getContainerView().removeView(mSadTabView);
            notifyContentChanged();
        }
        mSadTabView = null;
    }

    /**
     * Removes any existing sad tab view and shows it again. This "reloads" the tab without
     * going through any formal loading logic.
     */
    @VisibleForTesting
    public void reloadSadTabForTesting() {
        removeSadTabIfPresent();
        showSadTab();
    }

    /**
     * @return The ID of the renderer process that backs this tab or
     *         {@link #INVALID_RENDER_PROCESS_PID} if there is none.
     */
    @VisibleForTesting
    public int getCurrentRenderProcessIdForTesting() {
        assert mNativeTabAndroid != 0;
        return nativeGetCurrentRenderProcessId(mNativeTabAndroid);
    }

    /**
     * @return Whether or not the sad tab is showing.
     */
    public boolean isShowingSadTab() {
        return mSadTabView != null && getContentViewCore() != null
                && mSadTabView.getParent() == getContentViewCore().getContainerView();
    }

    /**
     * Calls onContentChanged on all TabObservers and updates accessibility visibility.
     */
    private void notifyContentChanged() {
        for (TabObserver observer : mObservers) observer.onContentChanged(this);
        updateAccessibilityVisibility();
    }

    /**
     * Cleans up all internal state, destroying any {@link NativePage} or {@link ContentViewCore}
     * currently associated with this {@link Tab}.  This also destroys the native counterpart
     * to this class, which means that all subclasses should erase their native pointers after
     * this method is called.  Once this call is made this {@link Tab} should no longer be used.
     */
    public void destroy() {
        mIsInitialized = false;
        // Update the title before destroying the tab. http://b/5783092
        updateTitle();

        if (mTabUma != null) mTabUma.onDestroy();

        for (TabObserver observer : mObservers) observer.onDestroyed(this);
        mObservers.clear();

        hideMediaDownloadInProductHelp();

        NativePage currentNativePage = mNativePage;
        mNativePage = null;
        destroyNativePageInternal(currentNativePage);
        destroyContentViewCore(true);

        // Destroys the native tab after destroying the ContentView but before destroying the
        // InfoBarContainer. The native tab should be destroyed before the infobar container as
        // destroying the native tab cleanups up any remaining infobars. The infobar container
        // expects all infobars to be cleaned up before its own destruction.
        assert mNativeTabAndroid != 0;
        nativeDestroy(mNativeTabAndroid);
        assert mNativeTabAndroid == 0;

        if (mInfoBarContainer != null) {
            mInfoBarContainer.destroy();
            mInfoBarContainer = null;
        }

        mPreviousTopControlsOffsetY = Float.NaN;
        mPreviousBottomControlsOffsetY = Float.NaN;
        mPreviousContentOffsetY = Float.NaN;

        mNeedsReload = false;
    }

    /**
     * @return Whether or not this Tab has a live native component.  This will be true prior to
     *         {@link #initializeNative()} being called or after {@link #destroy()}.
     */
    public boolean isInitialized() {
        return mIsInitialized;
    }

    /**
     * @return The URL that is currently visible in the location bar. This may not be the same as
     *         the last committed URL if a new navigation is in progress.
     */
    @CalledByNative
    public String getUrl() {
        String url = getWebContents() != null ? getWebContents().getVisibleUrl() : "";

        // If we have a ContentView, or a NativePage, or the url is not empty, we have a WebContents
        // so cache the WebContent's url. If not use the cached version.
        if (getContentViewCore() != null || getNativePage() != null || !TextUtils.isEmpty(url)) {
            mUrl = url;
        }

        return mUrl != null ? mUrl : "";
    }

    /**
     * @return The tab title.
     */
    @CalledByNative
    public String getTitle() {
        if (mTitle == null) updateTitle();
        return mTitle;
    }

    void updateTitle() {
        if (isFrozen()) return;

        // When restoring the tabs, the title will no longer be populated, so request it from the
        // ContentViewCore or NativePage (if present).
        String title = "";
        if (mNativePage != null) {
            title = mNativePage.getTitle();
        } else if (getWebContents() != null) {
            title = getWebContents().getTitle();
        }
        updateTitle(title);
    }

    /**
     * Cache the title for the current page.
     *
     * {@link ContentViewClient#onUpdateTitle} is unreliable, particularly for navigating backwards
     * and forwards in the history stack, so pull the correct title whenever the page changes.
     * onUpdateTitle is only called when the title of a navigation entry changes. When the user goes
     * back a page the navigation entry exists with the correct title, thus the title is not
     * actually changed, and no notification is sent.
     * @param title Title of the page.
     */
    void updateTitle(String title) {
        if (TextUtils.equals(mTitle, title)) return;

        mIsTabStateDirty = true;
        mTitle = title;
        mIsTitleDirectionRtl = LocalizationUtils.getFirstStrongCharacterDirection(title)
                == LocalizationUtils.RIGHT_TO_LEFT;
        notifyPageTitleChanged();
    }

    private void notifyPageTitleChanged() {
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            observers.next().onTitleUpdated(this);
        }
    }

    /**
     * Notify the observers that the load progress has changed.
     * @param progress The current percentage of progress.
     */
    protected void notifyLoadProgress(int progress) {
        for (TabObserver observer : mObservers) observer.onLoadProgressChanged(Tab.this, progress);
    }

    private void notifyFaviconChanged() {
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            observers.next().onFaviconUpdated(this, null);
        }
    }

    /**
     * @return True if the tab title should be displayed from right to left.
     */
    public boolean isTitleDirectionRtl() {
        return mIsTitleDirectionRtl;
    }

    /**
     * @return The bitmap of the favicon scaled to 16x16dp. null if no favicon
     *         is specified or it requires the default favicon.
     */
    public Bitmap getFavicon() {
        // If we have no content or a native page, return null.
        if (getContentViewCore() == null) return null;

        // Use the cached favicon only if the page wasn't changed.
        if (mFavicon != null && mFaviconUrl != null && mFaviconUrl.equals(getUrl())) {
            return mFavicon;
        }

        return nativeGetFavicon(mNativeTabAndroid);
    }

    /**
     * Loads the tab if it's not loaded (e.g. because it was killed in background).
     * This will trigger a regular load for tabs with pending lazy first load (tabs opened in
     * background on low-memory devices).
     * @return true iff the Tab handled the request.
     */
    @CalledByNative
    public boolean loadIfNeeded() {
        if (getActivity() == null) {
            Log.e(TAG, "Tab couldn't be loaded because Context was null.");
            return false;
        }

        if (mPendingLoadParams != null) {
            assert isFrozen();
            initContentViewCore(WebContentsFactory.createWebContents(isIncognito(), isHidden()));
            loadUrl(mPendingLoadParams);
            mPendingLoadParams = null;
            return true;
        }

        restoreIfNeeded();
        return true;
    }

    /**
     * Loads a tab that was already loaded but since then was lost. This happens either when we
     * unfreeze the tab from serialized state or when we reload a tab that crashed. In both cases
     * the load codepath is the same (run in loadIfNecessary()) and the same caching policies of
     * history load are used.
     */
    private final void restoreIfNeeded() {
        try {
            TraceEvent.begin("Tab.restoreIfNeeded");
            if (isFrozen() && mFrozenContentsState != null) {
                // Restore is needed for a tab that is loaded for the first time. WebContents will
                // be restored from a saved state.
                unfreezeContents();
            } else if (mNeedsReload) {
                // Restore is needed for a tab that was previously loaded, but its renderer was
                // killed by the oom killer.
                mNeedsReload = false;
                requestRestoreLoad();
            } else {
                // No restore needed.
                return;
            }

            loadIfNecessary();
            mIsBeingRestored = true;
            if (mTabUma != null) mTabUma.onRestoreStarted();
        } finally {
            TraceEvent.end("Tab.restoreIfNeeded");
        }
    }

    /**
     * Issues a fake notification about the renderer being killed.
     *
     * @param wasOomProtected True if the renderer was protected from the OS out-of-memory killer
     *                        (e.g. renderer for the currently selected tab)
     */
    @VisibleForTesting
    public void simulateRendererKilledForTesting(boolean wasOomProtected) {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.renderProcessGone(wasOomProtected);
        }
    }

    /**
     * Restores the WebContents from its saved state.  This should only be called if the tab is
     * frozen with a saved TabState, and NOT if it was frozen for a lazy load.
     * @return Whether or not the restoration was successful.
     */
    protected boolean unfreezeContents() {
        try {
            TraceEvent.begin("Tab.unfreezeContents");
            assert mFrozenContentsState != null;
            assert getContentViewCore() == null;

            WebContents webContents =
                    mFrozenContentsState.restoreContentsFromByteBuffer(isHidden());
            if (webContents == null) {
                // State restore failed, just create a new empty web contents as that is the best
                // that can be done at this point. TODO(jcivelli) http://b/5910521 - we should show
                // an error page instead of a blank page in that case (and the last loaded URL).
                webContents = WebContentsFactory.createWebContents(isIncognito(), isHidden());
                mTabUma = new TabUma(TabCreationState.FROZEN_ON_RESTORE_FAILED);
                mFailedToRestore = true;
            }
            View compositorView = getActivity().getCompositorViewHolder();
            webContents.setSize(compositorView.getWidth(), compositorView.getHeight());

            mFrozenContentsState = null;
            initContentViewCore(webContents);

            if (mFailedToRestore) {
                String url = TextUtils.isEmpty(mUrl) ? UrlConstants.NTP_URL : mUrl;
                loadUrl(new LoadUrlParams(url, PageTransition.GENERATED));
            }
            return !mFailedToRestore;
        } finally {
            TraceEvent.end("Tab.unfreezeContents");
        }
    }

    /**
     * @return Whether the unfreeze attempt from a saved tab state failed.
     */
    public boolean didFailToRestore() {
        return mFailedToRestore;
    }

    /**
     * @return Whether or not the tab is hidden.
     */
    public boolean isHidden() {
        return mIsHidden;
    }

    /**
     * @return Whether the tab can currently be interacted with by the user.  This requires the
     *         view owned by the Tab to be visible and in a state where the user can interact with
     *         it (i.e. not in something like the phone tab switcher).
     */
    @CalledByNative
    public boolean isUserInteractable() {
        return mInteractableState;
    }

    /**
     * Update the interactable state of the tab. If the state has changed, it will call the
     * {@link #onInteractableStateChanged(boolean)} method.
     */
    private void updateInteractableState() {
        boolean currentState =
                !mIsHidden && !isFrozen() && (mIsViewAttachedToWindow || VrShellDelegate.isInVr());

        if (currentState == mInteractableState) return;

        mInteractableState = currentState;
        onInteractableStateChanged(currentState);
    }

    /**
     * A notification that the interactability of this tab has changed.
     * @param interactable Whether the tab is interactable.
     */
    private void onInteractableStateChanged(boolean interactable) {
        for (TabObserver observer : mObservers) observer.onInteractabilityChanged(interactable);
    }

    /**
     * @return Whether or not the tab is in the closing process.
     */
    public boolean isClosing() {
        return mIsClosing;
    }

    /**
     * @param closing Whether or not the tab is in the closing process.
     */
    public void setClosing(boolean closing) {
        mIsClosing = closing;

        for (TabObserver observer : mObservers) observer.onClosingStateChanged(this, closing);
    }

    /**
     * @return Whether the Tab needs to be reloaded. {@see #mNeedsReload}
     */
    public boolean needsReload() {
        return mNeedsReload;
    }

    /**
     * Set whether the Tab needs to be reloaded. {@see #mNeedsReload}
     */
    protected void setNeedsReload(boolean needsReload) {
        mNeedsReload = needsReload;
    }

    /**
     * @return true iff the tab is loading and an interstitial page is not showing.
     */
    public boolean isLoading() {
        return mIsLoading && !isShowingInterstitialPage();
    }

    /**
     * @return true iff the tab is performing a restore page load.
     */
    public boolean isBeingRestored() {
        return mIsBeingRestored;
    }

    /**
     * @return The id of the tab that caused this tab to be opened.
     */
    public int getParentId() {
        return mParentId;
    }

    /**
     * @return Whether the tab should be grouped with its parent tab (true by default).
     */
    public boolean isGroupedWithParent() {
        return mGroupedWithParent;
    }

    /**
     * Sets whether the tab should be grouped with its parent tab.
     *
     * @param groupedWithParent The new value.
     * @see #isGroupedWithParent
     */
    public void setGroupedWithParent(boolean groupedWithParent) {
        mGroupedWithParent = groupedWithParent;
    }

    private void destroyNativePageInternal(NativePage nativePage) {
        if (nativePage == null) return;
        assert nativePage != mNativePage : "Attempting to destroy active page.";

        nativePage.destroy();
    }

    /**
     * Called when the background color for the content changes.
     * @param color The current for the background.
     */
    void onBackgroundColorChanged(int color) {
        for (TabObserver observer : mObservers) observer.onBackgroundColorChanged(this, color);
    }

    /**
     * Destroys the current {@link ContentViewCore}.
     * @param deleteNativeWebContents Whether or not to delete the native WebContents pointer.
     */
    private final void destroyContentViewCore(boolean deleteNativeWebContents) {
        if (mContentViewCore == null) return;

        mContentViewCore.getContainerView().setOnHierarchyChangeListener(null);
        mContentViewCore.getContainerView().setOnSystemUiVisibilityChangeListener(null);

        if (mInfoBarContainer != null && mInfoBarContainer.getParent() != null) {
            mInfoBarContainer.removeFromParentView();
            mInfoBarContainer.setWebContents(null);
        }
        if (mSwipeRefreshHandler != null) {
            mSwipeRefreshHandler.destroy();
            mSwipeRefreshHandler = null;
        }
        mContentView.removeOnAttachStateChangeListener(mAttachStateChangeListener);
        mContentView = null;
        updateInteractableState();

        if (mGestureStateListener != null) {
            GestureListenerManager manager =
                    GestureListenerManager.fromWebContents(mContentViewCore.getWebContents());
            manager.removeListener(mGestureStateListener);
        }

        mContentViewCore.destroy();
        mContentViewCore = null;

        mWebContentsDelegate = null;

        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }

        if (mStartupPageLoadMetricsObserver != null) {
            PageLoadMetrics.removeObserver(mStartupPageLoadMetricsObserver);
            mStartupPageLoadMetricsObserver = null;
        }

        assert mNativeTabAndroid != 0;
        nativeDestroyWebContents(mNativeTabAndroid, deleteNativeWebContents);
    }

    /**
     * @return The {@link WindowAndroid} associated with this {@link Tab}.
     */
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * @return The current {@link TabWebContentsDelegateAndroid} instance.
     */
    public TabWebContentsDelegateAndroid getTabWebContentsDelegateAndroid() {
        return mWebContentsDelegate;
    }

    @CalledByNative
    protected void onFaviconAvailable(Bitmap icon) {
        if (icon == null) return;
        String url = getUrl();
        boolean pageUrlChanged = !url.equals(mFaviconUrl);
        // This method will be called multiple times if the page has more than one favicon.
        // we are trying to use the 16x16 DP icon here, Bitmap.createScaledBitmap will return
        // the origin bitmap if it is already 16x16 DP.
        if (pageUrlChanged || (icon.getWidth() == mIdealFaviconSize
                && icon.getHeight() == mIdealFaviconSize)) {
            mFavicon = Bitmap.createScaledBitmap(icon, mIdealFaviconSize, mIdealFaviconSize, true);
            mFaviconUrl = url;
        }

        for (TabObserver observer : mObservers) observer.onFaviconUpdated(this, icon);
    }

    /**
     * Checks if this tab is currently presented in the context of custom tabs. Tabs can be moved
     * between different activities so the returned value might change over the lifetime of the tab.
     * @return true if this is currently a custom tab.
     */
    @CalledByNative
    public boolean isCurrentlyACustomTab() {
        ChromeActivity activity = getActivity();
        return activity != null && activity.isCustomTab();
    }

    /**
     * Called when the navigation entry containing the history item changed,
     * for example because of a scroll offset or form field change.
     */
    @CalledByNative
    private void onNavEntryChanged() {
        mIsTabStateDirty = true;
    }

    /**
     * Called when navigation entries were removed.
     */
    void notifyNavigationEntriesDeleted() {
        mIsTabStateDirty = true;
        for (TabObserver observer : mObservers) observer.onNavigationEntriesDeleted(this);
    }

    /**
     * Returns the SnackbarManager for the activity that owns this Tab, if any. May
     * return null.
     */
    public SnackbarManager getSnackbarManager() {
        if (getActivity() == null) return null;
        return getActivity().getSnackbarManager();
    }

    /**
     * @return The native pointer representing the native side of this {@link Tab} object.
     */
    @CalledByNative
    private long getNativePtr() {
        return mNativeTabAndroid;
    }

    /** This is currently called when committing a pre-rendered page. */
    @VisibleForTesting
    @CalledByNative
    public void swapWebContents(
            WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {
        ContentViewCore cvc = createContentViewCore(webContents);
        swapContentViewCore(cvc, false, didStartLoad, didFinishLoad);
    }

    private static Rect getEstimatedContentSize(Context context) {
        return ExternalPrerenderHandler.estimateContentSize((Application) context, false);
    }

    /**
     * Called to swap out the current view with the one passed in.
     *
     * @param newContentViewCore The content view that should be swapped into the tab.
     * @param deleteOldNativeWebContents Whether to delete the native web
     *         contents of old view.
     * @param didStartLoad Whether
     *         WebContentsObserver::DidStartProvisionalLoadForFrame() has
     *         already been called.
     * @param didFinishLoad Whether WebContentsObserver::DidFinishLoad() has
     *         already been called.
     */
    private void swapContentViewCore(ContentViewCore newContentViewCore,
            boolean deleteOldNativeWebContents, boolean didStartLoad, boolean didFinishLoad) {
        int originalWidth = 0;
        int originalHeight = 0;
        if (mContentViewCore != null) {
            originalWidth = mContentViewCore.getViewportWidthPix();
            originalHeight = mContentViewCore.getViewportHeightPix();
            mContentViewCore.onHide();
        }

        Rect bounds = new Rect();
        if (originalWidth == 0 && originalHeight == 0) {
            bounds = getEstimatedContentSize(getApplicationContext());
            originalWidth = bounds.right - bounds.left;
            originalHeight = bounds.bottom - bounds.top;
        }

        destroyContentViewCore(deleteOldNativeWebContents);
        NativePage previousNativePage = mNativePage;
        mNativePage = null;
        // Size of the new content is zero at this point. Set the view size in advance
        // so that next onShow() call won't send a resize message with zero size
        // to the renderer process. This prevents the size fluttering that may confuse
        // Blink and break rendered result (see http://crbug.com/340987).
        newContentViewCore.getWebContents().setSize(originalWidth, originalHeight);

        if (!bounds.isEmpty()) {
            nativeOnPhysicalBackingSizeChanged(mNativeTabAndroid,
                    newContentViewCore.getWebContents(), bounds.right, bounds.bottom);
        }
        newContentViewCore.onShow();
        setContentViewCore(newContentViewCore);

        destroyNativePageInternal(previousNativePage);
        for (TabObserver observer : mObservers) {
            observer.onWebContentsSwapped(this, didStartLoad, didFinishLoad);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        assert mNativeTabAndroid != 0;
        mNativeTabAndroid = 0;
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        assert mNativeTabAndroid == 0;
        mNativeTabAndroid = nativePtr;
    }

    /**
     * @return Whether the TabState representing this Tab has been updated.
     */
    public boolean isTabStateDirty() {
        return mIsTabStateDirty;
    }

    /**
     * Set whether the TabState representing this Tab has been updated.
     * @param isDirty Whether the Tab's state has changed.
     */
    public void setIsTabStateDirty(boolean isDirty) {
        mIsTabStateDirty = isDirty;
    }

    /**
     * @return Whether the Tab should be preserved in Android's Recents list when users hit "back".
     */
    public boolean shouldPreserve() {
        return mShouldPreserve;
    }

    /**
     * Sets whether the Tab should be preserved in Android's Recents list when users hit "back".
     * @param preserve Whether the tab should be preserved.
     */
    public void setShouldPreserve(boolean preserve) {
        mShouldPreserve = preserve;
    }

    /**
     * @return Whether there are pending {@link LoadUrlParams} associated with the tab.  This
     *         indicates the tab was created for lazy load.
     */
    public boolean hasPendingLoadParams() {
        return mPendingLoadParams != null;
    }

    /**
     * @return Parameters that should be used for a lazily loaded Tab.  May be null.
     */
    private LoadUrlParams getPendingLoadParams() {
        return mPendingLoadParams;
    }

    /**
     * @param params Parameters that should be used for a lazily loaded Tab.
     */
    private void setPendingLoadParams(LoadUrlParams params) {
        mPendingLoadParams = params;
        mUrl = params.getUrl();
    }

    /**
     * @see #setAppAssociatedWith(String) for more information.
     * TODO(aurimas): investigate reducing the visibility of this method after TabModel refactoring.
     *
     * @return The id of the application associated with that tab (null if not
     *         associated with an app).
     */
    public String getAppAssociatedWith() {
        return mAppAssociatedWith;
    }

    /**
     * Associates this tab with the external app with the specified id. Once a Tab is associated
     * with an app, it is reused when a new page is opened from that app (unless the user typed in
     * the location bar or in the page, in which case the tab is dissociated from any app)
     * TODO(aurimas): investigate reducing the visibility of this method after TabModel refactoring.
     *
     * @param appId The ID of application associated with the tab.
     */
    public void setAppAssociatedWith(String appId) {
        mAppAssociatedWith = appId;
    }

    /**
     * @return See {@link #mTimestampMillis}.
     */
    private long getTimestampMillis() {
        return mTimestampMillis;
    }

    /**
     * Restores a tab either frozen or from state.
     * TODO(aurimas): investigate reducing the visibility of this method after TabModel refactoring.
     */
    public void createHistoricalTab() {
        if (!isFrozen()) {
            nativeCreateHistoricalTab(mNativeTabAndroid);
        } else if (mFrozenContentsState != null) {
            mFrozenContentsState.createHistoricalTab();
        }
    }

    /**
     * @return The reason the Tab was launched.
     */
    public TabLaunchType getLaunchType() {
        return mLaunchType;
    }

    /**
     * @return true iff the tab doesn't hold a live page. This happens before initialize() and when
     * the tab holds frozen WebContents state that is yet to be inflated.
     */
    @VisibleForTesting
    public boolean isFrozen() {
        return getNativePage() == null && getContentViewCore() == null;
    }

    /**
     * @return An instance of a {@link FullscreenManager}.
     */
    public FullscreenManager getFullscreenManager() {
        return mFullscreenManager;
    }

    /**
     * Toggles fullscreen mode. If enabling fullscreen while the tab is not interactable, fullscreen
     * will be delayed until the tab is interactable.
     * @param enableFullscreen Whether fullscreen should be enabled.
     */
    public void toggleFullscreenMode(boolean enableFullscreen) {
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            observers.next().onToggleFullscreenMode(this, enableFullscreen);
        }
    }

    /**
     * Called when offset values related with fullscreen functionality has been changed by the
     * compositor.
     * @param topControlsOffsetY The Y offset of the top controls in physical pixels.
     *    {@code Float.NaN} if the value is invalid and the cached value should be used.
     * @param bottomControlsOffsetY The Y offset of the bottom controls in physical pixels.
     *    {@code Float.NaN} if the value is invalid and the cached value should be used.
     * @param contentOffsetY The Y offset of the content in physical pixels.
     */
    void onOffsetsChanged(
            float topControlsOffsetY, float bottomControlsOffsetY, float contentOffsetY) {
        if (!Float.isNaN(topControlsOffsetY)) mPreviousTopControlsOffsetY = topControlsOffsetY;
        if (!Float.isNaN(bottomControlsOffsetY)) {
            mPreviousBottomControlsOffsetY = bottomControlsOffsetY;
        }
        if (!Float.isNaN(contentOffsetY)) mPreviousContentOffsetY = contentOffsetY;

        if (mFullscreenManager == null) return;
        if (isShowingSadTab() || isNativePage()) {
            mFullscreenManager.setPositionsForTabToNonFullscreen();
        } else {
            mFullscreenManager.setPositionsForTab(mPreviousTopControlsOffsetY,
                    mPreviousBottomControlsOffsetY, mPreviousContentOffsetY);
        }
        TabModelImpl.setActualTabSwitchLatencyMetricRequired();
    }

    /**
     * Push state about whether or not the browser controls can show or hide to the renderer.
     */
    public void updateFullscreenEnabledState() {
        if (isFrozen()) return;

        int constraints = getBrowserControlsStateConstraints();

        updateBrowserControlsState(constraints, BrowserControlsState.BOTH,
                constraints != BrowserControlsState.HIDDEN);

        if (getContentViewCore() != null && mFullscreenManager != null) {
            getContentViewCore().updateMultiTouchZoomSupport(
                    !mFullscreenManager.getPersistentFullscreenMode());
        }
    }

    /**
     * Updates the browser controls state for this tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process.
     *
     * @param constraints The constraints that determine whether the controls can be shown
     *                    or hidden at all.
     * @param current The desired current state for the controls.  Pass
     *                {@link BrowserControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *                should jump immediately.
     */
    protected void updateBrowserControlsState(@BrowserControlsState int constraints,
            @BrowserControlsState int current, boolean animate) {
        if (mNativeTabAndroid == 0) return;
        nativeUpdateBrowserControlsState(mNativeTabAndroid, constraints, current, animate);
    }

    /**
     * Updates the browser controls state for this tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process.
     *
     * @param current The desired current state for the controls.  Pass
     *                {@link BrowserControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *                should jump immediately.
     */
    public void updateBrowserControlsState(int current, boolean animate) {
        int constraints = getBrowserControlsStateConstraints();
        // Do nothing if current and constraints conflict to avoid error in
        // renderer.
        if ((constraints == BrowserControlsState.HIDDEN && current == BrowserControlsState.SHOWN)
                || (constraints == BrowserControlsState.SHOWN
                           && current == BrowserControlsState.HIDDEN)) {
            return;
        }
        updateBrowserControlsState(getBrowserControlsStateConstraints(), current, animate);
    }

    /**
     * @return Whether hiding browser controls is enabled or not.
     */
    private boolean canAutoHideBrowserControls() {
        return mBrowserControlsVisibilityDelegate.canAutoHideBrowserControls();
    }

    /**
     * Performs any subclass-specific tasks when the Tab crashes.
     */
    void handleTabCrash() {
        mIsLoading = false;

        if (mTabUma != null) mTabUma.onRendererCrashed();

        boolean sadTabShown = isShowingSadTab();
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            observers.next().onCrash(this, sadTabShown);
        }
        mIsBeingRestored = false;
    }

    /**
     * @return Whether showing browser controls is enabled or not.
     */
    public boolean canShowBrowserControls() {
        return mBrowserControlsVisibilityDelegate.canShowBrowserControls();
    }

    /**
     * @return The current visibility constraints for the display of browser controls.
     *         {@link BrowserControlsState} defines the valid return options.
     */
    @BrowserControlsState
    public int getBrowserControlsStateConstraints() {
        int constraints = BrowserControlsState.BOTH;
        if (!canShowBrowserControls()) {
            constraints = BrowserControlsState.HIDDEN;
        } else if (!canAutoHideBrowserControls()) {
            constraints = BrowserControlsState.SHOWN;
        }
        return constraints;
    }

    public void setTopControlsHeight(int height, boolean controlsResizeView) {
        float scale = getWindowAndroid().getDisplay().getDipScale();
        mTopControlsHeight = (int) (height / scale);
        mControlsResizeView = controlsResizeView;
    }

    public void setBottomControlsHeight(int height) {
        float scale = getWindowAndroid().getDisplay().getDipScale();
        mBottomControlsHeight = (int) (height / scale);
    }

    int getTopControlsHeight() {
        return mTopControlsHeight;
    }

    int getBottomControlsHeight() {
        return mBottomControlsHeight;
    }

    boolean controlsResizeView() {
        return mControlsResizeView;
    }

    /**
     * @param manager The fullscreen manager that should be notified of changes to this tab (if
     *                set to null, no more updates will come from this tab).
     */
    public void setFullscreenManager(FullscreenManager manager) {
        mFullscreenManager = manager;
        if (mFullscreenManager != null) {
            boolean topOffsetsInitialized = !Float.isNaN(mPreviousTopControlsOffsetY)
                    && !Float.isNaN(mPreviousContentOffsetY);
            boolean bottomOffsetsInitialized =
                    !Float.isNaN(mPreviousBottomControlsOffsetY);
            boolean isChromeHomeEnabled = FeatureUtilities.isChromeHomeEnabled();

            // Make sure the dominant control offsets have been set.
            if ((!topOffsetsInitialized && !isChromeHomeEnabled)
                    || (!bottomOffsetsInitialized && isChromeHomeEnabled)) {
                mFullscreenManager.setPositionsForTabToNonFullscreen();
            } else {
                mFullscreenManager.setPositionsForTab(mPreviousTopControlsOffsetY,
                        mPreviousBottomControlsOffsetY,
                        mPreviousContentOffsetY);
            }
            updateFullscreenEnabledState();
        }
    }

    /**
     * Add a new navigation entry for the current URL and page title.
     */
    void pushNativePageStateToNavigationEntry() {
        assert mNativeTabAndroid != 0 && getNativePage() != null;
        nativeSetActiveNavigationEntryTitleForUrl(mNativeTabAndroid, getNativePage().getUrl(),
                getNativePage().getTitle());
    }

    @Override
    public void onChildViewRemoved(View parent, View child) {
        FullscreenManager fullscreenManager = getFullscreenManager();
        if (fullscreenManager != null) {
            fullscreenManager.updateContentViewChildrenState();
        }
    }

    @Override
    public void onChildViewAdded(View parent, View child) {
        FullscreenManager fullscreenManager = getFullscreenManager();
        if (fullscreenManager != null) {
            fullscreenManager.updateContentViewChildrenState();
        }
    }

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        FullscreenManager fullscreenManager = getFullscreenManager();
        if (fullscreenManager != null) {
            fullscreenManager.onContentViewSystemUiVisibilityChange(visibility);
        }
    }

    /**
     * @return The ID of the bookmark associated with the current URL, or
     *         {@link #INVALID_BOOKMARK_ID} if no such bookmark exists.
     */
    public long getBookmarkId() {
        return isFrozen() ? INVALID_BOOKMARK_ID : nativeGetBookmarkId(mNativeTabAndroid, false);
    }

    /**
     * Same as getBookmarkId() but never returns ids for managed bookmarks, or any other bookmarks
     * that can't be edited by the user.
     */
    public long getUserBookmarkId() {
        return isFrozen() ? INVALID_BOOKMARK_ID : nativeGetBookmarkId(mNativeTabAndroid, true);
    }

    /**
     * @return Original url of the tab, which is the original url from DOMDistiller.
     */
    public String getOriginalUrl() {
        return DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(getUrl());
    }

    /**
     * Request that this tab receive focus. Currently, this function requests focus for the main
     * View (usually a ContentView).
     */
    public void requestFocus() {
        View view = getView();
        if (view != null) view.requestFocus();
    }

    @CalledByNative
    protected void openNewTab(String url, String extraHeaders, ResourceRequestBody postData,
            int disposition, boolean hasParent, boolean isRendererInitiated) {
        if (isClosing()) return;

        boolean incognito = isIncognito();
        TabLaunchType tabLaunchType = TabLaunchType.FROM_LONGPRESS_FOREGROUND;
        Tab parentTab = hasParent ? this : null;

        switch (disposition) {
            case WindowOpenDisposition.NEW_WINDOW: // fall through
            case WindowOpenDisposition.NEW_FOREGROUND_TAB:
                break;
            case WindowOpenDisposition.NEW_POPUP: // fall through
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                tabLaunchType = TabLaunchType.FROM_LONGPRESS_BACKGROUND;
                break;
            case WindowOpenDisposition.OFF_THE_RECORD:
                assert incognito;
                incognito = true;
                break;
            default:
                assert false;
        }

        // If shouldIgnoreNewTab returns true, the intent is handled by another
        // activity. As a result, don't launch a new tab to open the URL. If TabModelSelector
        // is not accessible, then we can't open a new tab.
        if (shouldIgnoreNewTab(url, incognito) || getTabModelSelector() == null) return;

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setVerbatimHeaders(extraHeaders);
        loadUrlParams.setPostData(postData);
        loadUrlParams.setIsRendererInitiated(isRendererInitiated);
        getTabModelSelector().openNewTab(
                loadUrlParams, tabLaunchType, parentTab, incognito);
    }

    /**
     * @return True if the Tab should block the creation of new tabs via {@link #openNewTab}.
     */
    private boolean shouldIgnoreNewTab(String url, boolean incognito) {
        InterceptNavigationDelegateImpl delegate = getInterceptNavigationDelegate();
        return delegate != null && delegate.shouldIgnoreNewTab(url, incognito);
    }

    /**
     * See {@link #mInterceptNavigationDelegate}.
     */
    public InterceptNavigationDelegateImpl getInterceptNavigationDelegate() {
        return mInterceptNavigationDelegate;
    }

    @VisibleForTesting
    public AuthenticatorNavigationInterceptor getAuthenticatorHelper() {
        return getInterceptNavigationDelegate().getAuthenticatorNavigationInterceptor();
    }

    /**
     * See {@link #mInterceptNavigationDelegate}.
     */
    public void setInterceptNavigationDelegate(InterceptNavigationDelegateImpl delegate) {
        mInterceptNavigationDelegate = delegate;
        nativeSetInterceptNavigationDelegate(mNativeTabAndroid, delegate);
    }

    /**
     * @return the TabRedirectHandler for the tab.
     */
    public TabRedirectHandler getTabRedirectHandler() {
        return mTabRedirectHandler;
    }

    /**
     * Sets the TabRedirectHandler for the tab.
     *
     * @param tabRedirectHandler the TabRedirectHandler
     */
    public void setTabRedirectHandler(TabRedirectHandler tabRedirectHandler) {
        mTabRedirectHandler = tabRedirectHandler;
    }

    /**
     * @return the AppBannerManager.
     */
    public AppBannerManager getAppBannerManager() {
        return AppBannerManager.getAppBannerManagerForWebContents(getWebContents());
    }

    @VisibleForTesting
    public boolean hasPrerenderedUrl(String url) {
        return nativeHasPrerenderedUrl(mNativeTabAndroid, url);
    }

    @VisibleForTesting
    public int getSystemWindowInsetBottom() {
        ChromeActivity activity = getActivity();
        if (activity != null && activity.getInsetObserverView() != null) {
            return activity.getInsetObserverView().getSystemWindowInsetsBottom();
        }
        return 0;
    }

    /**
     * @return the UMA object for the tab. Note that this may be null in some
     * cases.
     */
    public TabUma getTabUma() {
        return mTabUma;
    }

    /**
     * @return The most recently used rank for this tab in the given TabModel.
     */
    private static int computeMRURank(Tab tab, TabModel model) {
        final long tabLastShow = tab.getTabUma().getLastShownTimestamp();
        int mruRank = 0;
        for (int i = 0; i < model.getCount(); i++) {
            Tab otherTab = model.getTabAt(i);
            if (otherTab != tab && otherTab.getTabUma() != null
                    && otherTab.getTabUma().getLastShownTimestamp() > tabLastShow) {
                mruRank++;
            }
        }
        return mruRank;
    }

    /**
     * Sets the Intent that can be fired to restart the Activity of this Tab's parent.
     * Should only be called if the Tab was launched via a different Activity.
     */
    public void setParentIntent(Intent parentIntent) {
        mParentIntent = parentIntent;
    }

    /**
     * @return Intent that can be fired to restart the parent Activity.
     */
    protected Intent getParentIntent() {
        return mParentIntent;
    }

    /**
     * Creates a new, "frozen" tab from a saved state. This can be used for background tabs restored
     * on cold start that should be loaded when switched to. initialize() needs to be called
     * afterwards to complete the second level initialization.
     */
    public static Tab createFrozenTabFromState(
            int id, boolean incognito, WindowAndroid nativeWindow, int parentId, TabState state) {
        assert state != null;
        return new Tab(id, parentId, incognito, nativeWindow, TabLaunchType.FROM_RESTORE,
                TabCreationState.FROZEN_ON_RESTORE, state);
    }

    /**
     * Update whether or not the current native tab and/or web contents are
     * currently visible (from an accessibility perspective), or whether
     * they're obscured by another view.
     */
    public void updateAccessibilityVisibility() {
        View view = getView();
        if (view != null) {
            int importantForAccessibility = isObscuredByAnotherViewForAccessibility()
                    ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                    : View.IMPORTANT_FOR_ACCESSIBILITY_YES;
            if (view.getImportantForAccessibility() != importantForAccessibility) {
                view.setImportantForAccessibility(importantForAccessibility);
                view.sendAccessibilityEvent(
                        AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
            }
        }

        WebContentsAccessibility wcax = getWebContentsAccessibility(getWebContents());
        if (wcax != null) {
            boolean isWebContentObscured = isObscuredByAnotherViewForAccessibility()
                    || isShowingSadTab();
            wcax.setObscuredByAnotherView(isWebContentObscured);
        }
    }

    private boolean isObscuredByAnotherViewForAccessibility() {
        ChromeActivity activity = getActivity();
        return activity != null && activity.isViewObscuringAllTabs();
    }

    /**
     * Creates a new tab to be loaded lazily. This can be used for tabs opened in the background
     * that should be loaded when switched to. initialize() needs to be called afterwards to
     * complete the second level initialization.
     */
    public static Tab createTabForLazyLoad(boolean incognito, WindowAndroid nativeWindow,
            TabLaunchType type, int parentId, LoadUrlParams loadUrlParams) {
        Tab tab = new Tab(INVALID_TAB_ID, parentId, incognito, nativeWindow, type,
                TabCreationState.FROZEN_FOR_LAZY_LOAD, null);
        tab.setPendingLoadParams(loadUrlParams);
        return tab;
    }

    /**
     * Creates a fresh tab. initialize() needs to be called afterwards to complete the second level
     * initialization.
     * @param initiallyHidden true iff the tab being created is initially in background
     */
    public static Tab createLiveTab(int id, boolean incognito, WindowAndroid nativeWindow,
            TabLaunchType type, int parentId, boolean initiallyHidden) {
        return new Tab(id, parentId, incognito, nativeWindow, type,
                initiallyHidden ? TabCreationState.LIVE_IN_BACKGROUND
                                : TabCreationState.LIVE_IN_FOREGROUND,
                null);
    }

    /**
     * Creates an instance of a {@link Tab} that is fully detached from any activity.
     * Also performs general tab initialization as well as detached specifics.
     *
     * The current application context must allow the creation of a WindowAndroid.
     *
     * @return The newly created and initialized tab.
     */
    public static Tab createDetached(TabDelegateFactory delegateFactory) {
        Context context = ContextUtils.getApplicationContext();
        WindowAndroid windowAndroid = new WindowAndroid(context);
        Tab tab = new Tab(INVALID_TAB_ID, INVALID_TAB_ID, false, windowAndroid,
                TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION, null, null);
        tab.initialize(null, null, delegateFactory, true, false);

        // Resize the webContent to avoid expensive post load resize when attaching the tab.
        Rect bounds = getEstimatedContentSize(context);
        int width = bounds.right - bounds.left;
        int height = bounds.bottom - bounds.top;
        tab.getWebContents().setSize(width, height);

        tab.detach();
        return tab;
    }

    /**
     * @return Whether the theme color for this tab is the default color.
     */
    public boolean isDefaultThemeColor() {
        return isNativePage() || mDefaultThemeColor == getThemeColor();
    }

    /**
     * @return The default theme color for this tab.
     */
    @VisibleForTesting
    public int getDefaultThemeColor() {
        return mDefaultThemeColor;
    }

    /**
     * @return Intent that tells Chrome to bring an Activity for a particular Tab back to the
     *         foreground, or null if this isn't possible.
     */
    @Nullable
    public static Intent createBringTabToFrontIntent(int tabId) {
        // Iterate through all {@link CustomTab}s and check whether the given tabId belongs to a
        // {@link CustomTab}. If so, return null as the client app's task cannot be foregrounded.
        List<WeakReference<Activity>> list = ApplicationStatus.getRunningActivities();
        for (WeakReference<Activity> ref : list) {
            Activity activity = ref.get();
            if (activity instanceof CustomTabActivity
                    && ((CustomTabActivity) activity).getActivityTab() != null
                    && tabId == ((CustomTabActivity) activity).getActivityTab().getId()) {
                return null;
            }
        }

        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, ChromeLauncherActivity.class);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(TabOpenType.BRING_TAB_TO_FRONT.name(), tabId);
        return intent;
    }

    /**
     * Check whether the context menu download should be intercepted.
     *
     * @param url URL to be downloaded.
     * @return whether the download should be intercepted.
     */
    protected boolean shouldInterceptContextMenuDownload(String url) {
        return mDownloadDelegate.shouldInterceptContextMenuDownload(url);
    }

    void handleRendererUnresponsive() {
        mIsRendererUnresponsive = true;
        if (mFullscreenManager == null) return;

        mFullscreenManager.setPositionsForTabToNonFullscreen();
        updateBrowserControlsState(BrowserControlsState.SHOWN, false);
    }

    void handleRendererResponsive() {
        mIsRendererUnresponsive = false;
        if (mFullscreenManager == null) return;
        updateFullscreenEnabledState();
    }

    /**
     * @return Whether the renderer is currently unresponsive.
     */
    protected boolean isRendererUnresponsive() {
        return mIsRendererUnresponsive;
    }

    /**
     * Reset swipe-to-refresh handler.
     */
    void resetSwipeRefreshHandler() {
        // When the dialog is visible, keeping the refresh animation active
        // in the background is distracting and unnecessary (and likely to
        // jank when the dialog is shown).
        if (mSwipeRefreshHandler != null) {
            mSwipeRefreshHandler.reset();
        }
    }

    /**
     * Stop swipe-to-refresh animation.
     */
    void stopSwipeRefreshHandler() {
        if (mSwipeRefreshHandler != null) {
            mSwipeRefreshHandler.didStopRefreshing();
        }
    }

    /**
     * Set whether closing this Tab should return the user to the app that spawned Chrome.
     */
    public void setIsAllowedToReturnToExternalApp(boolean state) {
        mIsAllowedToReturnToExternalApp = state;
    }

    /**
     * @return Whether closing this Tab should return the user to the app that spawned Chrome.
     */
    public boolean isAllowedToReturnToExternalApp() {
        return mIsAllowedToReturnToExternalApp;
    }

    /**
     * @return Whether or not the tab was opened by an app other than Chrome.
     */
    public boolean isCreatedForExternalApp() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        return getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP
                && !TextUtils.equals(getAppAssociatedWith(), packageName);
    }

    /**
     * Set the Webapp manifest scope, which is used to allow frames within the scope to autoplay
     * media unmuted.
     */
    public void setWebappManifestScope(String scope) {
        nativeSetWebappManifestScope(mNativeTabAndroid, scope);
    }

    /**
     * Configures web preferences for enabling Picture-in-Picture.
     * @param enabled Whether Picture-in-Picture should be enabled.
     */
    public void setPictureInPictureEnabled(boolean enabled) {
        if (mNativeTabAndroid == 0) return;
        nativeSetPictureInPictureEnabled(mNativeTabAndroid, enabled);
    }

    /**
     * Configures web preferences for viewing downloaded media.
     * @param enabled Whether embedded media experience should be enabled.
     */
    public void enableEmbeddedMediaExperience(boolean enabled) {
        if (mNativeTabAndroid == 0) return;
        nativeEnableEmbeddedMediaExperience(mNativeTabAndroid, enabled);
    }

    /**
     * Called when the orientation of the activity has changed.
     */
    public void onOrientationChange() {
        hideMediaDownloadInProductHelp();
    }

    /**
     * Handle browser controls when a tab modal dialog is shown.
     * @param isShowing Whether a tab modal dialog is showing.
     */
    public void onTabModalDialogStateChanged(boolean isShowing) {
        mIsShowingTabModalDialog = isShowing;
        if (mFullscreenManager == null) return;
        mFullscreenManager.setPositionsForTabToNonFullscreen();
        updateBrowserControlsState(BrowserControlsState.SHOWN, false);
    }

    @CalledByNative
    private void showMediaDownloadInProductHelp(int x, int y, int width, int height) {
        Rect rect = new Rect(x, y, x + width, y + height);

        // If we are not currently showing the widget, ask the tracker if we can show it.
        if (mDownloadIPHBubble == null) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
            tracker.notifyEvent(EventConstants.MEDIA_DOWNLOAD_BUTTON_DISPLAYED);
            if (!tracker.shouldTriggerHelpUI(FeatureConstants.MEDIA_DOWNLOAD_FEATURE)) {
                // Inform native that the button was dismissed to notify the renderer that the
                // request was rejected.
                nativeMediaDownloadInProductHelpDismissed(mNativeTabAndroid);
                return;
            }

            mDownloadIPHBubble = new TextBubble(getApplicationContext(),
                    mContentViewCore.getContainerView(), R.string.iph_media_download_text,
                    R.string.iph_media_download_accessibility_text, rect);
            mDownloadIPHBubble.setDismissOnTouchInteraction(true);
            mDownloadIPHBubble.addOnDismissListener(new OnDismissListener() {
                @Override
                public void onDismiss() {
                    ThreadUtils.postOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            hideMediaDownloadInProductHelp();
                        }
                    });
                }
            });
        }

        mDownloadIPHBubble.setPreferredVerticalOrientation(
                AnchoredPopupWindow.VERTICAL_ORIENTATION_BELOW);
        mDownloadIPHBubble.show();
        createPulse(rect);
    }

    private void createPulse(Rect rect) {
        if (mPulsePopupWindow == null) {
            PulseDrawable pulseDrawable = PulseDrawable.createCircle(mThemedApplicationContext);
            View view = new Button(getActivity());
            view.setLayoutParams(new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            view.setBackground(pulseDrawable);

            mPulsePopupWindow = new PopupWindow(getActivity());
            mPulsePopupWindow.setBackgroundDrawable(null);
            mPulsePopupWindow.setContentView(view);
            mPulsePopupWindow.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
            mPulsePopupWindow.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
            mPulsePopupWindow.getContentView().setOnClickListener(
                    v -> hideMediaDownloadInProductHelp());
            mPulsePopupWindow.showAtLocation(
                    getView(), Gravity.TOP | Gravity.START, rect.left, rect.top);
            pulseDrawable.start();
        }

        mPulsePopupWindow.update(rect.left, rect.top, rect.width(), rect.height());
    }

    @CalledByNative
    private void hideMediaDownloadInProductHelp() {
        if (mPulsePopupWindow != null && mPulsePopupWindow.isShowing()) {
            mPulsePopupWindow.dismiss();
            mPulsePopupWindow = null;
        }

        if (mDownloadIPHBubble == null) return;

        mDownloadIPHBubble.dismiss();
        mDownloadIPHBubble = null;
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
        tracker.dismissed(FeatureConstants.MEDIA_DOWNLOAD_FEATURE);
        nativeMediaDownloadInProductHelpDismissed(mNativeTabAndroid);
    }

    private native void nativeInit();
    private native void nativeDestroy(long nativeTabAndroid);
    private native void nativeInitWebContents(long nativeTabAndroid, boolean incognito,
            boolean isBackgroundTab, WebContents webContents,
            TabWebContentsDelegateAndroid delegate, ContextMenuPopulator contextMenuPopulator);
    private native void nativeUpdateDelegates(long nativeTabAndroid,
            TabWebContentsDelegateAndroid delegate, ContextMenuPopulator contextMenuPopulator);
    private native void nativeDestroyWebContents(long nativeTabAndroid, boolean deleteNative);
    private native void nativeOnPhysicalBackingSizeChanged(
            long nativeTabAndroid, WebContents webContents, int width, int height);
    private native Profile nativeGetProfileAndroid(long nativeTabAndroid);
    private native int nativeLoadUrl(long nativeTabAndroid, String url, String extraHeaders,
            ResourceRequestBody postData, int transition, String referrerUrl, int referrerPolicy,
            boolean isRendererInitiated, boolean shoulReplaceCurrentEntry,
            long intentReceivedTimestamp, boolean hasUserGesture, boolean shouldClearHistoryList);
    private native void nativeSetActiveNavigationEntryTitleForUrl(long nativeTabAndroid, String url,
            String title);
    private native boolean nativePrint(
            long nativeTabAndroid, int renderProcessId, int renderFrameId);
    private native Bitmap nativeGetFavicon(long nativeTabAndroid);
    private native void nativeCreateHistoricalTab(long nativeTabAndroid);
    private native void nativeUpdateBrowserControlsState(
            long nativeTabAndroid, int constraints, int current, boolean animate);
    private native void nativeLoadOriginalImage(long nativeTabAndroid);
    private native long nativeGetBookmarkId(long nativeTabAndroid, boolean onlyEditable);
    private native void nativeSetInterceptNavigationDelegate(long nativeTabAndroid,
            InterceptNavigationDelegate delegate);
    private native void nativeAttachToTabContentManager(long nativeTabAndroid,
            TabContentManager tabContentManager);
    private native void nativeClearThumbnailPlaceholder(long nativeTabAndroid);
    private native boolean nativeHasPrerenderedUrl(long nativeTabAndroid, String url);
    private native void nativeSetWebappManifestScope(long nativeTabAndroid, String scope);
    private native void nativeSetPictureInPictureEnabled(long nativeTabAndroid, boolean enabled);
    private native void nativeEnableEmbeddedMediaExperience(long nativeTabAndroid, boolean enabled);
    private native void nativeAttachDetachedTab(long nativeTabAndroid);
    private native void nativeMediaDownloadInProductHelpDismissed(long nativeTabAndroid);
    private native int nativeGetCurrentRenderProcessId(long nativeTabAndroid);
}
