// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.CUSTOM_TABS_UI_TYPE_MINIMAL_UI_WEBAPP;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnSystemUiVisibilityChangeListener;
import android.view.ViewGroup;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SingleTabActivity;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.customtabs.CustomTabAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabLayoutManager;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ScreenOrientationProvider;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.PageTransition;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/**
 * Displays a webapp in a nearly UI-less Chrome (InfoBars still appear).
 */
public class WebappActivity extends SingleTabActivity {
    public static final String WEBAPP_SCHEME = "webapp";

    private static final String TAG = "WebappActivity";
    private static final String HISTOGRAM_NAVIGATION_STATUS = "Webapp.NavigationStatus";
    private static final long MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL = 1000;

    private static final int ENTER_IMMERSIVE_MODE_DELAY_MILLIS = 300;
    private static final int RESTORE_IMMERSIVE_MODE_DELAY_MILLIS = 3000;
    static final int IMMERSIVE_MODE_UI_FLAGS = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
            | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
            | View.SYSTEM_UI_FLAG_LOW_PROFILE
            | View.SYSTEM_UI_FLAG_IMMERSIVE;

    private final WebappActionsNotificationManager mNotificationManager;
    private final WebappDirectoryManager mDirectoryManager;

    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;

    protected WebappInfo mWebappInfo;

    private WebappSplashScreenController mSplashController;

    private boolean mIsInitialized;
    private Integer mBrandColor;

    private Bitmap mLargestFavicon;

    private Runnable mSetImmersiveRunnable;

    /** Initialization-on-demand holder. This exists for thread-safe lazy initialization. */
    private static class Holder {
        // This static map is used to cache WebappInfo objects between their initial creation in
        // WebappLauncherActivity and final use in WebappActivity.
        private static final HashMap<String, WebappInfo> sWebappInfoMap =
                new HashMap<String, WebappInfo>();
    }

    /**
     * Construct all the variables that shouldn't change.  We do it here both to clarify when the
     * objects are created and to ensure that they exist throughout the parallelized initialization
     * of the WebappActivity.
     */
    public WebappActivity() {
        mWebappInfo = createWebappInfo(null);
        mDirectoryManager = new WebappDirectoryManager();
        mSplashController = createWebappSplashScreenController();
        mNotificationManager = new WebappActionsNotificationManager(this);
    }

    protected WebappSplashScreenController createWebappSplashScreenController() {
        return new WebappSplashScreenController();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (intent == null) return;

        if (mNotificationManager.handleNotificationAction(intent)) return;

        super.onNewIntent(intent);

        WebappInfo newWebappInfo = popWebappInfo(WebappInfo.idFromIntent(intent));
        if (newWebappInfo == null) newWebappInfo = createWebappInfo(intent);

        if (newWebappInfo == null) {
            Log.e(TAG, "Failed to parse new Intent: " + intent);
            ApiCompatibilityUtils.finishAndRemoveTask(this);
        } else if (newWebappInfo.shouldForceNavigation() && mIsInitialized) {
            LoadUrlParams params =
                    new LoadUrlParams(newWebappInfo.uri().toString(), PageTransition.AUTO_TOPLEVEL);
            params.setShouldClearHistoryList(true);
            getActivityTab().loadUrl(params);
        }
    }

    protected boolean isInitialized() {
        return mIsInitialized;
    }

    protected WebappInfo createWebappInfo(Intent intent) {
        return (intent == null) ? WebappInfo.createEmpty() : WebappInfo.create(intent);
    }

    protected void initializeUI(Bundle savedInstanceState) {
        // We do not load URL when restoring from saved instance states.
        if (savedInstanceState == null) {
            getActivityTab().loadUrl(
                    new LoadUrlParams(mWebappInfo.uri().toString(), PageTransition.AUTO_TOPLEVEL));
        } else {
            if (NetworkChangeNotifier.isOnline()) getActivityTab().reloadIgnoringCache();
        }

        getActivityTab().addObserver(createTabObserver());
        getActivityTab().getTabWebContentsDelegateAndroid().setDisplayMode(
                mWebappInfo.displayMode());
    }

