// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.view.ActionMode;
import android.view.ContextMenu;
import android.view.KeyEvent;
import android.view.View;

import com.google.android.apps.chrome.R;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeMobileApplication;
import org.chromium.chrome.browser.CompositorChromeActivity;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.FrozenNativePage;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.TabUma;
import org.chromium.chrome.browser.TabUma.TabCreationState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuParams;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchTabHelper;
import org.chromium.chrome.browser.crash.MinidumpUploadService;
import org.chromium.chrome.browser.dom_distiller.ReaderModeActivityDelegate;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.download.ChromeDownloadDelegate;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.externalnav.ExternalNavigationParams;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.media.MediaNotificationService;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.ntp.NativePageAssassin;
import org.chromium.chrome.browser.ntp.NativePageFactory;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.tab.BackgroundContentViewHelper.BackgroundContentViewDelegate;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content.browser.ActivityContentVideoViewClient;
import org.chromium.content.browser.ContentVideoViewClient;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.SelectActionMode;
import org.chromium.content.browser.SelectActionModeCallback.ActionHandler;
import org.chromium.content.browser.crypto.CipherFactory;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.InvalidateTypes;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ConsoleMessageLevel;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.WindowOpenDisposition;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

import java.util.Locale;

/**
 * A representation of a Tab for Chrome. This manages wrapping a WebContents and interacting with
 * most of Chromium while representing a consistent version of web content to the front end.
 */
public class ChromeTab extends Tab {
    public static final int NTP_TAB_ID = -2;

    private static final String TAG = "ChromeTab";

    // URL didFailLoad error code. Should match the value in net_error_list.h.
    public static final int BLOCKED_BY_ADMINISTRATOR = -22;

    public static final String PAGESPEED_PASSTHROUGH_HEADER =
            "X-PSA-Client-Options: v=1,m=1\nCache-Control: no-cache";

    private static final int MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD = 1;

    /** The maximum amount of time to wait for a page to load before entering fullscreen.  -1 means
     *  wait until the page finishes loading. */
    private static final long MAX_FULLSCREEN_LOAD_DELAY_MS = 3000;

    private ReaderModeManager mReaderModeManager;

    private TabRedirectHandler mTabRedirectHandler;

    private ChromeDownloadDelegate mDownloadDelegate;

    private boolean mIsFullscreenWaitingForLoad = false;
    private ExternalNavigationHandler.OverrideUrlLoadingResult mLastOverrideUrlLoadingResult =
            ExternalNavigationHandler.OverrideUrlLoadingResult.NO_OVERRIDE;

    /**
     * Whether didCommitProvisionalLoadForFrame() hasn't yet been called for the current native page
     * (page A). To decrease latency, we show native pages in both loadUrl() and
     * didCommitProvisionalLoadForFrame(). However, we mustn't show a new native page (page B) in
     * loadUrl() if the current native page hasn't yet been committed. Otherwise, we'll show each
     * page twice (A, B, A, B): the first two times in loadUrl(), the second two times in
     * didCommitProvisionalLoadForFrame().
     */
    private boolean mIsNativePageCommitPending;

    protected final ChromeActivity mActivity;

    private WebContentsObserver mWebContentsObserver;

    private Handler mHandler;

    private final Runnable mCloseContentsRunnable = new Runnable() {
        @Override
        public void run() {
            mActivity.getTabModelSelector().closeTab(ChromeTab.this);
        }
    };

    /**
     * The data reduction proxy was in use on the last page load if true.
     */
    protected boolean mUsedSpdyProxy;

    /**
     * The data reduction proxy was in pass through mode on the last page load if true.
     */
    protected boolean mUsedSpdyProxyWithPassthrough;

    /**
     * The last page load had request headers indicating that the data reduction proxy should
     * be put in pass through mode, if true.
     */
    private boolean mLastPageLoadHasSpdyProxyPassthroughHeaders;

    /**
     * Listens to gesture events fired by the ContentViewCore.
     */
    private GestureStateListener mGestureStateListener;

    /**
     * The background content view helper which loads the original page in background content view.
     */
    private BackgroundContentViewHelper mBackgroundContentViewHelper;

    /**
     * The load progress of swapped in content view at the time of swap.
     */
    private int mLoadProgressAtViewSwapInTime;

