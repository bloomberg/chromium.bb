// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.text.TextUtils;

import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.FrozenNativePage;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchTabHelper;
import org.chromium.chrome.browser.dom_distiller.ReaderModeActivityDelegate;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.media.ui.MediaSessionTabHelper;
import org.chromium.chrome.browser.ntp.NativePageAssassin;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.tab.TabUma.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.crypto.CipherFactory;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

/**
 * A representation of a Tab for Chrome. This manages wrapping a WebContents and interacting with
 * most of Chromium while representing a consistent version of web content to the front end.
 */
public class ChromeTab extends Tab {
    public static final int NTP_TAB_ID = -2;

    /** The maximum amount of time to wait for a page to load before entering fullscreen.  -1 means
     *  wait until the page finishes loading. */
    private static final long MAX_FULLSCREEN_LOAD_DELAY_MS = 3000;

    private ReaderModeManager mReaderModeManager;

    private TabRedirectHandler mTabRedirectHandler;

    private boolean mIsFullscreenWaitingForLoad = false;

    /**
     * Whether didCommitProvisionalLoadForFrame() hasn't yet been called for the current native page
     * (page A). To decrease latency, we show native pages in both loadUrl() and
     * didCommitProvisionalLoadForFrame(). However, we mustn't show a new native page (page B) in
     * loadUrl() if the current native page hasn't yet been committed. Otherwise, we'll show each
     * page twice (A, B, A, B): the first two times in loadUrl(), the second two times in
     * didCommitProvisionalLoadForFrame().
     */
    private boolean mIsNativePageCommitPending;

    private WebContentsObserver mWebContentsObserver;

    /**
     * Listens to gesture events fired by the ContentViewCore.
     */
    private GestureStateListener mGestureStateListener;

    /**
     * Basic constructor. This is hidden, so that explicitly named factory methods are used to
     * create tabs. initialize() needs to be called afterwards to complete the second level
     * initialization.
     * @param creationState State in which the tab is created, needed to initialize TabUma
     *                      accounting. When null, TabUma will not be initialized.
     * @param frozenState TabState that was saved when the Tab was last persisted to storage.
     */
    protected ChromeTab(
            int id, ChromeActivity activity, boolean incognito, WindowAndroid nativeWindow,
            TabLaunchType type, int parentId, TabCreationState creationState,
            TabState frozenState) {
        super(id, parentId, incognito, activity, nativeWindow, type, frozenState);

        if (creationState != null) {
            if (frozenState == null) {
                assert type != TabLaunchType.FROM_RESTORE
                        && creationState != TabCreationState.FROZEN_ON_RESTORE;
            } else {
                assert type == TabLaunchType.FROM_RESTORE
                        && creationState == TabCreationState.FROZEN_ON_RESTORE;
            }
        }

        addObserver(mTabObserver);

        if (mActivity != null && creationState != null) {
            setTabUma(new TabUma(
                    this, creationState, mActivity.getTabModelSelector().getModel(incognito)));
        }

        if (incognito) {
            CipherFactory.getInstance().triggerKeyGeneration();
        }

        mReaderModeManager = new ReaderModeManager(this, activity);
        RevenueStats.getInstance().tabCreated(this);

        mTabRedirectHandler = new TabRedirectHandler(activity);

        ContextualSearchTabHelper.createForTab(this);
        if (nativeWindow != null) ThumbnailTabHelper.createForTab(this);
        MediaSessionTabHelper.createForTab(this);
    }

    /**
     * Creates a minimal {@link ChromeTab} for testing. Do not use outside testing.
     *
     * @param id        The id of the tab.
     * @param incognito Whether the tab is incognito.
     */
    @VisibleForTesting
    public ChromeTab(int id, boolean incognito) {
        super(id, incognito, null);
        mTabRedirectHandler = new TabRedirectHandler(null);
    }

    /**
     * Creates a fresh tab. initialize() needs to be called afterwards to complete the second level
     * initialization.
     * @param initiallyHidden true iff the tab being created is initially in background
     */
    public static ChromeTab createLiveTab(int id, ChromeActivity activity, boolean incognito,
            WindowAndroid nativeWindow, TabLaunchType type, int parentId, boolean initiallyHidden) {
        return new ChromeTab(id, activity, incognito, nativeWindow, type, parentId,
                initiallyHidden ? TabCreationState.LIVE_IN_BACKGROUND :
                                  TabCreationState.LIVE_IN_FOREGROUND, null);
    }

    /**
     * Creates a new, "frozen" tab from a saved state. This can be used for background tabs restored
     * on cold start that should be loaded when switched to. initialize() needs to be called
     * afterwards to complete the second level initialization.
     */
    public static ChromeTab createFrozenTabFromState(
            int id, ChromeActivity activity, boolean incognito,
            WindowAndroid nativeWindow, int parentId, TabState state) {
        assert state != null;
        return new ChromeTab(id, activity, incognito, nativeWindow,
                TabLaunchType.FROM_RESTORE, parentId, TabCreationState.FROZEN_ON_RESTORE,
                state);
    }