    @Override
    public void preInflationStartup() {
        Intent intent = getIntent();
        String id = WebappInfo.idFromIntent(intent);
        WebappInfo info = popWebappInfo(id);
        // When WebappActivity is killed by the Android OS, and an entry stays in "Android Recents"
        // (The user does not swipe it away), when WebappActivity is relaunched it is relaunched
        // with the intent stored in WebappActivity#getIntent() at the time that the WebappActivity
        // was killed. WebappActivity may be relaunched from:

        // (A) An intent from WebappLauncherActivity (e.g. as a result of a notification or a deep
        // link). Android drops the intent from WebappLauncherActivity in favor of
        // WebappActivity#getIntent() at the time that the WebappActivity was killed.

        // (B) The user selecting the WebappActivity in recents. In case (A) we want to use the
        // intent sent to WebappLauncherActivity and ignore WebappActivity#getSavedInstanceState().
        // In case (B) we want to restore to saved tab state.
        if (info == null) {
            info = createWebappInfo(intent);
        } else if (info.shouldForceNavigation()) {
            // Don't restore to previous page, navigate using WebappInfo retrieved from cache.
            resetSavedInstanceState();
        }

        if (info == null) {
            // If {@link info} is null, there isn't much we can do, abort.
            ApiCompatibilityUtils.finishAndRemoveTask(this);
            return;
        }

        mWebappInfo = info;

        // Initialize the WebappRegistry and warm up the shared preferences for this web app. No-ops
        // if the registry and this web app are already initialized. Must override Strict Mode to
        // avoid a violation.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            WebappRegistry.getInstance();
            WebappRegistry.warmUpSharedPrefsForId(id);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        ScreenOrientationProvider.lockOrientation(
                getWindowAndroid(), (byte) mWebappInfo.orientation());
        super.preInflationStartup();
    }

    @Override
    public void finishNativeInitialization() {
        if (!mWebappInfo.isInitialized()) {
            ApiCompatibilityUtils.finishAndRemoveTask(this);
            return;
        }

        initializeUI(getSavedInstanceState());
        LayoutManager layoutDriver = new CustomTabLayoutManager(getCompositorViewHolder());
        initializeCompositorContent(layoutDriver, findViewById(R.id.url_bar),
                (ViewGroup) findViewById(android.R.id.content),
                (ToolbarControlContainer) findViewById(R.id.control_container));
        getToolbarManager().initializeWithNative(getTabModelSelector(),
                getFullscreenManager().getBrowserVisibilityDelegate(), getFindToolbarManager(),
                null, layoutDriver, null, null, null, null);
        getToolbarManager().setShowTitle(true);
        getToolbarManager().setCloseButtonDrawable(null); // Hides close button.

        if (getFullscreenManager() != null) getFullscreenManager().setTab(getActivityTab());
        mSplashController.onFinishedNativeInit(getActivityTab(), getCompositorViewHolder());
        super.finishNativeInitialization();
        mIsInitialized = true;
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        if (getActivityTab() != null) {
            outState.putInt(BUNDLE_TAB_ID, getActivityTab().getId());
            outState.putString(BUNDLE_TAB_URL, getActivityTab().getUrl());
        }
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        mDirectoryManager.cleanUpDirectories(this, getActivityId());
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();
        mDirectoryManager.cancelCleanup();
        if (getActivityTab() != null) saveState(getActivityDirectory());
        if (getFullscreenManager() != null) {
            getFullscreenManager().setPersistentFullscreenMode(false);
        }
    }

    /**
     * Saves the tab data out to a file.
     */
    void saveState(File activityDirectory) {
        String tabFileName = TabState.getTabStateFilename(getActivityTab().getId(), false);
        File tabFile = new File(activityDirectory, tabFileName);

        // Temporarily allowing disk access while fixing. TODO: http://crbug.com/525781
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            long time = SystemClock.elapsedRealtime();
            TabState.saveState(tabFile, getActivityTab().getState(), false);
            RecordHistogram.recordTimesHistogram("Android.StrictMode.WebappSaveState",
                    SystemClock.elapsedRealtime() - time, TimeUnit.MILLISECONDS);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        // Re-enter immersive mode after users switch back to this Activity.
        if (hasFocus) {
            asyncSetImmersive(ENTER_IMMERSIVE_MODE_DELAY_MILLIS);
        }
    }