    /**
     * Whether forward history should be cleared after navigation is committed.
     */
    private boolean mClearAllForwardHistoryRequired;

    private boolean mShouldClearRedirectHistoryForTabClobbering;

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

        if (frozenState == null) {
            assert type != TabLaunchType.FROM_RESTORE
                    && creationState != TabCreationState.FROZEN_ON_RESTORE;
        } else {
            assert type == TabLaunchType.FROM_RESTORE
                    && creationState == TabCreationState.FROZEN_ON_RESTORE;
        }

        addObserver(mTabObserver);
        mActivity = activity;
        mHandler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                if (msg == null) return;
                if (msg.what == MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD) {
                    enableFullscreenAfterLoad();
                }
            }
        };
        setContentViewClient(createContentViewClient());
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
    }

    /**
     * Creates a minimal {@link ChromeTab} for testing. Do not use outside testing.
     *
     * @param id        The id of the tab.
     * @param incognito Whether the tab is incognito.
     */
    @VisibleForTesting
    public ChromeTab(int id, boolean incognito) {
        super(id, incognito, null, null);
        mActivity = null;
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

    /**
     * Initializes the ChromeTab after construction with an existing ContentViewCore.
     */
    @Override
    protected void internalInit() {
        super.internalInit();
        if (mBackgroundContentViewHelper == null) {
            BackgroundContentViewDelegate delegate = new BackgroundContentViewDelegate() {
                @Override
                public void onBackgroundViewReady(
                        ContentViewCore cvc, boolean didStartLoad, boolean didFinishLoad,
                        int progress) {
                    WebContents previewWebContents = getWebContents();
                    swapContentViewCore(cvc, false, didStartLoad, didFinishLoad);
                    mLoadProgressAtViewSwapInTime = progress;
                    mBackgroundContentViewHelper.unloadAndDeleteWebContents(previewWebContents);

                    // Enter to fullscreen.
                    mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                    mHandler.sendEmptyMessageDelayed(
                            MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, MAX_FULLSCREEN_LOAD_DELAY_MS);
                    updateFullscreenEnabledState();
                }

                @Override
                public void onLoadProgressChanged(int progress) {
                    notifyLoadProgress(getProgress());
                }
            };
            mBackgroundContentViewHelper = new BackgroundContentViewHelper(
                    getWindowAndroid(), this, delegate);
        }
    }

    /**
     * Remember if the last load used the data reduction proxy, and if so,
     * also remember if it used pass through mode.
     */
    private void maybeSetDataReductionProxyUsed() {
        // Ignore internal URLs.
        String url = getUrl();
        if (url != null && url.toLowerCase(Locale.US).startsWith("chrome://")) {
            return;
        }
        mUsedSpdyProxy = false;
        mUsedSpdyProxyWithPassthrough = false;
        if (isSpdyProxyEnabledForUrl(url)) {
            mUsedSpdyProxy = true;
            if (mLastPageLoadHasSpdyProxyPassthroughHeaders) {
                mLastPageLoadHasSpdyProxyPassthroughHeaders = false;
                mUsedSpdyProxyWithPassthrough = true;
            }
        }
    }

    @Override
    protected void openNewTab(
            LoadUrlParams params, TabLaunchType launchType, Tab parentTab, boolean incognito) {
        mActivity.getTabModelSelector().openNewTab(params, launchType, parentTab, incognito);
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
        /**
         * This method is meant to be overridden by DocumentTab because the
         * TabModelSelector returned by the activity is not correct.
         * TODO(dfalcantara): remove this when DocumentActivity.getTabModelSelector()
         * will return the right TabModelSelector.
         */
        protected TabModel getTabModel() {
            return mActivity.getTabModelSelector().getModel(isIncognito());
        }

        @Override
        public boolean addNewContents(WebContents sourceWebContents, WebContents webContents,
                int disposition, Rect initialPosition, boolean userGesture) {
            if (isClosing()) return false;

            // TODO(johnme): Open tabs in same order as Chrome.
            Tab tab = mActivity.getTabCreator(isIncognito()).createTabWithWebContents(
                    webContents, getId(), TabLaunchType.FROM_LONGPRESS_FOREGROUND);

            if (tab == null) return false;

            if (disposition == WindowOpenDisposition.NEW_POPUP) {
                PolicyAuditor auditor =
                        ((ChromeMobileApplication) getApplicationContext()).getPolicyAuditor();
                auditor.notifyAuditEvent(getApplicationContext(), AuditEvent.OPEN_POPUP_URL_SUCCESS,
                        tab.getUrl(), "");
            }

            return true;
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
            Intent newIntent = new Intent();
            newIntent.setAction(Intent.ACTION_MAIN);
            newIntent.setPackage(mActivity.getPackageName());
            newIntent.putExtra(TabOpenType.BRING_TAB_TO_FRONT.name(),
                               ChromeTab.this.getId());

            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            getApplicationContext().startActivity(newIntent);
        }

        @Override
        public void navigationStateChanged(int flags) {
            if ((flags & InvalidateTypes.TAB) != 0) {
                MediaNotificationService.updateMediaNotificationForTab(
                        getApplicationContext(), getId(), isCapturingAudio(),
                        isCapturingVideo(), hasAudibleAudio(), getUrl());
            }
            super.navigationStateChanged(flags);
        }

        @Override
        public void onLoadProgressChanged(int progress) {
            if (!isLoading()) return;
            if (progress >= mLoadProgressAtViewSwapInTime) mLoadProgressAtViewSwapInTime = 0;
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
            return !isClosing() && super.nativeIsCapturingAudio(getWebContents());
        }

        /**
         * @return Whether video is being captured.
         */
        private boolean isCapturingVideo() {
            return !isClosing() && super.nativeIsCapturingVideo(getWebContents());
        }

        /**
         * @return Whether audio is being played.
         */
        private boolean hasAudibleAudio() {
            return !isClosing() && super.nativeHasAudibleAudio(getWebContents());
        }

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

    private class ChromeTabChromeContextMenuItemDelegate extends TabChromeContextMenuItemDelegate {
        private boolean mIsImage;
        private boolean mIsVideo;

        public void setParamsInfo(boolean isImage, boolean isVideo) {
            mIsImage = isImage;
            mIsVideo = isVideo;
        }

        @Override
        public boolean isIncognitoSupported() {
            return PrefServiceBridge.getInstance().isIncognitoModeEnabled();
        }

        @Override
        public boolean canLoadOriginalImage() {
            return mUsedSpdyProxy && !mUsedSpdyProxyWithPassthrough;
        }

        @Override
        public boolean startDownload(String url, boolean isLink) {
            if (isLink) {
                RecordUserAction.record("MobileContextMenuDownloadLink");
                if (shouldInterceptContextMenuDownload(url)) {
                    return false;
                }
            } else if (mIsImage) {
                RecordUserAction.record("MobileContextMenuDownloadImage");
            } else if (mIsVideo) {
                RecordUserAction.record("MobileContextMenuDownloadVideo");
            }
            return true;
        }

        @Override
        public void onSaveToClipboard(String text, boolean isUrl) {
            if (isUrl) {
                RecordUserAction.record("MobileContextMenuCopyLinkAddress");
            } else {
                RecordUserAction.record("MobileContextMenuCopyLinkText");
            }
            super.onSaveToClipboard(text, isUrl);
        }

        @Override
        public void onSaveImageToClipboard(String url) {
            RecordUserAction.record("MobileContextMenuSaveImage");
            super.onSaveImageToClipboard(url);
        }

        @Override
        public void onOpenInNewTab(String url, Referrer referrer) {
            RecordUserAction.record("MobileContextMenuOpenLinkInNewTab");
            RecordUserAction.record("MobileNewTabOpened");
            LoadUrlParams loadUrlParams = new LoadUrlParams(url);
            loadUrlParams.setReferrer(referrer);
            mActivity.getTabModelSelector().openNewTab(loadUrlParams,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND, ChromeTab.this, isIncognito());
        }

        @Override
        public void onOpenInNewIncognitoTab(String url) {
            RecordUserAction.record("MobileContextMenuOpenLinkInIncognito");
            RecordUserAction.record("MobileNewTabOpened");
            mActivity.getTabModelSelector().openNewTab(new LoadUrlParams(url),
                    TabLaunchType.FROM_LONGPRESS_FOREGROUND, ChromeTab.this, true);
        }

        @Override
        public void onOpenImageUrl(String url, Referrer referrer) {
            RecordUserAction.record("MobileContextMenuViewImage");
            super.onOpenImageUrl(url, referrer);
        }

        @Override
        public void onOpenImageInNewTab(String url, Referrer referrer) {
            boolean useOriginal = isSpdyProxyEnabledForUrl(url);
            RecordUserAction.record("MobileContextMenuOpenImageInNewTab");
            if (useOriginal) {
                RecordUserAction.record("MobileContextMenuOpenOriginalImageInNewTab");
            }

            LoadUrlParams loadUrlParams = new LoadUrlParams(url);
            loadUrlParams.setVerbatimHeaders(useOriginal ? PAGESPEED_PASSTHROUGH_HEADER : null);
            loadUrlParams.setReferrer(referrer);
            mActivity.getTabModelSelector().openNewTab(loadUrlParams,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND, ChromeTab.this, isIncognito());
        }

        @Override
        public void onSearchByImageInNewTab() {
            RecordUserAction.record("MobileContextMenuSearchByImage");
            super.onSearchByImageInNewTab();
        }
    }

    /**
     * This class is solely to track UMA stats.  When we upstream UMA stats we can remove this.
     */
    private static class ChromeTabChromeContextMenuPopulator extends ChromeContextMenuPopulator {
        private final ChromeTabChromeContextMenuItemDelegate mDelegate;

        public ChromeTabChromeContextMenuPopulator(
                ChromeTabChromeContextMenuItemDelegate delegate) {
            super(delegate);

            mDelegate = delegate;
        }

        @Override
        public void buildContextMenu(ContextMenu menu, Context context,
                ContextMenuParams params) {
            if (params.isAnchor()) {
                RecordUserAction.record("MobileContextMenuLink");
            } else if (params.isImage()) {
                RecordUserAction.record("MobileContextMenuImage");
            } else if (params.isSelectedText()) {
                RecordUserAction.record("MobileContextMenuText");
            } else if (params.isVideo()) {
                RecordUserAction.record("MobileContextMenuVideo");
            }

            mDelegate.setParamsInfo(params.isImage(), params.isVideo());
            super.buildContextMenu(menu, context, params);
        }
    }

    @Override
    protected ContextMenuPopulator createContextMenuPopulator() {
        return new ChromeTabChromeContextMenuPopulator(
                new ChromeTabChromeContextMenuItemDelegate());
    }

    /** @return The {@link BackgroundContentViewHelper} associated with the current tab. */
    public BackgroundContentViewHelper getBackgroundContentViewHelper() {
        return mBackgroundContentViewHelper;
    }

    @VisibleForTesting
    public void setViewClientForTesting(ContentViewClient client) {
        setContentViewClient(client);
    }

    @VisibleForTesting
    public ContentViewClient getViewClientForTesting() {
        return getContentViewClient();
    }

    private ContentViewClient createContentViewClient() {
        return new TabContentViewClient() {
            @Override
            public void onBackgroundColorChanged(int color) {
                ChromeTab.this.onBackgroundColorChanged(color);
            }

            @Override
            public void onOffsetsForFullscreenChanged(
                    float topControlsOffsetY, float contentOffsetY, float overdrawBottomHeight) {
                onOffsetsChanged(topControlsOffsetY, contentOffsetY, overdrawBottomHeight,
                        isShowingSadTab());
            }

            @Override
            public void performWebSearch(String searchQuery) {
                if (TextUtils.isEmpty(searchQuery)) return;
                String url = TemplateUrlService.getInstance().getUrlForSearchQuery(searchQuery);
                String headers = GeolocationHeader.getGeoHeader(getApplicationContext(), url,
                        isIncognito());

                LoadUrlParams loadUrlParams = new LoadUrlParams(url);
                loadUrlParams.setVerbatimHeaders(headers);
                loadUrlParams.setTransitionType(PageTransition.GENERATED);
                mActivity.getTabModelSelector().openNewTab(loadUrlParams,
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND, ChromeTab.this, isIncognito());
            }

            @Override
            public boolean doesPerformWebSearch() {
                return true;
            }

            @Override
            public SelectActionMode startActionMode(
                    View view, ActionHandler actionHandler, boolean floating) {
                if (floating) return null;
                ChromeSelectActionModeCallback callback =
                        new ChromeSelectActionModeCallback(view.getContext(), actionHandler);
                ActionMode actionMode = view.startActionMode(callback);
                return actionMode != null ? new SelectActionMode(actionMode) : null;
            }

            @Override
            public boolean supportsFloatingActionMode() {
                return false;
            }

            @Override
            public ContentVideoViewClient getContentVideoViewClient() {
                return new ActivityContentVideoViewClient(mActivity) {
                    @Override
                    public void enterFullscreenVideo(View view) {
                        super.enterFullscreenVideo(view);
                        FullscreenManager fullscreenManager = getFullscreenManager();
                        if (fullscreenManager != null) {
                            fullscreenManager.setOverlayVideoMode(true);
                            // Disable double tap for video.
                            if (getContentViewCore() != null) {
                                getContentViewCore().updateDoubleTapSupport(false);
                            }
                        }
                    }

                    @Override
                    public void exitFullscreenVideo() {
                        FullscreenManager fullscreenManager = getFullscreenManager();
                        if (fullscreenManager != null) {
                            fullscreenManager.setOverlayVideoMode(false);
                            // Disable double tap for video.
                            if (getContentViewCore() != null) {
                                getContentViewCore().updateDoubleTapSupport(true);
                            }
                        }
                        super.exitFullscreenVideo();
                    }
                };
            }
        };
    }

    private WebContentsObserver createWebContentsObserver(WebContents webContents) {
        return new WebContentsObserver(webContents) {
            @Override
            public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
                PolicyAuditor auditor =
                        ((ChromeMobileApplication) getApplicationContext()).getPolicyAuditor();
                auditor.notifyAuditEvent(
                        getApplicationContext(), AuditEvent.OPEN_URL_SUCCESS, validatedUrl, "");
            }

            @Override
            public void didFailLoad(boolean isProvisionalLoad,
                    boolean isMainFrame, int errorCode, String description, String failingUrl) {
                if (isMainFrame) {
                    if (failingUrl.startsWith("chrome://newtab/")
                            || failingUrl.startsWith("chrome://welcome/")) {
                        // If a tab was showing the old NTP or welcome page in a previous version
                        // of Chrome, that tab will appear as a blank page after Chrome updates.
                        // To fix this, these obsolete URLs are redirected to the NTP.
                        // TODO(newt): remove this once most users have upgraded past M39.
                        // http://crbug.com/491878
                        loadUrl(new LoadUrlParams(UrlConstants.NTP_URL,
                                PageTransition.AUTO_TOPLEVEL));
                        return;
                    }
                }

                PolicyAuditor auditor =
                        ((ChromeMobileApplication) getApplicationContext()).getPolicyAuditor();
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

                if (!mBackgroundContentViewHelper.hasPendingBackgroundPage()) {
                    mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                    mHandler.sendEmptyMessageDelayed(
                            MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, MAX_FULLSCREEN_LOAD_DELAY_MS);
                    updateFullscreenEnabledState();
                }

                // http://crbug/426679 : if navigation is canceled due to intent handling, we want
                // to go back to the last committed entry index which was saved before the
                // navigation, and remove the empty entries from the navigation history.
                if (mClearAllForwardHistoryRequired && getWebContents() != null) {
                    NavigationController navigationController =
                            getWebContents().getNavigationController();
                    int lastCommittedEntryIndex = getLastCommittedEntryIndex();
                    while (navigationController.canGoForward()) {
                        boolean ret = navigationController.removeEntryAtIndex(
                                lastCommittedEntryIndex + 1);
                        assert ret;
                    }
                } else if (mShouldClearRedirectHistoryForTabClobbering
                        && getWebContents() != null) {
                    // http://crbug/479056: Even if we clobber the current tab, we want to remove
                    // redirect history to be consistent.
                    NavigationController navigationController =
                            getWebContents().getNavigationController();
                    int indexBeforeRedirection = mTabRedirectHandler
                            .getLastCommittedEntryIndexBeforeStartingNavigation();
                    int lastCommittedEntryIndex = getLastCommittedEntryIndex();
                    for (int i = lastCommittedEntryIndex - 1; i > indexBeforeRedirection; --i) {
                        boolean ret = navigationController.removeEntryAtIndex(i);
                        assert ret;
                    }
                }
                mClearAllForwardHistoryRequired = false;
                mShouldClearRedirectHistoryForTabClobbering = false;
            }

            @Override
            public void didAttachInterstitialPage() {
                PolicyAuditor auditor =
                        ((ChromeMobileApplication) getApplicationContext()).getPolicyAuditor();
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
                MediaNotificationService.updateMediaNotificationForTab(
                        getApplicationContext(), getId(), false, false, false, getUrl());
                super.destroy();
            }
        };
    }

    @Override
    protected void didStartPageLoad(String validatedUrl, boolean showingErrorPage) {
        if (mBackgroundContentViewHelper.isPageSwappingInProgress()) {
            mLoadProgressAtViewSwapInTime = 0;
        }

        mIsFullscreenWaitingForLoad = !DomDistillerUrlUtils.isDistilledPage(validatedUrl);
        mLoadProgressAtViewSwapInTime = 0;

        super.didStartPageLoad(validatedUrl, showingErrorPage);
    }

    @Override
    protected void didFinishPageLoad() {
        // We should not mark finished if we have pending background page to swap in.
        // We'll instead call didFinishPageLoad after the background page is swapped in.
        if (mBackgroundContentViewHelper.hasPendingBackgroundPage()) return;

        mLoadProgressAtViewSwapInTime = 0;

        super.didFinishPageLoad();

        // Handle the case where we were pre-renderered and the enable fullscreen message was
        // never enqueued.
        if (mIsFullscreenWaitingForLoad
                && !mHandler.hasMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD)
                && !mBackgroundContentViewHelper.hasPendingBackgroundPage()) {
            mHandler.sendEmptyMessageDelayed(
                    MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, MAX_FULLSCREEN_LOAD_DELAY_MS);
        }

        maybeSetDataReductionProxyUsed();
    }

    @Override
    protected void didFailPageLoad(int errorCode) {
        mLoadProgressAtViewSwapInTime = 0;
        cancelEnableFullscreenLoadDelay();
        super.didFailPageLoad(errorCode);
        updateFullscreenEnabledState();
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

            mDownloadDelegate = new ChromeDownloadDelegate(mActivity,
                    mActivity.getTabModelSelector(), this);
            cvc.setDownloadDelegate(mDownloadDelegate);
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

    @Override
    public void stopLoading() {
        super.stopLoading();
        mBackgroundContentViewHelper.stopLoading();
    }

    @Override
    public int getProgress() {
        if (mBackgroundContentViewHelper.hasPendingBackgroundPage()
                || mBackgroundContentViewHelper.isPageSwappingInProgress()) {
            return mBackgroundContentViewHelper.getProgress();
        }
        int currentTabProgress = super.getProgress();
        return Math.max(mLoadProgressAtViewSwapInTime, currentTabProgress);
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

    /**
     * Checks if spdy proxy is enabled for input url.
     * @param url Input url to check for spdy setting.
     * @return true if url is enabled for spdy proxy.
    */
    protected boolean isSpdyProxyEnabledForUrl(String url) {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()
                && url != null && !url.toLowerCase(Locale.US).startsWith("https://")
                && !isIncognito()) {
            return true;
        }
        return false;
    }

    @Override
    public void goBack() {
        mBackgroundContentViewHelper.recordBack();
        super.goBack();
    }

    @Override
    public void reload() {
        mBackgroundContentViewHelper.recordReload();
        super.reload();
    }

    @Override
    public void reloadIgnoringCache() {
        mBackgroundContentViewHelper.recordReload();
        super.reloadIgnoringCache();
    }

    @Override
    public int loadUrl(LoadUrlParams params) {
        try {
            TraceEvent.begin("ChromeTab.loadUrl");

            // The data reduction proxy can only be set to pass through mode via loading an image in
            // a new tab. We squirrel away whether pass through mode was set, and check it in:
            // @see ChromeWebContentsDelegateAndroid#onLoadStopped()
            mLastPageLoadHasSpdyProxyPassthroughHeaders = false;
            if (TextUtils.equals(params.getVerbatimHeaders(), PAGESPEED_PASSTHROUGH_HEADER)) {
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
        return getInterceptNavigationDelegate().mAuthenticatorHelper;
    }

    /**
     * @return the TabRedirectHandler for the tab.
     */
    public TabRedirectHandler getTabRedirectHandler() {
        return mTabRedirectHandler;
    }

    /**
     * Delegate that handles intercepting top-level navigation.
     */
    protected class InterceptNavigationDelegateImpl implements InterceptNavigationDelegate {
        final ExternalNavigationHandler mExternalNavHandler;
        final AuthenticatorNavigationInterceptor mAuthenticatorHelper;

        /**
         * Defualt constructor of {@link InterceptNavigationDelegateImpl}.
         */
        public InterceptNavigationDelegateImpl() {
            this(new ExternalNavigationHandler(mActivity));
        }

        /**
         * Constructs a new instance of {@link InterceptNavigationDelegateImpl} with the given
         * {@link ExternalNavigationHandler}.
         */
        public InterceptNavigationDelegateImpl(ExternalNavigationHandler externalNavHandler) {
            mExternalNavHandler = externalNavHandler;
            mAuthenticatorHelper = ((ChromeMobileApplication) getApplicationContext())
                    .createAuthenticatorNavigationInterceptor(ChromeTab.this);
        }

        public boolean shouldIgnoreNewTab(String url, boolean incognito) {
            if (mAuthenticatorHelper != null && mAuthenticatorHelper.handleAuthenticatorUrl(url)) {
                return true;
            }

            ExternalNavigationParams params = new ExternalNavigationParams.Builder(url, incognito)
                    .setTab(ChromeTab.this)
                    .setOpenInNewTab(true)
                    .build();
            return mExternalNavHandler.shouldOverrideUrlLoading(params)
                    != ExternalNavigationHandler.OverrideUrlLoadingResult.NO_OVERRIDE;
        }

        @Override
        public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
            final String url = navigationParams.url;

            if (mAuthenticatorHelper != null && mAuthenticatorHelper.handleAuthenticatorUrl(url)) {
                return true;
            }

            mTabRedirectHandler.updateNewUrlLoading(navigationParams.pageTransitionType,
                    navigationParams.isRedirect,
                    navigationParams.hasUserGesture || navigationParams.hasUserGestureCarryover,
                    mActivity.getLastUserInteractionTime(), getLastCommittedEntryIndex());
            final boolean shouldCloseTab = shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent();
            boolean isInitialTabLaunchInBackground =
                    getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND && shouldCloseTab;
            // http://crbug.com/448977: If a new tab is closed by this overriding, we should open an
            // Intent in a new tab when Chrome receives it again.
            ExternalNavigationParams params = new ExternalNavigationParams.Builder(
                    url, isIncognito(), navigationParams.referrer,
                    navigationParams.pageTransitionType,
                    navigationParams.isRedirect)
                    .setTab(ChromeTab.this)
                    .setApplicationMustBeInForeground(true)
                    .setRedirectHandler(mTabRedirectHandler)
                    .setOpenInNewTab(shouldCloseTab)
                    .setIsBackgroundTabNavigation(isHidden() && !isInitialTabLaunchInBackground)
                    .setIsMainFrame(navigationParams.isMainFrame)
                    .setNeedsToCloseTabAfterIncognitoDialog(shouldCloseTab
                            && navigationParams.isMainFrame)
                    .build();
            ExternalNavigationHandler.OverrideUrlLoadingResult result =
                    mExternalNavHandler.shouldOverrideUrlLoading(params);
            mLastOverrideUrlLoadingResult = result;
            switch (result) {
                case OVERRIDE_WITH_EXTERNAL_INTENT:
                    assert mExternalNavHandler.canExternalAppHandleUrl(url);
                    if (navigationParams.isMainFrame) {
                        onOverrideUrlLoadingAndLaunchIntent();
                    }
                    return true;
                case OVERRIDE_WITH_CLOBBERING_TAB:
                    mShouldClearRedirectHistoryForTabClobbering = true;
                    return true;
                case OVERRIDE_WITH_INCOGNITO_MODE:
                    if (!shouldCloseTab && navigationParams.isMainFrame) {
                        onOverrideUrlLoadingAndLaunchIntent();
                    }
                    return true;
                case NO_OVERRIDE:
                default:
                    if (navigationParams.isExternalProtocol) {
                        logBlockedNavigationToDevToolsConsole(url);
                        return true;
                    }
                    return false;
            }
        }

        private void logBlockedNavigationToDevToolsConsole(String url) {
            int resId = mExternalNavHandler.canExternalAppHandleUrl(url)
                    ? R.string.blocked_navigation_warning
                    : R.string.unreachable_navigation_warning;
            getWebContents().addMessageToDevToolsConsole(
                    ConsoleMessageLevel.WARNING, getApplicationContext().getString(resId, url));
        }
    }

    @Override
    protected boolean shouldIgnoreNewTab(String url, boolean incognito) {
        InterceptNavigationDelegateImpl delegate = getInterceptNavigationDelegate();
        return delegate != null && delegate.shouldIgnoreNewTab(url, incognito);
    }

    private int getLastCommittedEntryIndex() {
        if (getWebContents() == null) return -1;
        return getWebContents().getNavigationController().getLastCommittedEntryIndex();
    }

    /**
     * @return A potential fallback texture id to use when trying to draw this tab.
     */
    public int getFallbackTextureId() {
        return INVALID_TAB_ID;
    }

    private boolean shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent() {
        if (getWebContents() == null) return false;
        if (!getWebContents().getNavigationController().canGoToOffset(0)) return true;

        // http://crbug/415948 : if the last committed entry index which was saved before this
        // navigation is invalid, it means that this navigation is the first one since this tab was
        // created.
        // In such case, we would like to close this tab.
        if (mTabRedirectHandler.isOnNavigation()) {
            return mTabRedirectHandler.getLastCommittedEntryIndexBeforeStartingNavigation()
                    == TabRedirectHandler.INVALID_ENTRY_INDEX;
        }
        return false;
    }

    /**
     * Called when Chrome decides to override URL loading and show an intent picker.
     */
    protected void onOverrideUrlLoadingAndLaunchIntent() {
        if (getWebContents() == null) return;

        // Before leaving Chrome, close the empty child tab.
        // If a new tab is created through JavaScript open to load this
        // url, we would like to close it as we will load this url in a
        // different Activity.
        if (shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent()) {
            getChromeWebContentsDelegateAndroid().closeContents();
        } else if (mTabRedirectHandler.isOnNavigation()) {
            int lastCommittedEntryIndexBeforeNavigation =
                    mTabRedirectHandler.getLastCommittedEntryIndexBeforeStartingNavigation();
            if (getLastCommittedEntryIndex() > lastCommittedEntryIndexBeforeNavigation) {
                // http://crbug/426679 : we want to go back to the last committed entry index which
                // was saved before this navigation, and remove the empty entries from the
                // navigation history.
                mClearAllForwardHistoryRequired = true;
                getWebContents().getNavigationController().goToNavigationIndex(
                        lastCommittedEntryIndexBeforeNavigation);
            }
        }
    }

    @Override
    protected InterceptNavigationDelegateImpl getInterceptNavigationDelegate() {
        return (InterceptNavigationDelegateImpl) super.getInterceptNavigationDelegate();
    }

    /**
     * Factory method for {@link InterceptNavigationDelegateImpl}. Meant to be overridden by
     * subclasses.
     * @return A new instance of {@link InterceptNavigationDelegateImpl}.
     */
    protected InterceptNavigationDelegateImpl createInterceptNavigationDelegate() {
        return new InterceptNavigationDelegateImpl();
    }

    @Override
    public void setClosing(boolean closing) {
        if (closing) mBackgroundContentViewHelper.recordTabClose();
        super.setClosing(closing);
    }

    /**
     * @return The reader mode manager for this tab that handles UI events for reader mode.
     */
    public ReaderModeManager getReaderModeManager() {
        return mReaderModeManager;
    }

    public ReaderModeActivityDelegate getReaderModeActivityDelegate() {
        if (!(mActivity instanceof CompositorChromeActivity)) return null;
        return ((CompositorChromeActivity) mActivity).getReaderModeActivityDelegate();
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
        public void onContentChanged(Tab tab) {
            mLoadProgressAtViewSwapInTime = 0;
        }

        @Override
        public void onSSLStateUpdated(Tab tab) {
            PolicyAuditor auditor =
                    ((ChromeMobileApplication) getApplicationContext()).getPolicyAuditor();
            auditor.notifyCertificateFailure(getWebContents(), getApplicationContext());
            updateFullscreenEnabledState();
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
    };

    @VisibleForTesting
    public OverrideUrlLoadingResult getLastOverrideUrlLoadingResultForTests() {
        return mLastOverrideUrlLoadingResult;
    }
}
