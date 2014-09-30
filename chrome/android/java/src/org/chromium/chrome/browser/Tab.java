// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.ContextMenu;
import android.view.View;

import org.chromium.base.CalledByNative;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItemDelegate;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuParams;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorWrapper;
import org.chromium.chrome.browser.contextmenu.EmptyChromeContextMenuItemDelegate;
import org.chromium.chrome.browser.dom_distiller.DomDistillerFeedbackReporter;
import org.chromium.chrome.browser.infobar.AutoLoginProcessor;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.toolbar.ToolbarModelSecurityLevel;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.NavigationClient;
import org.chromium.content.browser.WebContentsObserverAndroid;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * The basic Java representation of a tab.  Contains and manages a {@link ContentView}.
 *
 * Tab provides common functionality for ChromeShell Tab as well as Chrome on Android's
 * tab. It is intended to be extended either on Java or both Java and C++, with ownership managed
 * by this base class.
 *
 * Extending just Java:
 *  - Just extend the class normally.  Do not override initializeNative().
 * Extending Java and C++:
 *  - Because of the inner-workings of JNI, the subclass is responsible for constructing the native
 *    subclass, which in turn constructs TabAndroid (the native counterpart to Tab), which in
 *    turn sets the native pointer for Tab.  For destruction, subclasses in Java must clear
 *    their own native pointer reference, but Tab#destroy() will handle deleting the native
 *    object.
 *
 * Notes on {@link Tab#getId()}:
 *
 *    Tabs are all generated using a static {@link AtomicInteger} which means they are unique across
 *  all {@link Activity}s running in the same {@link android.app.Application} process.  Calling
 *  {@link Tab#incrementIdCounterTo(int)} will ensure new {@link Tab}s get ids greater than or equal
 *  to the parameter passed to that method.  This should be used when doing things like loading
 *  persisted {@link Tab}s from disk on process start to ensure all new {@link Tab}s don't have id
 *  collision.
 *    Some {@link Activity}s will not call this because they do not persist state, which means those
 *  ids can potentially conflict with the ones restored from persisted state depending on which
 *  {@link Activity} runs first on process start.  If {@link Tab}s are ever shared across
 *  {@link Activity}s or mixed with {@link Tab}s from other {@link Activity}s conflicts can occur
 *  unless special care is taken to make sure {@link Tab#incrementIdCounterTo(int)} is called with
 *  the correct value across all affected {@link Activity}s.
 */
public class Tab implements NavigationClient {
    public static final int INVALID_TAB_ID = -1;

    /** Used for automatically generating tab ids. */
    private static final AtomicInteger sIdCounter = new AtomicInteger();

    private long mNativeTabAndroid;

    /** Unique id of this tab (within its container). */
    private final int mId;

    /** Whether or not this tab is an incognito tab. */
    private final boolean mIncognito;

    /** An Application {@link Context}.  Unlike {@link #mContext}, this is the only one that is
     * publicly exposed to help prevent leaking the {@link Activity}. */
    private final Context mApplicationContext;

    /** The {@link Context} used to create {@link View}s and other Android components.  Unlike
     * {@link #mApplicationContext}, this is not publicly exposed to help prevent leaking the
     * {@link Activity}. */
    private final Context mContext;

    /** Gives {@link Tab} a way to interact with the Android window. */
    private final WindowAndroid mWindowAndroid;

    /** The current native page (e.g. chrome-native://newtab), or {@code null} if there is none. */
    private NativePage mNativePage;

    /** InfoBar container to show InfoBars for this tab. */
    private InfoBarContainer mInfoBarContainer;

    /** Manages app banners shown for this tab. */
    private AppBannerManager mAppBannerManager;

    /** The sync id of the Tab if session sync is enabled. */
    private int mSyncId;

    /**
     * The {@link ContentViewCore} showing the current page or {@code null} if the tab is frozen.
     */
    private ContentViewCore mContentViewCore;

    /**
     * A list of Tab observers.  These are used to broadcast Tab events to listeners.
     */
    private final ObserverList<TabObserver> mObservers = new ObserverList<TabObserver>();

    // Content layer Observers and Delegates
    private ContentViewClient mContentViewClient;
    private WebContentsObserverAndroid mWebContentsObserver;
    private VoiceSearchTabHelper mVoiceSearchTabHelper;
    private TabChromeWebContentsDelegateAndroid mWebContentsDelegate;
    private DomDistillerFeedbackReporter mDomDistillerFeedbackReporter;

    /**
     * If this tab was opened from another tab, store the id of the tab that
     * caused it to be opened so that we can activate it when this tab gets
     * closed.
     */
    private int mParentId = INVALID_TAB_ID;

    /**
     * Whether the tab should be grouped with its parent tab.
     */
    private boolean mGroupedWithParent = true;

    private boolean mIsClosing = false;

    private Bitmap mFavicon = null;

    private String mFaviconUrl = null;

    /**
     * A default {@link ChromeContextMenuItemDelegate} that supports some of the context menu
     * functionality.
     */
    protected class TabChromeContextMenuItemDelegate
            extends EmptyChromeContextMenuItemDelegate {
        private final Clipboard mClipboard;

        /**
         * Builds a {@link TabChromeContextMenuItemDelegate} instance.
         */
        public TabChromeContextMenuItemDelegate() {
            mClipboard = new Clipboard(getApplicationContext());
        }

        @Override
        public boolean isIncognito() {
            return mIncognito;
        }

        @Override
        public void onSaveToClipboard(String text, boolean isUrl) {
            mClipboard.setText(text, text);
        }

        @Override
        public void onSaveImageToClipboard(String url) {
            mClipboard.setHTMLText("<img src=\"" + url + "\">", url, url);
        }

        @Override
        public String getPageUrl() {
            return getUrl();
        }
    }

    /**
     * A basic {@link ChromeWebContentsDelegateAndroid} that forwards some calls to the registered
     * {@link TabObserver}s.  Meant to be overridden by subclasses.
     */
    public class TabChromeWebContentsDelegateAndroid
            extends ChromeWebContentsDelegateAndroid {
        @Override
        public void onLoadProgressChanged(int progress) {
            for (TabObserver observer : mObservers) {
                observer.onLoadProgressChanged(Tab.this, progress);
            }
        }

        @Override
        public void onLoadStarted() {
            for (TabObserver observer : mObservers) observer.onLoadStarted(Tab.this);
        }

        @Override
        public void onLoadStopped() {
            for (TabObserver observer : mObservers) observer.onLoadStopped(Tab.this);
        }

        @Override
        public void onUpdateUrl(String url) {
            for (TabObserver observer : mObservers) observer.onUpdateUrl(Tab.this, url);
        }

        @Override
        public void showRepostFormWarningDialog(final ContentViewCore contentViewCore) {
            RepostFormWarningDialog warningDialog = new RepostFormWarningDialog(
                    new Runnable() {
                        @Override
                        public void run() {
                            getWebContents().getNavigationController().cancelPendingReload();
                        }
                    }, new Runnable() {
                        @Override
                        public void run() {
                            getWebContents().getNavigationController().continuePendingReload();
                        }
                    });
            Activity activity = (Activity) mContext;
            warningDialog.show(activity.getFragmentManager(), null);
        }

        @Override
        public void toggleFullscreenModeForTab(boolean enableFullscreen) {
            for (TabObserver observer : mObservers) {
                observer.onToggleFullscreenMode(Tab.this, enableFullscreen);
            }
        }

        @Override
        public void navigationStateChanged(int flags) {
            if ((flags & INVALIDATE_TYPE_TITLE) != 0) {
                for (TabObserver observer : mObservers) observer.onTitleUpdated(Tab.this);
            }
            if ((flags & INVALIDATE_TYPE_URL) != 0) {
                for (TabObserver observer : mObservers) observer.onUrlUpdated(Tab.this);
            }
        }

        @Override
        public void visibleSSLStateChanged() {
            for (TabObserver observer : mObservers) observer.onSSLStateUpdated(Tab.this);
        }
    }

    private class TabContextMenuPopulator extends ContextMenuPopulatorWrapper {
        public TabContextMenuPopulator(ContextMenuPopulator populator) {
            super(populator);
        }

        @Override
        public void buildContextMenu(ContextMenu menu, Context context, ContextMenuParams params) {
            super.buildContextMenu(menu, context, params);
            for (TabObserver observer : mObservers) observer.onContextMenuShown(Tab.this, menu);
        }
    }

    private class TabWebContentsObserverAndroid extends WebContentsObserverAndroid {
        public TabWebContentsObserverAndroid(WebContents webContents) {
            super(webContents);
        }

        @Override
        public void navigationEntryCommitted() {
            if (getNativePage() != null) {
                pushNativePageStateToNavigationEntry();
            }
        }

        @Override
        public void didFailLoad(boolean isProvisionalLoad, boolean isMainFrame, int errorCode,
                String description, String failingUrl) {
            for (TabObserver observer : mObservers) {
                observer.onDidFailLoad(Tab.this, isProvisionalLoad, isMainFrame, errorCode,
                        description, failingUrl);
            }
        }

        @Override
        public void didStartProvisionalLoadForFrame(long frameId, long parentFrameId,
                boolean isMainFrame, String validatedUrl, boolean isErrorPage,
                boolean isIframeSrcdoc) {
            for (TabObserver observer : mObservers) {
                observer.onDidStartProvisionalLoadForFrame(Tab.this, frameId, parentFrameId,
                        isMainFrame, validatedUrl, isErrorPage, isIframeSrcdoc);
            }
        }

        @Override
        public void didChangeThemeColor(int color) {
            for (TabObserver observer : mObservers) {
                observer.onDidChangeThemeColor(color);
            }
        }
    }

    /**
     * Creates an instance of a {@link Tab} with no id.
     * @param incognito Whether or not this tab is incognito.
     * @param context   An instance of a {@link Context}.
     * @param window    An instance of a {@link WindowAndroid}.
     */
    @VisibleForTesting
    public Tab(boolean incognito, Context context, WindowAndroid window) {
        this(INVALID_TAB_ID, incognito, context, window);
    }

    /**
     * Creates an instance of a {@link Tab}.
     * @param id        The id this tab should be identified with.
     * @param incognito Whether or not this tab is incognito.
     * @param context   An instance of a {@link Context}.
     * @param window    An instance of a {@link WindowAndroid}.
     */
    public Tab(int id, boolean incognito, Context context, WindowAndroid window) {
        this(INVALID_TAB_ID, id, incognito, context, window);
    }

    /**
     * Creates an instance of a {@link Tab}.
     * @param id        The id this tab should be identified with.
     * @param parentId  The id id of the tab that caused this tab to be opened.
     * @param incognito Whether or not this tab is incognito.
     * @param context   An instance of a {@link Context}.
     * @param window    An instance of a {@link WindowAndroid}.
     */
    public Tab(int id, int parentId, boolean incognito, Context context, WindowAndroid window) {
        // We need a valid Activity Context to build the ContentView with.
        assert context == null || context instanceof Activity;

        mId = generateValidId(id);
        mParentId = parentId;
        mIncognito = incognito;
        // TODO(dtrainor): Only store application context here.
        mContext = context;
        mApplicationContext = context != null ? context.getApplicationContext() : null;
        mWindowAndroid = window;
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
        return getWebContents() != null && getWebContents().getNavigationController()
                .canGoForward();
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

    @Override
    public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
        if (getWebContents() != null) {
            return getWebContents().getNavigationController()
                    .getDirectedNavigationHistory(isForward, itemLimit);
        } else {
            return new NavigationHistory();
        }
    }

    @Override
    public void goToNavigationIndex(int index) {
        if (getWebContents() != null) {
            getWebContents().getNavigationController().goToNavigationIndex(index);
        }
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
        TraceEvent.begin();

        // We load the URL from the tab rather than directly from the ContentView so the tab has a
        // chance of using a prerenderer page is any.
        int loadType = nativeLoadUrl(
                mNativeTabAndroid,
                params.getUrl(),
                params.getVerbatimHeaders(),
                params.getPostData(),
                params.getTransitionType(),
                params.getReferrer() != null ? params.getReferrer().getUrl() : null,
                // Policy will be ignored for null referrer url, 0 is just a placeholder.
                // TODO(ppi): Should we pass Referrer jobject and add JNI methods to read it from
                //            the native?
                params.getReferrer() != null ? params.getReferrer().getPolicy() : 0,
                params.getIsRendererInitiated());

        TraceEvent.end();

        for (TabObserver observer : mObservers) {
            observer.onLoadUrl(this, params.getUrl(), loadType);
        }
        return loadType;
    }

    /**
     * @return Whether or not the {@link Tab} is currently showing an interstitial page, such as
     *         a bad HTTPS page.
     */
    public boolean isShowingInterstitialPage() {
        return getWebContents() != null && getWebContents().isShowingInterstitialPage();
    }

    /**
     * @return Whether or not the tab has something valid to render.
     */
    public boolean isReady() {
        return mNativePage != null || (mContentViewCore != null && mContentViewCore.isReady());
    }

    /**
     * @return The {@link View} displaying the current page in the tab. This might be a
     *         native view or a placeholder view for content rendered by the compositor.
     *         This can be {@code null}, if the tab is frozen or being initialized or destroyed.
     */
    public View getView() {
        return mNativePage != null ? mNativePage.getView() :
                (mContentViewCore != null ? mContentViewCore.getContainerView() : null);
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
        return mApplicationContext;
    }

    /**
     * @return The infobar container.
     */
    public final InfoBarContainer getInfoBarContainer() {
        return mInfoBarContainer;
    }

    /**
     * Create an {@code AutoLoginProcessor} to decide how to handle login
     * requests.
     */
    protected AutoLoginProcessor createAutoLoginProcessor() {
        return new AutoLoginProcessor() {
            @Override
            public void processAutoLoginResult(String accountName, String authToken,
                    boolean success, String result) {
            }
        };
    }

    /**
     * Prints the current page.
     *
     * @return Whether the printing process is started successfully.
     **/
    public boolean print() {
        assert mNativeTabAndroid != 0;
        return nativePrint(mNativeTabAndroid);
    }

    /**
     * Reloads the current page content.
     */
    public void reload() {
        // TODO(dtrainor): Should we try to rebuild the ContentView if it's frozen?
        if (getWebContents() != null) getWebContents().getNavigationController().reload(true);
    }

    /**
     * Reloads the current page content.
     * This version ignores the cache and reloads from the network.
     */
    public void reloadIgnoringCache() {
        if (getWebContents() != null) {
            getWebContents().getNavigationController().reloadIgnoringCache(true);
        }
    }

    /** Stop the current navigation. */
    public void stopLoading() {
        if (getWebContents() != null) getWebContents().stop();
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
     * @return The web contents associated with this tab.
     */
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

    /**
     * @return Whether or not this tab is incognito.
     */
    public boolean isIncognito() {
        return mIncognito;
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
            getWebContents().getNavigationController()
                    .setUseDesktopUserAgent(useDesktop, reloadOnChange);
        }
    }

    /**
     * @return Whether or not the {@link ContentViewCore} is using a desktop user agent.
     */
    public boolean getUseDesktopUserAgent() {
        return getWebContents() != null && getWebContents().getNavigationController()
                .getUseDesktopUserAgent();
    }

    /**
     * @return The current {ToolbarModelSecurityLevel} for the tab.
     */
    public int getSecurityLevel() {
        if (mNativeTabAndroid == 0) return ToolbarModelSecurityLevel.NONE;
        return nativeGetSecurityLevel(mNativeTabAndroid);
    }

    /**
     * @return The sync id of the tab if session sync is enabled, {@code 0} otherwise.
     */
    @CalledByNative
    protected int getSyncId() {
        return mSyncId;
    }

    /**
     * @param syncId The sync id of the tab if session sync is enabled.
     */
    @CalledByNative
    protected void setSyncId(int syncId) {
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
     * @return The {@link ContentViewClient} currently bound to any {@link ContentViewCore}
     *         associated with the current page.  There can still be a {@link ContentViewClient}
     *         even when there is no {@link ContentViewCore}.
     */
    protected ContentViewClient getContentViewClient() {
        return mContentViewClient;
    }

    /**
     * @param client The {@link ContentViewClient} to be bound to any current or new
     *               {@link ContentViewCore}s associated with this {@link Tab}.
     */
    protected void setContentViewClient(ContentViewClient client) {
        if (mContentViewClient == client) return;

        ContentViewClient oldClient = mContentViewClient;
        mContentViewClient = client;

        if (mContentViewCore == null) return;

        if (mContentViewClient != null) {
            mContentViewCore.setContentViewClient(mContentViewClient);
        } else if (oldClient != null) {
            // We can't set a null client, but we should clear references to the last one.
            mContentViewCore.setContentViewClient(new ContentViewClient());
        }
    }

    /**
     * Triggers the showing logic for the view backing this tab.
     */
    protected void show() {
        if (mContentViewCore != null) mContentViewCore.onShow();
    }

    /**
     * Triggers the hiding logic for the view backing the tab.
     */
    protected void hide() {
        if (mContentViewCore != null) mContentViewCore.onHide();
    }

    /**
     * Shows the given {@code nativePage} if it's not already showing.
     * @param nativePage The {@link NativePage} to show.
     */
    protected void showNativePage(NativePage nativePage) {
        if (mNativePage == nativePage) return;
        NativePage previousNativePage = mNativePage;
        mNativePage = nativePage;
        pushNativePageStateToNavigationEntry();
        for (TabObserver observer : mObservers) observer.onContentChanged(this);
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
    }

    /**
     * Hides the current {@link NativePage}, if any, and shows the {@link ContentViewCore}'s view.
     */
    protected void showRenderedPage() {
        if (mNativePage == null) return;
        NativePage previousNativePage = mNativePage;
        mNativePage = null;
        for (TabObserver observer : mObservers) observer.onContentChanged(this);
        destroyNativePageInternal(previousNativePage);
    }

    /**
     * Initializes this {@link Tab}.
     */
    public void initialize() {
        initializeNative();
    }

    /**
     * Builds the native counterpart to this class.  Meant to be overridden by subclasses to build
     * subclass native counterparts instead.  Subclasses should not call this via super and instead
     * rely on the native class to create the JNI association.
     */
    protected void initializeNative() {
        if (mNativeTabAndroid == 0) nativeInit();
        assert mNativeTabAndroid != 0;
    }

    /**
     * A helper method to initialize a {@link ContentViewCore} without any
     * native WebContents pointer.
     */
    protected final void initContentViewCore() {
        initContentViewCore(ContentViewUtil.createNativeWebContents(mIncognito));
    }

    /**
     * Creates and initializes the {@link ContentViewCore}.
     *
     * @param nativeWebContents The native web contents pointer.
     */
    protected void initContentViewCore(long nativeWebContents) {
        ContentViewCore cvc = new ContentViewCore(mContext);
        ContentView cv = ContentView.newInstance(mContext, cvc);
        cvc.initialize(cv, cv, nativeWebContents, getWindowAndroid());
        setContentViewCore(cvc);
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
    protected void setContentViewCore(ContentViewCore cvc) {
        NativePage previousNativePage = mNativePage;
        mNativePage = null;
        destroyNativePageInternal(previousNativePage);

        mContentViewCore = cvc;

        mWebContentsDelegate = createWebContentsDelegate();
        mWebContentsObserver = new TabWebContentsObserverAndroid(mContentViewCore.getWebContents());
        mVoiceSearchTabHelper = new VoiceSearchTabHelper(mContentViewCore.getWebContents());

        if (mContentViewClient != null) mContentViewCore.setContentViewClient(mContentViewClient);

        assert mNativeTabAndroid != 0;
        nativeInitWebContents(
                mNativeTabAndroid, mIncognito, mContentViewCore, mWebContentsDelegate,
                new TabContextMenuPopulator(createContextMenuPopulator()));

        // In the case where restoring a Tab or showing a prerendered one we already have a
        // valid infobar container, no need to recreate one.
        if (mInfoBarContainer == null) {
            // The InfoBarContainer needs to be created after the ContentView has been natively
            // initialized.
            WebContents webContents = mContentViewCore.getWebContents();
            mInfoBarContainer = new InfoBarContainer(
                    (Activity) mContext, createAutoLoginProcessor(), getId(),
                    mContentViewCore.getContainerView(), webContents);
        } else {
            mInfoBarContainer.onParentViewChanged(getId(), mContentViewCore.getContainerView());
        }

        if (AppBannerManager.isEnabled() && mAppBannerManager == null) {
            mAppBannerManager = new AppBannerManager(this);
        }

        if (DomDistillerFeedbackReporter.isEnabled() && mDomDistillerFeedbackReporter == null) {
            mDomDistillerFeedbackReporter = new DomDistillerFeedbackReporter(this);
        }

        for (TabObserver observer : mObservers) observer.onContentChanged(this);

        // For browser tabs, we want to set accessibility focus to the page
        // when it loads. This is not the default behavior for embedded
        // web views.
        mContentViewCore.setShouldSetAccessibilityFocusOnPageLoad(true);
    }

    /**
     * Cleans up all internal state, destroying any {@link NativePage} or {@link ContentViewCore}
     * currently associated with this {@link Tab}.  This also destroys the native counterpart
     * to this class, which means that all subclasses should erase their native pointers after
     * this method is called.  Once this call is made this {@link Tab} should no longer be used.
     */
    public void destroy() {
        for (TabObserver observer : mObservers) observer.onDestroyed(this);
        mObservers.clear();

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
    }

    /**
     * @return Whether or not this Tab has a live native component.
     */
    public boolean isInitialized() {
        return mNativeTabAndroid != 0;
    }

    /**
     * @return The url associated with the tab.
     */
    @CalledByNative
    public String getUrl() {
        return getWebContents() != null ? getWebContents().getUrl() : "";
    }

    /**
     * @return The tab title.
     */
    @CalledByNative
    public String getTitle() {
        if (mNativePage != null) return mNativePage.getTitle();
        if (getWebContents() != null) return getWebContents().getTitle();
        return "";
    }

    /**
     * @return The bitmap of the favicon scaled to 16x16dp. null if no favicon
     *         is specified or it requires the default favicon.
     */
    public Bitmap getFavicon() {
        String url = getUrl();
        // Invalidate our cached values if necessary.
        if (url == null || !url.equals(mFaviconUrl)) {
            mFavicon = null;
            mFaviconUrl = null;
        }

        if (mFavicon == null) {
            // If we have no content return null.
            if (getNativePage() == null && getContentViewCore() == null) return null;

            Bitmap favicon = nativeGetFavicon(mNativeTabAndroid);

            // If the favicon is not yet valid (i.e. it's either blank or a placeholder), then do
            // not cache the results.  We still return this though so we have something to show.
            if (favicon != null && nativeIsFaviconValid(mNativeTabAndroid)) {
                mFavicon = favicon;
                mFaviconUrl = url;
            }

            return favicon;
        }

        return mFavicon;
    }

    /**
     * Loads the tab if it's not loaded (e.g. because it was killed in background).
     * @return true iff the Tab handled the request.
     */
    @CalledByNative
    public boolean loadIfNeeded() {
        return false;
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
     * Destroys the current {@link ContentViewCore}.
     * @param deleteNativeWebContents Whether or not to delete the native WebContents pointer.
     */
    protected final void destroyContentViewCore(boolean deleteNativeWebContents) {
        if (mContentViewCore == null) return;

        destroyContentViewCoreInternal(mContentViewCore);

        if (mInfoBarContainer != null && mInfoBarContainer.getParent() != null) {
            mInfoBarContainer.removeFromParentView();
        }
        mContentViewCore.destroy();

        mContentViewCore = null;
        mWebContentsDelegate = null;
        mWebContentsObserver = null;
        mVoiceSearchTabHelper = null;

        assert mNativeTabAndroid != 0;
        nativeDestroyWebContents(mNativeTabAndroid, deleteNativeWebContents);
    }

    /**
     * Gives subclasses the chance to clean up some state associated with this
     * {@link ContentViewCore}. This is because {@link #getContentViewCore()}
     * can return {@code null} if a {@link NativePage} is showing.
     *
     * @param cvc The {@link ContentViewCore} that should have associated state
     *            cleaned up.
     */
    protected void destroyContentViewCoreInternal(ContentViewCore cvc) {
    }

    /**
     * A helper method to allow subclasses to build their own delegate.
     * @return An instance of a {@link TabChromeWebContentsDelegateAndroid}.
     */
    protected TabChromeWebContentsDelegateAndroid createWebContentsDelegate() {
        return new TabChromeWebContentsDelegateAndroid();
    }

    /**
     * A helper method to allow subclasses to handle the Instant support
     * disabled event.
     */
    @CalledByNative
    private void onWebContentsInstantSupportDisabled() {
      for (TabObserver observer : mObservers) observer.onWebContentsInstantSupportDisabled();
    }

    /**
     * A helper method to allow subclasses to build their own menu populator.
     * @return An instance of a {@link ContextMenuPopulator}.
     */
    protected ContextMenuPopulator createContextMenuPopulator() {
        return new ChromeContextMenuPopulator(new TabChromeContextMenuItemDelegate());
    }

    /**
     * @return The {@link WindowAndroid} associated with this {@link Tab}.
     */
    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * @return The current {@link TabChromeWebContentsDelegateAndroid} instance.
     */
    protected TabChromeWebContentsDelegateAndroid getChromeWebContentsDelegateAndroid() {
        return mWebContentsDelegate;
    }

    /**
     * Called when the favicon of the content this tab represents changes.
     */
    @CalledByNative
    protected void onFaviconUpdated() {
        mFavicon = null;
        mFaviconUrl = null;
        for (TabObserver observer : mObservers) observer.onFaviconUpdated(this);
    }

    /**
     * Called when the navigation entry containing the historyitem changed,
     * for example because of a scroll offset or form field change.
     */
    @CalledByNative
    protected void onNavEntryChanged() {
    }

    /**
     * @return The native pointer representing the native side of this {@link Tab} object.
     */
    @CalledByNative
    protected long getNativePtr() {
        return mNativeTabAndroid;
    }

    /** This is currently called when committing a pre-rendered page. */
    @CalledByNative
    private void swapWebContents(
            long newWebContents, boolean didStartLoad, boolean didFinishLoad) {
        ContentViewCore cvc = new ContentViewCore(mContext);
        ContentView cv = ContentView.newInstance(mContext, cvc);
        cvc.initialize(cv, cv, newWebContents, getWindowAndroid());
        swapContentViewCore(cvc, false, didStartLoad, didFinishLoad);
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
    protected void swapContentViewCore(ContentViewCore newContentViewCore,
            boolean deleteOldNativeWebContents, boolean didStartLoad, boolean didFinishLoad) {
        int originalWidth = 0;
        int originalHeight = 0;
        if (mContentViewCore != null) {
            originalWidth = mContentViewCore.getViewportWidthPix();
            originalHeight = mContentViewCore.getViewportHeightPix();
            mContentViewCore.onHide();
        }
        destroyContentViewCore(deleteOldNativeWebContents);
        NativePage previousNativePage = mNativePage;
        mNativePage = null;
        setContentViewCore(newContentViewCore);
        // Size of the new ContentViewCore is zero at this point. If we don't call onSizeChanged(),
        // next onShow() call would send a resize message with the current ContentViewCore size
        // (zero) to the renderer process, although the new size will be set soon.
        // However, this size fluttering may confuse Blink and rendered result can be broken
        // (see http://crbug.com/340987).
        mContentViewCore.onSizeChanged(originalWidth, originalHeight, 0, 0);
        mContentViewCore.onShow();
        mContentViewCore.attachImeAdapter();
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

    @CalledByNative
    private long getNativeInfoBarContainer() {
        return getInfoBarContainer().getNative();
    }

    /**
     * Validates {@code id} and increments the internal counter to make sure future ids don't
     * collide.
     * @param id The current id.  Maybe {@link #INVALID_TAB_ID}.
     * @return   A new id if {@code id} was {@link #INVALID_TAB_ID}, or {@code id}.
     */
    public static int generateValidId(int id) {
        if (id == INVALID_TAB_ID) id = generateNextId();
        incrementIdCounterTo(id + 1);

        return id;
    }

    /**
     * @return An unused id.
     */
    private static int generateNextId() {
        return sIdCounter.getAndIncrement();
    }

    private void pushNativePageStateToNavigationEntry() {
        assert mNativeTabAndroid != 0 && getNativePage() != null;
        nativeSetActiveNavigationEntryTitleForUrl(mNativeTabAndroid, getNativePage().getUrl(),
                getNativePage().getTitle());
    }

    /**
     * Ensures the counter is at least as high as the specified value.  The counter should always
     * point to an unused ID (which will be handed out next time a request comes in).  Exposed so
     * that anything externally loading tabs and ids can set enforce new tabs start at the correct
     * id.
     * TODO(aurimas): Investigate reducing the visiblity of this method.
     * @param id The minimum id we should hand out to the next new tab.
     */
    public static void incrementIdCounterTo(int id) {
        int diff = id - sIdCounter.get();
        if (diff <= 0) return;
        // It's possible idCounter has been incremented between the get above and the add below
        // but that's OK, because in the worst case we'll overly increment idCounter.
        sIdCounter.addAndGet(diff);
    }

    private native void nativeInit();
    private native void nativeDestroy(long nativeTabAndroid);
    private native void nativeInitWebContents(long nativeTabAndroid, boolean incognito,
            ContentViewCore contentViewCore, ChromeWebContentsDelegateAndroid delegate,
            ContextMenuPopulator contextMenuPopulator);
    private native void nativeDestroyWebContents(long nativeTabAndroid, boolean deleteNative);
    private native Profile nativeGetProfileAndroid(long nativeTabAndroid);
    private native int nativeLoadUrl(long nativeTabAndroid, String url, String extraHeaders,
            byte[] postData, int transition, String referrerUrl, int referrerPolicy,
            boolean isRendererInitiated);
    private native int nativeGetSecurityLevel(long nativeTabAndroid);
    private native void nativeSetActiveNavigationEntryTitleForUrl(long nativeTabAndroid, String url,
            String title);
    private native boolean nativePrint(long nativeTabAndroid);
    private native Bitmap nativeGetFavicon(long nativeTabAndroid);
    private native boolean nativeIsFaviconValid(long nativeTabAndroid);
}