    /**
     * Sets activity's decor view into an immersive mode.
     * If immersive mode is not supported, this method no-ops.
     */
    private void enterImmersiveMode() {
        // Immersive mode is only supported in API 19+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;

        if (mSetImmersiveRunnable == null) {
            final View decor = getWindow().getDecorView();

            mSetImmersiveRunnable = new Runnable() {
                @Override
                public void run() {
                    int currentFlags = decor.getSystemUiVisibility();
                    int desiredFlags = currentFlags | IMMERSIVE_MODE_UI_FLAGS;
                    if (currentFlags != desiredFlags) {
                        decor.setSystemUiVisibility(desiredFlags);
                    }
                }
            };

            // When we enter immersive mode for the first time, register a
            // SystemUiVisibilityChangeListener that restores immersive mode. This is necessary
            // because user actions like focusing a keyboard will break out of immersive mode.
            decor.setOnSystemUiVisibilityChangeListener(new OnSystemUiVisibilityChangeListener() {
                @Override
                public void onSystemUiVisibilityChange(int newFlags) {
                    if ((newFlags & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                        asyncSetImmersive(RESTORE_IMMERSIVE_MODE_DELAY_MILLIS);
                    }
                }
            });
        }

        asyncSetImmersive(0);
    }

    /**
     * This method no-ops before {@link #enterImmersiveMode()} is called explicitly.
     */
    private void asyncSetImmersive(int delayInMills) {
        if (mSetImmersiveRunnable == null) return;

        mHandler.removeCallbacks(mSetImmersiveRunnable);
        mHandler.postDelayed(mSetImmersiveRunnable, delayInMills);
    }

    @Override
    public void onResume() {
        if (!isFinishing()) {
            if (getIntent() != null) {
                // Avoid situations where Android starts two Activities with the same data.
                DocumentUtils.finishOtherTasksWithData(getIntent().getData(), getTaskId());
            }
            updateTaskDescription();
        }
        super.onResume();
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        mNotificationManager.maybeShowNotification();
    }

    @Override
    public void onPauseWithNative() {
        mNotificationManager.cancelNotification();
        super.onPauseWithNative();
    }

    @Override
    protected void initDeferredStartupForActivity() {
        super.initDeferredStartupForActivity();

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(mWebappInfo.id());
        if (storage != null) {
            onDeferredStartupWithStorage(storage);
        } else {
            onDeferredStartupWithNullStorage();
        }
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime.WebApp", timeMs, TimeUnit.MILLISECONDS);
    }

    protected void onDeferredStartupWithStorage(WebappDataStorage storage) {
        updateStorage(storage);
    }

    protected void onDeferredStartupWithNullStorage() {
        return;
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getAppMenuLayoutId() {
        return R.menu.custom_tabs_menu;
    }

    @Override
    protected int getToolbarLayoutId() {
        return R.layout.custom_tabs_toolbar;
    }

    @Override
    protected AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new CustomTabAppMenuPropertiesDelegate(this, CUSTOM_TABS_UI_TYPE_MINIMAL_UI_WEBAPP,
                new ArrayList<String>(), true /* is opened by Chrome */,
                true /* should show share */, false /* should show star (bookmarking) */,
                false /* should show download */);
    }

    @Override
    public void postInflationStartup() {
        initializeWebappData();

        super.postInflationStartup();
    }

    /**
     * @return Structure containing data about the webapp currently displayed.
     *         The return value should not be cached.
     */
    WebappInfo getWebappInfo() {
        return mWebappInfo;
    }

    /**
     * @return A string containing the scope of the webapp opened in this activity.
     */
    public String getWebappScope() {
        return mWebappInfo.scopeUri().toString();
    }

    public static void addWebappInfo(String id, WebappInfo info) {
        Holder.sWebappInfoMap.put(id, info);
    }

    public static WebappInfo popWebappInfo(String id) {
        return Holder.sWebappInfoMap.remove(id);
    }

    private void initializeWebappData() {
        if (mWebappInfo.displayMode() == WebDisplayMode.FULLSCREEN) {
            enterImmersiveMode();
        }

        ViewGroup contentView = (ViewGroup) findViewById(android.R.id.content);
        mSplashController.showSplashScreen(contentView, mWebappInfo);
    }

    protected void updateStorage(WebappDataStorage storage) {
        // The information in the WebappDataStorage may have been purged by the
        // user clearing their history or not launching the web app recently.
        // Restore the data if necessary from the intent.
        storage.updateFromShortcutIntent(getIntent());

        // A recent last used time is the indicator that the web app is still
        // present on the home screen, and enables sources such as notifications to
        // launch web apps. Thus, we do not update the last used time when the web
        // app is not directly launched from the home screen, as this interferes
        // with the heuristic.
        if (mWebappInfo.isLaunchedFromHomescreen()) {
            boolean previouslyLaunched = storage.hasBeenLaunched();
            long previousUsageTimestamp = storage.getLastUsedTime();
            storage.setHasBeenLaunched();
            storage.updateLastUsedTime();
            onUpdatedLastUsedTime(storage, previouslyLaunched, previousUsageTimestamp);
        }
    }

    /**
     * Called after updating the last used time in {@link WebappDataStorage}.
     * @param previouslyLaunched Whether the webapp has been previously launched from the home
     *     screen.
     * @param previousUsageTimestamp The previous time that the webapp was used.
     */
    protected void onUpdatedLastUsedTime(
            WebappDataStorage storage, boolean previouslyLaunched, long previousUsageTimestamp) {}

    @Override
    protected ChromeFullscreenManager createFullscreenManager() {
        // Disable HTML5 fullscreen in PWA fullscreen mode.
        return new ChromeFullscreenManager(this, false) {
            @Override
            public void setPersistentFullscreenMode(boolean enabled) {
                if (mWebappInfo.displayMode() == WebDisplayMode.FULLSCREEN) return;
                super.setPersistentFullscreenMode(enabled);
            }

            @Override
            public boolean getPersistentFullscreenMode() {
                if (mWebappInfo.displayMode() == WebDisplayMode.FULLSCREEN) return false;
                return super.getPersistentFullscreenMode();
            }
        };
    }

    @Override
    protected Tab createTab() {
        Tab tab = super.createTab();
        handleTabContentChanged(tab);
        return tab;
    }

    protected TabObserver createTabObserver() {
        return new EmptyTabObserver() {

            @Override
            public void onDidFinishNavigation(Tab tab, String url, boolean isInMainFrame,
                    boolean isErrorPage, boolean hasCommitted, boolean isSameDocument,
                    boolean isFragmentNavigation, Integer pageTransition, int errorCode,
                    int httpStatusCode) {
                if (hasCommitted && isInMainFrame) {
                    // Updates the URL.
                    mNotificationManager.maybeShowNotification();
                    RecordHistogram.recordBooleanHistogram(
                            HISTOGRAM_NAVIGATION_STATUS, !isErrorPage);
                }
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                mBrandColor = color;
                updateTaskDescription();
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                updateTaskDescription();
            }

            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon) {
                // No need to cache the favicon if there is an icon declared in app manifest.
                if (mWebappInfo.icon() != null) return;
                if (icon == null) return;
                if (mLargestFavicon == null || icon.getWidth() > mLargestFavicon.getWidth()
                        || icon.getHeight() > mLargestFavicon.getHeight()) {
                    mLargestFavicon = icon;
                    updateTaskDescription();
                }
            }

            @Override
            public void onDidAttachInterstitialPage(Tab tab) {
                int state = ApplicationStatus.getStateForActivity(WebappActivity.this);
                if (state == ActivityState.PAUSED || state == ActivityState.STOPPED
                        || state == ActivityState.DESTROYED) {
                    return;
                }

                // Kick the interstitial navigation to Chrome.
                Intent intent =
                        new Intent(Intent.ACTION_VIEW, Uri.parse(getActivityTab().getUrl()));
                intent.setPackage(getPackageName());
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                IntentHandler.startChromeLauncherActivityForTrustedIntent(intent);

                // Pretend like the navigation never happened.  We delay so that this happens while
                // the Activity is in the background.
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        if (getActivityTab().canGoBack()) {
                            getActivityTab().goBack();
                        } else {
                            ApiCompatibilityUtils.finishAndRemoveTask(WebappActivity.this);
                        }
                    }
                }, MS_BEFORE_NAVIGATING_BACK_FROM_INTERSTITIAL);
            }

            // TODO(piotrs): Remove this and clean up handleTabContentChanged() once pre-rendering
            //               is disabled and WebContents swapping can no longer happen
            //               (crbug.com/678332).
            @Override
            public void onContentChanged(Tab tab) {
                assert tab == getActivityTab();
                handleTabContentChanged(tab);
            }
        };
    }

    private void handleTabContentChanged(final Tab tab) {
        assert tab != null;

        WebContents webContents = tab.getWebContents();
        if (mWebContents == webContents) return;

        // Clean up any old references to the previous WebContents.
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }

        mWebContents = webContents;
        if (mWebContents == null) return;

        ContentViewCore.fromWebContents(webContents).setFullscreenRequiredForOrientationLock(false);
        mWebContentsObserver = new WebContentsObserver(webContents) {
            @Override
            public void didFinishNavigation(String url, boolean isInMainFrame, boolean isErrorPage,
                    boolean hasCommitted, boolean isSameDocument, boolean isFragmentNavigation,
                    Integer pageTransition, int errorCode, String errorDescription,
                    int httpStatusCode) {
                if (hasCommitted && isInMainFrame) {
                    // Notify the renderer to permanently hide the top controls since they do
                    // not apply to fullscreen content views.
                    tab.updateBrowserControlsState(tab.getBrowserControlsStateConstraints(), true);
                }
            }
        };
    }

    private void updateTaskDescription() {
        String title = null;
        if (!TextUtils.isEmpty(mWebappInfo.shortName())) {
            title = mWebappInfo.shortName();
        } else if (getActivityTab() != null) {
            title = getActivityTab().getTitle();
        }

        Bitmap icon = null;
        if (mWebappInfo.icon() != null) {
            icon = mWebappInfo.icon();
        } else if (getActivityTab() != null) {
            icon = mLargestFavicon;
        }

        if (mBrandColor == null && mWebappInfo.hasValidThemeColor()) {
            mBrandColor = (int) mWebappInfo.themeColor();
        }

        int taskDescriptionColor =
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_primary_color);

        // Don't use the brand color for the status bars if we're in display: fullscreen. This works
        // around an issue where the status bars go transparent and can't be seen on top of the page
        // content when users swipe them in or they appear because the on-screen keyboard was
        // triggered.
        int statusBarColor = Color.BLACK;
        if (mBrandColor != null && mWebappInfo.displayMode() != WebDisplayMode.FULLSCREEN) {
            taskDescriptionColor = mBrandColor;
            statusBarColor = ColorUtils.getDarkenedColorForStatusBar(mBrandColor);
            getToolbarManager().updatePrimaryColor(mBrandColor, false);
        }

        ApiCompatibilityUtils.setTaskDescription(this, title, icon,
                ColorUtils.getOpaqueColor(taskDescriptionColor));
        ApiCompatibilityUtils.setStatusBarColor(getWindow(), statusBarColor);
    }

    @Override
    protected void setStatusBarColor(Tab tab, int color) {
        // Intentionally do nothing as WebappActivity explicitly sets status bar color.
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.open_in_browser_id) {
            openCurrentUrlInChrome();
            if (fromMenu) {
                RecordUserAction.record("WebappMenuOpenInChrome");
            } else {
                RecordUserAction.record("Webapp.NotificationOpenInChrome");
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    /**
     * Opens the URL currently being displayed in the browser by reparenting the tab.
     */
    private boolean openCurrentUrlInChrome() {
        Tab tab = getActivityTab();
        if (tab == null) return false;

        String url = tab.getOriginalUrl();
        if (TextUtils.isEmpty(url)) {
            url = IntentHandler.getUrlFromIntent(getIntent());
        }

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Bundle startActivityOptions =
                ActivityOptionsCompat
                        .makeCustomAnimation(this, R.anim.abc_fade_in, R.anim.abc_fade_out)
                        .toBundle();

        tab.detachAndStartReparenting(intent, startActivityOptions, new Runnable() {
            @Override
            public void run() {
                ApiCompatibilityUtils.finishAndRemoveTask(WebappActivity.this);
            }
        });

        return true;
    }

    /**
     * Returns a unique identifier for this WebappActivity.
     * Note: do not call this function when you need {@link WebappInfo#id()}. Subclasses like
     * WebappManagedActivity and WebApkManagedActivity overwrite this function and return the
     * index of the activity.
     */
    protected String getActivityId() {
        return mWebappInfo.id();
    }

    /**
     * Get the active directory by this web app.
     *
     * @return The directory used for the current web app.
     */
    @Override
    protected final File getActivityDirectory() {
        return mDirectoryManager.getWebappDirectory(this, getActivityId());
    }

    @VisibleForTesting
    ViewGroup getSplashScreenForTests() {
        return mSplashController.getSplashScreenForTests();
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        return null;
    }

    @Override
    protected TabDelegateFactory createTabDelegateFactory() {
        return new WebappDelegateFactory(this);
    }

    @Override
    protected TabDelegate createTabDelegate(boolean incognito) {
        return new WebappTabDelegate(incognito);
    }

    // We're temporarily disable CS on webapp since there are some issues. (http://crbug.com/471950)
    // TODO(changwan): re-enable it once the issues are resolved.
    @Override
    protected boolean isContextualSearchAllowed() {
        return false;
    }
}