    /**
     * Creates a new tab to be loaded lazily. This can be used for tabs opened in the background
     * that should be loaded when switched to. initialize() needs to be called afterwards to
     * complete the second level initialization.
     */
    public static ChromeTab createTabForLazyLoad(ChromeActivity activity, boolean incognito,
            WindowAndroid nativeWindow, TabLaunchType type, int parentId,
            LoadUrlParams loadUrlParams) {
        ChromeTab tab = new ChromeTab(
                INVALID_TAB_ID, activity, incognito, nativeWindow, type, parentId,
                TabCreationState.FROZEN_FOR_LAZY_LOAD, null);
        tab.setPendingLoadParams(loadUrlParams);
        return tab;
    }

    public static ChromeTab fromTab(Tab tab) {
        return (ChromeTab) tab;
    }

    @Override
    protected void didStartPageLoad(String validatedUrl, boolean showingErrorPage) {
        mIsFullscreenWaitingForLoad = !DomDistillerUrlUtils.isDistilledPage(validatedUrl);

        super.didStartPageLoad(validatedUrl, showingErrorPage);
    }

    @Override
    protected void didFinishPageLoad() {
        super.didFinishPageLoad();

        // Handle the case where a commit or prerender swap notification failed to arrive and the
        // enable fullscreen message was never enqueued.
        scheduleEnableFullscreenLoadDelayIfNecessary();
    }

    @Override
    protected void didFailPageLoad(int errorCode) {
        cancelEnableFullscreenLoadDelay();
        super.didFailPageLoad(errorCode);
        updateFullscreenEnabledState();
    }

