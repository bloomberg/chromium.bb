// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeWebContentsDelegateAndroid;
import org.chromium.chrome.browser.FrozenNativePage;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchTabHelper;
import org.chromium.chrome.browser.crash.MinidumpUploadService;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.document.DocumentWebContentsDelegate;
import org.chromium.chrome.browser.dom_distiller.ReaderModeActivityDelegate;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.media.MediaCaptureNotificationService;
import org.chromium.chrome.browser.media.ui.MediaSessionTabHelper;
import org.chromium.chrome.browser.ntp.NativePageAssassin;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.tab.TabUma.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.crypto.CipherFactory;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.InvalidateTypes;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.WindowOpenDisposition;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

/**
 * A representation of a Tab for Chrome. This manages wrapping a WebContents and interacting with
 * most of Chromium while representing a consistent version of web content to the front end.
 */
public class ChromeTab extends Tab {
    public static final int NTP_TAB_ID = -2;

    private static final String TAG = "cr.ChromeTab";

    // URL didFailLoad error code. Should match the value in net_error_list.h.
    public static final int BLOCKED_BY_ADMINISTRATOR = -22;

    private static final int MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD = 1;

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

    private Handler mHandler;

    private final Runnable mCloseContentsRunnable = new Runnable() {
        @Override
        public void run() {
            boolean isSelected = mActivity.getTabModelSelector().getCurrentTab() == ChromeTab.this;
            mActivity.getTabModelSelector().closeTab(ChromeTab.this);

            // If the parent Tab belongs to another Activity, fire the Intent to bring it back.
            if (isSelected && getParentIntent() != null
                    && mActivity.getIntent() != getParentIntent()) {
                boolean mayLaunch = FeatureUtilities.isDocumentMode(mActivity)
                        ? isParentInAndroidOverview() : true;
                if (mayLaunch) mActivity.startActivity(getParentIntent());
            }
        }

        /** If the API allows it, returns whether a Task still exists for the parent Activity. */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        private boolean isParentInAndroidOverview() {
            ActivityManager activityManager =
                    (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
            for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
                Intent taskIntent = DocumentUtils.getBaseIntentFromTask(task);
                if (taskIntent != null && taskIntent.filterEquals(getParentIntent())) return true;
            }
            return false;
        }
    };


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
        mHandler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                if (msg == null) return;
                if (msg.what == MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD) {
                    enableFullscreenAfterLoad();
                }
            }
        };
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
    protected TabChromeWebContentsDelegateAndroid createWebContentsDelegate() {
        return new TabChromeWebContentsDelegateAndroidImpl();
    }

    /**
     * An implementation for this tab's web contents delegate.
     */
    public class TabChromeWebContentsDelegateAndroidImpl
            extends TabChromeWebContentsDelegateAndroid {
        private Pair<WebContents, String> mWebContentsUrlMapping;

        protected TabModel getTabModel() {
            // TODO(dfalcantara): Remove this when DocumentActivity.getTabModelSelector()
            //                    can return a TabModelSelector that activateContents() can use.
            return mActivity.getTabModelSelector().getModel(isIncognito());
        }

        @Override
        public boolean shouldResumeRequestsForCreatedWindow() {
            // Pause the WebContents if an Activity has to be created for it first.
            TabCreator tabCreator = mActivity.getTabCreator(isIncognito());
            assert tabCreator != null;
            return !tabCreator.createsTabsAsynchronously();
        }

        @Override
        public void webContentsCreated(WebContents sourceWebContents, long openerRenderFrameId,
                String frameName, String targetUrl, WebContents newWebContents) {
            super.webContentsCreated(sourceWebContents, openerRenderFrameId, frameName,
                    targetUrl, newWebContents);

            // The URL can't be taken from the WebContents if it's paused.  Save it for later.
            assert mWebContentsUrlMapping == null;
            mWebContentsUrlMapping = Pair.create(newWebContents, targetUrl);

            // TODO(dfalcantara): Re-remove this once crbug.com/508366 is fixed.
            TabCreator tabCreator = mActivity.getTabCreator(isIncognito());

            if (tabCreator != null && tabCreator.createsTabsAsynchronously()) {
                DocumentWebContentsDelegate.getInstance().attachDelegate(newWebContents);
            }
        }

        @Override
        public boolean addNewContents(WebContents sourceWebContents, WebContents webContents,
                int disposition, Rect initialPosition, boolean userGesture) {
            assert mWebContentsUrlMapping.first == webContents;

            TabCreator tabCreator = mActivity.getTabCreator(isIncognito());
            assert tabCreator != null;

            // Grab the URL, which might not be available via the Tab.
            String url = mWebContentsUrlMapping.second;
            mWebContentsUrlMapping = null;

            // Skip opening a new Tab if it doesn't make sense.
            if (isClosing()) return false;

            // Creating new Tabs asynchronously requires starting a new Activity to create the Tab,
            // so the Tab returned will always be null.  There's no way to know synchronously
            // whether the Tab is created, so assume it's always successful.
            boolean createdSuccessfully = tabCreator.createTabWithWebContents(
                    webContents, getId(), TabLaunchType.FROM_LONGPRESS_FOREGROUND, url);
            boolean success = tabCreator.createsTabsAsynchronously() || createdSuccessfully;
            if (success && disposition == WindowOpenDisposition.NEW_POPUP) {
                PolicyAuditor auditor =
                        ((ChromeApplication) getApplicationContext()).getPolicyAuditor();
                auditor.notifyAuditEvent(getApplicationContext(), AuditEvent.OPEN_POPUP_URL_SUCCESS,
                        url, "");
            }

            return success;
        }

        @Override
        public void activateContents() {
            boolean activityIsDestroyed = false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
                activityIsDestroyed = mActivity.isDestroyed();
            }
            if (activityIsDestroyed || !isInitialized()) {
                Log.e(TAG, "Activity destroyed before calling activateContents().  Bailing out.");
                return;
            }

            TabModel model = getTabModel();
            int index = model.indexOf(ChromeTab.this);
            if (index == TabModel.INVALID_TAB_INDEX) return;
            TabModelUtils.setIndex(model, index);
            bringActivityToForeground();
        }

        /**
         * Brings chrome's Activity to foreground, if it is not so.
         */
        protected void bringActivityToForeground() {
            // This intent is sent in order to get the activity back to the foreground if it was
            // not already. The previous call will activate the right tab in the context of the
            // TabModel but will only show the tab to the user if Chrome was already in the
            // foreground.
            // The intent is getting the tabId mostly because it does not cost much to do so.
            // When receiving the intent, the tab associated with the tabId should already be
            // active.
            // Note that calling only the intent in order to activate the tab is slightly slower
            // because it will change the tab when the intent is handled, which happens after
            // Chrome gets back to the foreground.
            Intent newIntent = Tab.createBringTabToFrontIntent(ChromeTab.this.getId());
            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            getApplicationContext().startActivity(newIntent);
        }

        @Override
        public void navigationStateChanged(int flags) {
            if ((flags & InvalidateTypes.TAB) != 0) {
                MediaCaptureNotificationService.updateMediaNotificationForTab(
                        getApplicationContext(), getId(), isCapturingAudio(),
                        isCapturingVideo(), getUrl());
            }
            super.navigationStateChanged(flags);
        }

        @Override
        public void onLoadProgressChanged(int progress) {
            if (!isLoading()) return;
            notifyLoadProgress(getProgress());
        }

        @Override
        public void closeContents() {
            // Execute outside of callback, otherwise we end up deleting the native
            // objects in the middle of executing methods on them.
            mHandler.removeCallbacks(mCloseContentsRunnable);
            mHandler.post(mCloseContentsRunnable);
        }

        @Override
        public boolean takeFocus(boolean reverse) {
            if (reverse) {
                View menuButton = mActivity.findViewById(R.id.menu_button);
                if (menuButton == null || !menuButton.isShown()) {
                    menuButton = mActivity.findViewById(R.id.document_menu_button);
                }
                if (menuButton != null && menuButton.isShown()) {
                    return menuButton.requestFocus();
                }

                View tabSwitcherButton = mActivity.findViewById(R.id.tab_switcher_button);
                if (tabSwitcherButton != null && tabSwitcherButton.isShown()) {
                    return tabSwitcherButton.requestFocus();
                }
            } else {
                View urlBar = mActivity.findViewById(R.id.url_bar);
                if (urlBar != null) return urlBar.requestFocus();
            }
            return false;
        }

        @Override
        public void handleKeyboardEvent(KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                if (mActivity.onKeyDown(event.getKeyCode(), event)) return;

                // Handle the Escape key here (instead of in KeyboardShortcuts.java), so it doesn't
                // interfere with other parts of the activity (e.g. the URL bar).
                if (event.getKeyCode() == KeyEvent.KEYCODE_ESCAPE && event.hasNoModifiers()) {
                    WebContents wc = getWebContents();
                    if (wc != null) wc.stop();
                    return;
                }
            }
            handleMediaKey(event);
        }

        /**
         * Redispatches unhandled media keys. This allows bluetooth headphones with play/pause or
         * other buttons to function correctly.
         */
        @TargetApi(19)
        private void handleMediaKey(KeyEvent e) {
            if (Build.VERSION.SDK_INT < 19) return;
            switch (e.getKeyCode()) {
                case KeyEvent.KEYCODE_MUTE:
                case KeyEvent.KEYCODE_HEADSETHOOK:
                case KeyEvent.KEYCODE_MEDIA_PLAY:
                case KeyEvent.KEYCODE_MEDIA_PAUSE:
                case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                case KeyEvent.KEYCODE_MEDIA_STOP:
                case KeyEvent.KEYCODE_MEDIA_NEXT:
                case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
                case KeyEvent.KEYCODE_MEDIA_REWIND:
                case KeyEvent.KEYCODE_MEDIA_RECORD:
                case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
                case KeyEvent.KEYCODE_MEDIA_CLOSE:
                case KeyEvent.KEYCODE_MEDIA_EJECT:
                case KeyEvent.KEYCODE_MEDIA_AUDIO_TRACK:
                    AudioManager am = (AudioManager) mActivity.getSystemService(
                            Context.AUDIO_SERVICE);
                    am.dispatchMediaKeyEvent(e);
                    break;
                default:
                    break;
            }
        }

        /**
         * @return Whether audio is being captured.
         */
        private boolean isCapturingAudio() {
            return !isClosing()
                    && ChromeWebContentsDelegateAndroid.nativeIsCapturingAudio(getWebContents());
        }

        /**
         * @return Whether video is being captured.
         */
        private boolean isCapturingVideo() {
            return !isClosing()
                    && ChromeWebContentsDelegateAndroid.nativeIsCapturingVideo(getWebContents());
        }
    }

    private WebContentsObserver createWebContentsObserver(WebContents webContents) {
        return new WebContentsObserver(webContents) {
            @Override
            public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
                PolicyAuditor auditor =
                        ((ChromeApplication) getApplicationContext()).getPolicyAuditor();
                auditor.notifyAuditEvent(
                        getApplicationContext(), AuditEvent.OPEN_URL_SUCCESS, validatedUrl, "");
            }

            @Override
            public void didFailLoad(boolean isProvisionalLoad, boolean isMainFrame, int errorCode,
                    String description, String failingUrl, boolean wasIgnoredByHandler) {
                PolicyAuditor auditor =
                        ((ChromeApplication) getApplicationContext()).getPolicyAuditor();
                auditor.notifyAuditEvent(getApplicationContext(), AuditEvent.OPEN_URL_FAILURE,
                        failingUrl, description);
                if (errorCode == BLOCKED_BY_ADMINISTRATOR) {
                    auditor.notifyAuditEvent(
                            getApplicationContext(), AuditEvent.OPEN_URL_BLOCKED, failingUrl, "");
                }
            }

            @Override
            public void didCommitProvisionalLoadForFrame(
                    long frameId, boolean isMainFrame, String url, int transitionType) {
                if (!isMainFrame) return;

                mIsNativePageCommitPending = false;
                boolean isReload = (transitionType == PageTransition.RELOAD);
                if (!maybeShowNativePage(url, isReload)) {
                    showRenderedPage();
                }

                mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                mHandler.sendEmptyMessageDelayed(
                        MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, MAX_FULLSCREEN_LOAD_DELAY_MS);
                updateFullscreenEnabledState();

                if (getInterceptNavigationDelegate() != null) {
                    getInterceptNavigationDelegate().maybeUpdateNavigationHistory();
                }
            }

            @Override
            public void didAttachInterstitialPage() {
                PolicyAuditor auditor =
                        ((ChromeApplication) getApplicationContext()).getPolicyAuditor();
                auditor.notifyCertificateFailure(getWebContents(), getApplicationContext());
            }

            @Override
            public void didDetachInterstitialPage() {
                if (!maybeShowNativePage(getUrl(), false)) {
                    showRenderedPage();
                }
            }

            @Override
            public void destroy() {
                MediaCaptureNotificationService.updateMediaNotificationForTab(
                        getApplicationContext(), getId(), false, false, getUrl());
                super.destroy();
            }
        };
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

    private void enableFullscreenAfterLoad() {
        if (!mIsFullscreenWaitingForLoad) return;

        mIsFullscreenWaitingForLoad = false;
        updateFullscreenEnabledState();
    }

    @Override
    protected boolean isHidingTopControlsEnabled() {
        return super.isHidingTopControlsEnabled()  && !mIsFullscreenWaitingForLoad;
    }

    @Override
    protected void handleTabCrash() {
        super.handleTabCrash();

        // Update the most recent minidump file with the logcat. Doing this asynchronously
        // adds a race condition in the case of multiple simultaneously renderer crashses
        // but because the data will be the same for all of them it is innocuous. We can
        // attempt to do this regardless of whether it was a foreground tab in the event
        // that it's a real crash and not just android killing the tab.
        Context context = getApplicationContext();
        Intent intent = MinidumpUploadService.createFindAndUploadLastCrashIntent(context);
        context.startService(intent);
        RecordUserAction.record("MobileBreakpadUploadAttempt");
    }

    @Override
    protected void setContentViewCore(ContentViewCore cvc) {
        try {
            TraceEvent.begin("ChromeTab.setContentViewCore");
            super.setContentViewCore(cvc);
            mWebContentsObserver = createWebContentsObserver(cvc.getWebContents());

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
            // @see ChromeWebContentsDelegateAndroid#onLoadStopped()
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
    private boolean maybeShowNativePage(String url, boolean isReload) {
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