    private void scheduleEnableFullscreenLoadDelayIfNecessary() {
        if (mIsFullscreenWaitingForLoad
                && !mHandler.hasMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD)) {
            mHandler.sendEmptyMessageDelayed(
                    MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, MAX_FULLSCREEN_LOAD_DELAY_MS);
        }
    }

    private void cancelEnableFullscreenLoadDelay() {
        mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
        mIsFullscreenWaitingForLoad = false;
    }

    /**
     * Removes the enable fullscreen runnable from the UI queue and runs it immediately.
     */
    @VisibleForTesting
    public void processEnableFullscreenRunnableForTest() {
        if (mHandler.hasMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD)) {
            mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
            enableFullscreenAfterLoad();
        }
    }

    @Override
    protected void enableFullscreenAfterLoad() {
        if (!mIsFullscreenWaitingForLoad) return;

        mIsFullscreenWaitingForLoad = false;
        updateFullscreenEnabledState();
    }

    @Override
    protected boolean isHidingTopControlsEnabled() {
        return super.isHidingTopControlsEnabled()  && !mIsFullscreenWaitingForLoad;
    }

    @Override
    protected void setContentViewCore(ContentViewCore cvc) {
        try {
            TraceEvent.begin("ChromeTab.setContentViewCore");
            super.setContentViewCore(cvc);
            mWebContentsObserver = new TabWebContentsObserver(cvc.getWebContents(), this);

            setInterceptNavigationDelegate(createInterceptNavigationDelegate());

            if (mGestureStateListener == null) mGestureStateListener = createGestureStateListener();
            cvc.addGestureStateListener(mGestureStateListener);
        } finally {
            TraceEvent.end("ChromeTab.setContentViewCore");
        }
    }

    private GestureStateListener createGestureStateListener() {
        return new GestureStateListener() {
            @Override
            public void onFlingStartGesture(int vx, int vy, int scrollOffsetY, int scrollExtentY) {
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

    @Override
    protected void destroyContentViewCoreInternal(ContentViewCore cvc) {
        super.destroyContentViewCoreInternal(cvc);

        if (mGestureStateListener != null) {
            cvc.removeGestureStateListener(mGestureStateListener);
        }
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
        }
        mWebContentsObserver = null;
    }

    /**
     * @return Whether the tab is ready to display or it should be faded in as it loads.
     */
    public boolean shouldStall() {
        return (isFrozen() || needsReload())
                && !NativePageFactory.isNativePageUrl(getUrl(), isIncognito());
    }

    @Override
    protected void showInternal(TabSelectionType type) {
        super.showInternal(type);

        // If the NativePage was frozen while in the background (see NativePageAssassin),
        // recreate the NativePage now.
        if (getNativePage() instanceof FrozenNativePage) {
            maybeShowNativePage(getUrl(), true);
        }
        NativePageAssassin.getInstance().tabShown(this);
    }

    @Override
    protected void restoreIfNeededInternal() {
        super.restoreIfNeededInternal();
    }

    @Override
    protected void hideInternal() {
        super.hideInternal();
        cancelEnableFullscreenLoadDelay();

        // Allow this tab's NativePage to be frozen if it stays hidden for a while.
        NativePageAssassin.getInstance().tabHidden(this);

        mTabRedirectHandler.clear();
    }

    @Override
    public int loadUrl(LoadUrlParams params) {
        try {
            TraceEvent.begin("ChromeTab.loadUrl");

            // The data reduction proxy can only be set to pass through mode via loading an image in
            // a new tab. We squirrel away whether pass through mode was set, and check it in:
            // @see TabWebContentsDelegateAndroid#onLoadStopped()
            mLastPageLoadHasSpdyProxyPassthroughHeaders = false;
            if (TextUtils.equals(params.getVerbatimHeaders(), PAGESPEED_PASSTHROUGH_HEADERS)) {
                mLastPageLoadHasSpdyProxyPassthroughHeaders = true;
            }

            // TODO(tedchoc): When showing the android NTP, delay the call to nativeLoadUrl until
            //                the android view has entirely rendered.
            if (!mIsNativePageCommitPending) {
                mIsNativePageCommitPending = maybeShowNativePage(params.getUrl(), false);
            }

            return super.loadUrl(params);
        } finally {
            TraceEvent.end("ChromeTab.loadUrl");
        }
    }

    @VisibleForTesting
    public AuthenticatorNavigationInterceptor getAuthenticatorHelper() {
        return getInterceptNavigationDelegate().getAuthenticatorNavigationInterceptor();
    }

    /**
     * @return the TabRedirectHandler for the tab.
     */
    public TabRedirectHandler getTabRedirectHandler() {
        return mTabRedirectHandler;
    }

    @Override
    protected boolean shouldIgnoreNewTab(String url, boolean incognito) {
        InterceptNavigationDelegateImpl delegate = getInterceptNavigationDelegate();
        return delegate != null && delegate.shouldIgnoreNewTab(url, incognito);
    }

    /**
     * @return A potential fallback texture id to use when trying to draw this tab.
     */
    public int getFallbackTextureId() {
        return INVALID_TAB_ID;
    }

    @Override
    public InterceptNavigationDelegateImpl getInterceptNavigationDelegate() {
        return (InterceptNavigationDelegateImpl) super.getInterceptNavigationDelegate();
    }

    /**
     * Factory method for {@link InterceptNavigationDelegateImpl}. Meant to be overridden by
     * subclasses.
     * @return A new instance of {@link InterceptNavigationDelegateImpl}.
     */
    protected InterceptNavigationDelegateImpl createInterceptNavigationDelegate() {
        return new InterceptNavigationDelegateImpl(mActivity, this);
    }

    /**
     * @return The reader mode manager for this tab that handles UI events for reader mode.
     */
    public ReaderModeManager getReaderModeManager() {
        return mReaderModeManager;
    }

    public ReaderModeActivityDelegate getReaderModeActivityDelegate() {
        return mActivity == null ? null : mActivity.getReaderModeActivityDelegate();
    }

    /**
     * Shows a native page for url if it's a valid chrome-native URL. Otherwise, does nothing.
     * @param url The url of the current navigation.
     * @param isReload Whether the current navigation is a reload.
     * @return True, if a native page was displayed for url.
     */
    boolean maybeShowNativePage(String url, boolean isReload) {
        NativePage candidateForReuse = isReload ? null : getNativePage();
        NativePage nativePage = NativePageFactory.createNativePageForURL(url, candidateForReuse,
                this, mActivity.getTabModelSelector(), mActivity);
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
    void handleDidCommitProvisonalLoadForFrame(String url, int transitionType) {
        mIsNativePageCommitPending = false;
        boolean isReload = (transitionType == PageTransition.RELOAD);
        if (!maybeShowNativePage(url, isReload)) {
            showRenderedPage();
        }

        mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
        mHandler.sendEmptyMessageDelayed(
                MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, MAX_FULLSCREEN_LOAD_DELAY_MS);
        updateFullscreenEnabledState();
    }

    // TODO(dtrainor): Port more methods to the observer.
    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onSSLStateUpdated(Tab tab) {
            PolicyAuditor auditor =
                    ((ChromeApplication) getApplicationContext()).getPolicyAuditor();
            auditor.notifyCertificateFailure(getWebContents(), getApplicationContext());
            updateFullscreenEnabledState();
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            if (!didStartLoad) return;

            String url = tab.getUrl();
            // Simulate the PAGE_LOAD_STARTED notification that we did not get.
            didStartPageLoad(url, false);

            // As we may have missed the main frame commit notification for the
            // swapped web contents, schedule the enabling of fullscreen now.
            scheduleEnableFullscreenLoadDelayIfNecessary();

            if (didFinishLoad) {
                // Simulate the PAGE_LOAD_FINISHED notification that we did not get.
                didFinishPageLoad();
            }
        }
    };
}
