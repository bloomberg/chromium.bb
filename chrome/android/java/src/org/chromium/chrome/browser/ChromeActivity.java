// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.SearchManager;
import android.app.assist.AssistContent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.annotation.CallSuper;
import android.support.v7.app.AlertDialog;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityManager.TouchExplorationStateChangeListener;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.appmenu.AppMenu;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager
        .ContextualSearchTabPromotionDelegate;
import org.chromium.chrome.browser.datausage.DataUseTabUIManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.dom_distiller.DistilledPagePrefsView;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.items
        .OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.firstrun.ForcedSigninProcessor;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.gsa.ContextReporter;
import org.chromium.chrome.browser.gsa.GSAAccountChangeListener;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.media.PictureInPictureController;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.StartupMetrics;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.nfc.BeamController;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.page_info.PageInfoPopup;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.physicalweb.PhysicalWebShareActivity;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.printing.PrintShareActivity;
import org.chromium.chrome.browser.printing.TabPrinter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.OptionalShareTargetsManager;
import org.chromium.chrome.browser.share.ShareActivity;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareParams;
import org.chromium.chrome.browser.snackbar.BottomContainer;
import org.chromium.chrome.browser.snackbar.DataReductionPromoSnackbarController;
import org.chromium.chrome.browser.snackbar.DataUseSnackbarController;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.toolbar.Toolbar;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.vr_shell.VrShellDelegate;
import org.chromium.chrome.browser.webapps.AddToHomescreenManager;
import org.chromium.chrome.browser.widget.ControlContainer;
import org.chromium.chrome.browser.widget.FadingBackgroundView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController;
import org.chromium.chrome.browser.widget.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content.browser.ContentVideoView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.ContentBitmapCallback;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.readback_types.ReadbackResponse;
import org.chromium.policy.CombinedPolicyProvider;
import org.chromium.policy.CombinedPolicyProvider.PolicyChangeListener;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;
import org.chromium.webapk.lib.client.WebApkNavigationClient;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nullable;

/**
 * A {@link AsyncInitializationActivity} that builds and manages a {@link CompositorViewHolder}
 * and associated classes.
 */
public abstract class ChromeActivity extends AsyncInitializationActivity
        implements TabCreatorManager, AccessibilityStateChangeListener, PolicyChangeListener,
        ContextualSearchTabPromotionDelegate, SnackbarManageable, SceneChangeObserver {
    /**
     * Factory which creates the AppMenuHandler.
     */
    public interface AppMenuHandlerFactory {
        /**
         * @return AppMenuHandler for the given activity and menu resource id.
         */
        public AppMenuHandler get(Activity activity, AppMenuPropertiesDelegate delegate,
                int menuResourceId);
    }

    /**
     * No control container to inflate during initialization.
     */
    static final int NO_CONTROL_CONTAINER = -1;

    /**
     * No toolbar layout to inflate during initialization.
     */
    static final int NO_TOOLBAR_LAYOUT = -1;

    private static final int RECORD_MULTI_WINDOW_SCREEN_WIDTH_DELAY_MS = 5000;

    /**
     * Timeout in ms for reading PartnerBrowserCustomizations provider.
     */
    private static final int PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS = 10000;
    private static final String TAG = "ChromeActivity";
    private static final Rect EMPTY_RECT = new Rect();

    private static AppMenuHandlerFactory sAppMenuHandlerFactory =
            (activity, delegate, menuResourceId) -> new AppMenuHandler(activity, delegate,
                    menuResourceId);

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private TabCreatorManager.TabCreator mRegularTabCreator;
    private TabCreatorManager.TabCreator mIncognitoTabCreator;
    private TabContentManager mTabContentManager;
    private UmaSessionStats mUmaSessionStats;
    private ContextReporter mContextReporter;

    private boolean mPartnerBrowserRefreshNeeded;

    protected IntentHandler mIntentHandler;

    /** Set if {@link #postDeferredStartupIfNeeded()} is called before native has loaded. */
    private boolean mDeferredStartupQueued;

    /** Whether or not {@link #postDeferredStartupIfNeeded()} has already successfully run. */
    private boolean mDeferredStartupPosted;

    private boolean mTabModelsInitialized;
    private boolean mNativeInitialized;
    private boolean mRemoveWindowBackgroundDone;

    // The class cannot implement TouchExplorationStateChangeListener,
    // because it is only available for Build.VERSION_CODES.KITKAT and later.
    // We have to instantiate the TouchExplorationStateChangeListner object in the code.
    @SuppressLint("NewApi")
    private TouchExplorationStateChangeListener mTouchExplorationStateChangeListener;

    // Observes when sync becomes ready to create the mContextReporter.
    private ProfileSyncService.SyncStateChangedListener mSyncStateChangedListener;

    private ChromeFullscreenManager mFullscreenManager;
    private boolean mCreatedFullscreenManager;

    // The PictureInPictureController is initialized lazily https://crbug.com/729738.
    private PictureInPictureController mPictureInPictureController;

    private CompositorViewHolder mCompositorViewHolder;
    private InsetObserverView mInsetObserverView;
    private ContextualSearchManager mContextualSearchManager;
    protected ReaderModeManager mReaderModeManager;
    private SnackbarManager mSnackbarManager;
    private DataUseSnackbarController mDataUseSnackbarController;
    private DataReductionPromoSnackbarController mDataReductionPromoSnackbarController;
    private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private AppMenuHandler mAppMenuHandler;
    private ToolbarManager mToolbarManager;
    private FindToolbarManager mFindToolbarManager;
    private BottomSheet mBottomSheet;
    private BottomSheetContentController mBottomSheetContentController;
    private FadingBackgroundView mFadingBackgroundView;

    // Time in ms that it took took us to inflate the initial layout
    private long mInflateInitialLayoutDurationMs;

    private int mUiMode;
    private int mScreenWidthDp;
    private Runnable mRecordMultiWindowModeScreenWidthRunnable;

    private final DiscardableReferencePool mReferencePool = new DiscardableReferencePool();

    private AssistStatusHandler mAssistStatusHandler;

    // A set of views obscuring all tabs. When this set is nonempty,
    // all tab content will be hidden from the accessibility tree.
    private Set<View> mViewsObscuringAllTabs = new HashSet<>();

    // See enableHardwareAcceleration()
    private boolean mSetWindowHWA;

    // Skips capturing screenshot for testing purpose.
    private boolean mScreenshotCaptureSkippedForTesting;

    /** Whether or not a PolicyChangeListener was added. */
    private boolean mDidAddPolicyChangeListener;

    /**
     * @param factory The {@link AppMenuHandlerFactory} for creating {@link mAppMenuHandler}
     */
    @VisibleForTesting
    public static void setAppMenuHandlerFactoryForTesting(AppMenuHandlerFactory factory) {
        sAppMenuHandlerFactory = factory;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ChromeWindow(this);
    }

    @Override
    public void preInflationStartup() {
        super.preInflationStartup();

        // Force a partner customizations refresh if it has yet to be initialized.  This can happen
        // if Chrome is killed and you refocus a previous activity from Android recents, which does
        // not go through ChromeLauncherActivity that would have normally triggered this.
        mPartnerBrowserRefreshNeeded = !PartnerBrowserCustomizations.isInitialized();

        ApplicationInitialization.enableFullscreenFlags(
                getResources(), this, getControlContainerHeightResource());
        getWindow().setBackgroundDrawable(getBackgroundDrawable());

        mFullscreenManager = createFullscreenManager();
        mCreatedFullscreenManager = true;
    }

    @SuppressLint("NewApi")
    @Override
    public void postInflationStartup() {
        super.postInflationStartup();

        Intent intent = getIntent();
        if (intent != null && getSavedInstanceState() == null) {
            VrShellDelegate.maybeHandleVrIntentPreNative(this, intent);
        }

        mSnackbarManager = new SnackbarManager(this, null);
        mDataUseSnackbarController = new DataUseSnackbarController(this, getSnackbarManager());

        mAssistStatusHandler = createAssistStatusHandler();
        if (mAssistStatusHandler != null) {
            if (mTabModelSelector != null) {
                mAssistStatusHandler.setTabModelSelector(mTabModelSelector);
            }
            mAssistStatusHandler.updateAssistState();
        }

        // If a user had ALLOW_LOW_END_DEVICE_UI explicitly set to false then we manually override
        // SysUtils.isLowEndDevice() with a switch so that they continue to see the normal UI. This
        // is only the case for grandfathered-in svelte users. We no longer do so for newer users.
        if (!ChromePreferenceManager.getInstance().getAllowLowEndDeviceUi()) {
            CommandLine.getInstance().appendSwitch(
                    BaseSwitches.DISABLE_LOW_END_DEVICE_MODE);
        }

        AccessibilityManager manager = (AccessibilityManager)
                getBaseContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        manager.addAccessibilityStateChangeListener(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            mTouchExplorationStateChangeListener = enabled -> checkAccessibility();
            manager.addTouchExplorationStateChangeListener(mTouchExplorationStateChangeListener);
        }

        // Make the activity listen to policy change events
        CombinedPolicyProvider.get().addPolicyChangeListener(this);
        mDidAddPolicyChangeListener = true;

        // Set up the animation placeholder to be the SurfaceView. This disables the
        // SurfaceView's 'hole' clipping during animations that are notified to the window.
        getWindowAndroid().setAnimationPlaceholderView(mCompositorViewHolder.getCompositorView());

        // Inform the WindowAndroid of the keyboard accessory view.
        getWindowAndroid().setKeyboardAccessoryView(
                (ViewGroup) findViewById(R.id.keyboard_accessory));
        initializeToolbar();
        initializeTabModels();
        if (!isFinishing() && getFullscreenManager() != null) {
            getFullscreenManager().initialize(
                    (ControlContainer) findViewById(R.id.control_container),
                    getTabModelSelector(),
                    getControlContainerHeightResource());
        }

        if (mBottomSheet != null) {
            mBottomSheet.setTabModelSelector(mTabModelSelector);
            mBottomSheet.setFullscreenManager(mFullscreenManager);

            mFadingBackgroundView = (FadingBackgroundView) findViewById(R.id.fading_focus_target);
            mBottomSheet.addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onTransitionPeekToHalf(float transitionFraction) {
                    mFadingBackgroundView.setViewAlpha(transitionFraction);
                }
            });
            mFadingBackgroundView.addObserver(mBottomSheet);

            mBottomSheetContentController =
                    (BottomSheetContentController) ((ViewStub) findViewById(R.id.bottom_nav_stub))
                            .inflate();
            int controlContainerHeight =
                    ((ControlContainer) findViewById(R.id.control_container)).getView().getHeight();
            mBottomSheetContentController.init(
                    mBottomSheet, controlContainerHeight, mTabModelSelector, this);
        }
        ((BottomContainer) findViewById(R.id.bottom_container)).initialize(mFullscreenManager);
    }

    @Override
    protected View getViewToBeDrawnBeforeInitializingNative() {
        View controlContainer = findViewById(R.id.control_container);
        return controlContainer != null ? controlContainer
                : super.getViewToBeDrawnBeforeInitializingNative();
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

        enableHardwareAcceleration();
        setLowEndTheme();
        int controlContainerLayoutId = getControlContainerLayoutId();
        WarmupManager warmupManager = WarmupManager.getInstance();
        if (warmupManager.hasViewHierarchyWithToolbar(controlContainerLayoutId)) {
            View placeHolderView = new View(this);
            setContentView(placeHolderView);
            ViewGroup contentParent = (ViewGroup) placeHolderView.getParent();
            warmupManager.transferViewHierarchyTo(contentParent);
            contentParent.removeView(placeHolderView);
        } else {
            warmupManager.clearViewHierarchy();

            // Allow disk access for the content view and toolbar container setup.
            // On certain android devices this setup sequence results in disk writes outside
            // of our control, so we have to disable StrictMode to work. See crbug.com/639352.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            try {
                setContentView(R.layout.main);
                if (controlContainerLayoutId != NO_CONTROL_CONTAINER) {
                    ViewStub toolbarContainerStub =
                            ((ViewStub) findViewById(R.id.control_container_stub));

                    toolbarContainerStub.setLayoutResource(controlContainerLayoutId);
                    View container = toolbarContainerStub.inflate();
                }

                // It cannot be assumed that the result of toolbarContainerStub.inflate() will be
                // the control container since it may be wrapped in another view.
                ControlContainer controlContainer =
                        (ControlContainer) findViewById(R.id.control_container);

                // Inflate the correct toolbar layout for the device.
                int toolbarLayoutId = getToolbarLayoutId();
                if (toolbarLayoutId != NO_TOOLBAR_LAYOUT && controlContainer != null) {
                    controlContainer.initWithToolbar(toolbarLayoutId);
                }

                // Get a handle to the bottom sheet if using the bottom control container.
                if (controlContainerLayoutId == R.layout.bottom_control_container) {
                    View coordinator = findViewById(R.id.coordinator);
                    mBottomSheet = (BottomSheet) findViewById(R.id.bottom_sheet);
                    mBottomSheet.init(coordinator, controlContainer.getView(), this);
                }
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
        TraceEvent.end("onCreate->setContentView()");
        mInflateInitialLayoutDurationMs = SystemClock.elapsedRealtime() - begin;

        // Set the status bar color to black by default. This is an optimization for
        // Chrome not to draw under status and navigation bars when we use the default
        // black status bar
        setStatusBarColor(null, Color.BLACK);

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();
        mCompositorViewHolder = (CompositorViewHolder) findViewById(R.id.compositor_view_holder);
        mCompositorViewHolder.setRootView(rootView);

        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the insets.
        rootView.setFitsSystemWindows(false);

        // Add a custom view right after the root view that stores the insets to access later.
        // ContentViewCore needs the insets to determine the portion of the screen obscured by
        // non-content displaying things such as the OSK.
        mInsetObserverView = InsetObserverView.create(this);
        rootView.addView(mInsetObserverView, 0);
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    /**
     * Constructs {@link ToolbarManager} and the handler necessary for controlling the menu on the
     * {@link Toolbar}. Extending classes can override this call to avoid creating the toolbar.
     */
    protected void initializeToolbar() {
        final View controlContainer = findViewById(R.id.control_container);
        assert controlContainer != null;
        ToolbarControlContainer toolbarContainer = (ToolbarControlContainer) controlContainer;
        mAppMenuPropertiesDelegate = createAppMenuPropertiesDelegate();
        mAppMenuHandler = sAppMenuHandlerFactory.get(this, mAppMenuPropertiesDelegate,
                getAppMenuLayoutId());
        Callback<Boolean> urlFocusChangedCallback = hasFocus -> onOmniboxFocusChanged(hasFocus);
        mToolbarManager = new ToolbarManager(this, toolbarContainer, mAppMenuHandler,
                mAppMenuPropertiesDelegate, getCompositorViewHolder().getInvalidator(),
                urlFocusChangedCallback);
        mFindToolbarManager = new FindToolbarManager(
                this, mToolbarManager.getActionModeController().getActionModeCallback());
        mAppMenuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (isVisible && !isInOverviewMode()) {
                    // The app menu badge should be removed the first time the menu is opened.
                    if (mToolbarManager.getToolbar().isShowingAppMenuUpdateBadge()) {
                        mToolbarManager.getToolbar().removeAppMenuUpdateBadge(true);
                        mCompositorViewHolder.requestRender();
                    }
                }
                if (!isVisible) {
                    mAppMenuPropertiesDelegate.onMenuDismissed();
                    MenuItem updateMenuItem = mAppMenuHandler.getAppMenu().getMenu().findItem(
                            R.id.update_menu_id);
                    if (updateMenuItem != null && updateMenuItem.isVisible()) {
                        UpdateMenuItemHelper.getInstance().onMenuDismissed();
                    }
                }
            }

            @Override
            public void onMenuHighlightChanged(boolean highlighting) {}
        });
    }

    /**
     * Initialize the {@link TabModelSelector}, {@link TabModel}s, and
     * {@link org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator} needed by
     * this activity.
     */
    protected final void initializeTabModels() {
        if (mTabModelsInitialized) return;

        mTabModelSelector = createTabModelSelector();
        if (mTabModelSelector == null) {
            assert isFinishing();
            mTabModelsInitialized = true;
            return;
        }

        Pair<? extends TabCreator, ? extends TabCreator> tabCreators = createTabCreators();
        mRegularTabCreator = tabCreators.first;
        mIncognitoTabCreator = tabCreators.second;

        OfflinePageUtils.observeTabModelSelector(this, mTabModelSelector);
        NewTabPageUma.monitorNTPCreation(mTabModelSelector);

        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                if (DataUseTabUIManager.checkAndResetDataUseTrackingStarted(tab)
                        && DataUseTabUIManager.shouldShowDataUseStartedUI()) {
                    mDataUseSnackbarController.showDataUseTrackingStartedBar();
                } else if (DataUseTabUIManager.shouldShowDataUseEndedUI()
                        && DataUseTabUIManager.shouldShowDataUseEndedSnackbar(
                                   getApplicationContext())
                        && DataUseTabUIManager.checkAndResetDataUseTrackingEnded(tab)) {
                    mDataUseSnackbarController.showDataUseTrackingEndedBar();
                }

                // Only alert about data savings once the first paint has happened. It doesn't make
                // sense to show a snackbar about savings when nothing has been displayed yet.
                if (DataReductionProxySettings.getInstance().isSnackbarPromoAllowed(tab.getUrl())) {
                    if (mDataReductionPromoSnackbarController == null) {
                        mDataReductionPromoSnackbarController =
                                new DataReductionPromoSnackbarController(
                                        getApplicationContext(), getSnackbarManager());
                    }
                    mDataReductionPromoSnackbarController.maybeShowDataReductionPromoSnackbar(
                            DataReductionProxySettings.getInstance()
                                    .getTotalHttpContentLengthSaved());
                }
            }

            @Override
            public void onShown(Tab tab) {
                setStatusBarColor(tab, tab.getThemeColor());
            }

            @Override
            public void onHidden(Tab tab) {
                mDataUseSnackbarController.dismissDataUseBar();
            }

            @Override
            public void onDestroyed(Tab tab) {
                mDataUseSnackbarController.dismissDataUseBar();
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                postDeferredStartupIfNeeded();
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                postDeferredStartupIfNeeded();
                OfflinePageUtils.showOfflineSnackbarIfNecessary(tab);
            }

            @Override
            public void onCrash(Tab tab, boolean sadTabShown) {
                postDeferredStartupIfNeeded();
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                if (getActivityTab() != tab) return;
                setStatusBarColor(tab, color);

                if (getToolbarManager() == null) return;
                getToolbarManager().updatePrimaryColor(color, true);

                ControlContainer controlContainer =
                        (ControlContainer) findViewById(R.id.control_container);
                controlContainer.getToolbarResourceAdapter().invalidate(null);
            }

            @Override
            public void onContentChanged(Tab tab) {
                if (getBottomSheet() != null) setStatusBarColor(tab, tab.getDefaultThemeColor());
            }
        };

        if (mAssistStatusHandler != null) {
            mAssistStatusHandler.setTabModelSelector(mTabModelSelector);
        }

        mTabModelsInitialized = true;
    }

    /**
     * @return The {@link TabModelSelector} owned by this {@link ChromeActivity}.
     */
    protected abstract TabModelSelector createTabModelSelector();

    /**
     * @return The {@link org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator}s owned
     *         by this {@link ChromeActivity}.  The first item in the Pair is the normal model tab
     *         creator, and the second is the tab creator for incognito tabs.
     */
    protected abstract Pair<? extends TabCreator, ? extends TabCreator> createTabCreators();

    /**
     * @return {@link ToolbarManager} that belongs to this activity.
     */
    public ToolbarManager getToolbarManager() {
        return mToolbarManager;
    }

    /**
     * @return {@link FindToolbarManager} that belongs to this activity.
     */
    public FindToolbarManager getFindToolbarManager() {
        return mFindToolbarManager;
    }

    /**
     * @return The resource id for the menu to use in {@link AppMenu}. Default is R.menu.main_menu.
     */
    protected int getAppMenuLayoutId() {
        return R.menu.main_menu;
    }

    /**
     * Get the Chrome Home bottom sheet if it exists.
     * @return The bottom sheet or null.
     */
    @Nullable
    public BottomSheet getBottomSheet() {
        return mBottomSheet;
    }

    /**
     * Get the Chrome Home bottom sheet content controller if it exists.
     * @return The {@link BottomSheetContentController} or null.
     */
    @Nullable
    public BottomSheetContentController getBottomSheetContentController() {
        return mBottomSheetContentController;
    }

    /**
     * @return The View used to obscure content and bring focus to a foreground view.
     */
    public FadingBackgroundView getFadingBackgroundView() {
        return mFadingBackgroundView;
    }

    /**
     * @return {@link AppMenuPropertiesDelegate} instance that the {@link AppMenuHandler}
     *         should be using in this activity.
     */
    protected AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new AppMenuPropertiesDelegate(this);
    }

    /**
     * @return The assist handler for this activity.
     */
    protected AssistStatusHandler getAssistStatusHandler() {
        return mAssistStatusHandler;
    }

    /**
     * @return A newly constructed assist handler for this given activity type.
     */
    protected AssistStatusHandler createAssistStatusHandler() {
        return new AssistStatusHandler(this);
    }

    /**
     * @return The resource id for the layout to use for {@link ControlContainer}. 0 by default.
     */
    protected int getControlContainerLayoutId() {
        return NO_CONTROL_CONTAINER;
    }

    /**
     * @return The layout ID for the toolbar to use.
     */
    protected int getToolbarLayoutId() {
        return NO_TOOLBAR_LAYOUT;
    }

    /**
     * @return Whether contextual search is allowed for this activity or not.
     */
    protected boolean isContextualSearchAllowed() {
        return true;
    }

    @Override
    public void initializeState() {
        super.initializeState();

        IntentHandler.setTestIntentsEnabled(
                CommandLine.getInstance().hasSwitch(ContentSwitches.ENABLE_TEST_INTENTS));
        mIntentHandler = new IntentHandler(createIntentHandlerDelegate(), getPackageName());
    }

    @Override
    public void initializeCompositor() {
        TraceEvent.begin("ChromeActivity:CompositorInitialization");
        super.initializeCompositor();

        setTabContentManager(new TabContentManager(this, getContentOffsetProvider(),
                DeviceClassManager.enableSnapshots()));
        mCompositorViewHolder.onNativeLibraryReady(getWindowAndroid(), getTabContentManager());

        if (isContextualSearchAllowed() && ContextualSearchFieldTrial.isEnabled()) {
            mContextualSearchManager = new ContextualSearchManager(this, this);
            if (mFindToolbarManager != null) {
                mContextualSearchManager.setFindToolbarManager(mFindToolbarManager);
            }
        }

        if (ReaderModeManager.isEnabled(this)) {
            mReaderModeManager = new ReaderModeManager(getTabModelSelector(), this);
        }

        TraceEvent.end("ChromeActivity:CompositorInitialization");
    }

    @Override
    public void onStartWithNative() {
        assert mNativeInitialized : "onStartWithNative was called before native was initialized.";

        super.onStartWithNative();
        UpdateMenuItemHelper.getInstance().onStart();
        ChromeActivitySessionTracker.getInstance().onStartWithNative();

        if (!SysUtils.isLowEndDevice()) {
            if (GSAState.getInstance(this).isGsaAvailable()) {
                // GSA connection is not needed on low-end devices because Icing is disabled.
                GSAAccountChangeListener.getInstance().connect();
                createContextReporterIfNeeded();
            } else {
                ContextReporter.reportStatus(ContextReporter.STATUS_GSA_NOT_AVAILABLE);
            }
        }

        // postDeferredStartupIfNeeded() is called in TabModelSelectorTabObsever#onLoadStopped(),
        // #onPageLoadFinished() and #onCrash(). If we are not actively loading a tab (e.g.
        // in Android N multi-instance, which is created by re-parenting an existing tab),
        // ensure onDeferredStartup() gets called by calling postDeferredStartupIfNeeded() here.
        if (mDeferredStartupQueued || getActivityTab() == null || !getActivityTab().isLoading()) {
            postDeferredStartupIfNeeded();
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        maybeRemoveWindowBackground();

        Tab tab = getActivityTab();
        if (tab == null) return;
        if (hasFocus) {
            tab.onActivityShown();
        } else {
            boolean stopped = ApplicationStatus.getStateForActivity(this) == ActivityState.STOPPED;
            if (stopped) tab.onActivityHidden();
        }
    }

    /**
     * Set device status bar to a given color.
     * @param tab The tab that is currently showing.
     * @param color The color that the status bar should be set to.
     */
    protected void setStatusBarColor(Tab tab, int color) {
        int statusBarColor = (tab != null && tab.isDefaultThemeColor())
                ? Color.BLACK : ColorUtils.getDarkenedColorForStatusBar(color);
        ApiCompatibilityUtils.setStatusBarColor(getWindow(), statusBarColor);
    }

    private void createContextReporterIfNeeded() {
        if (mContextReporter != null || getActivityTab() == null) return;

        final SyncController syncController = SyncController.get(this);
        final ProfileSyncService syncService = ProfileSyncService.get();

        if (syncController != null && syncController.isSyncingUrlsWithKeystorePassphrase()) {
            assert syncService != null;
            mContextReporter = AppHooks.get().createGsaHelper().getContextReporter(this);

            if (mSyncStateChangedListener != null) {
                syncService.removeSyncStateChangedListener(mSyncStateChangedListener);
                mSyncStateChangedListener = null;
            }

            return;
        } else {
            ContextReporter.reportSyncStatus(syncService);
        }

        if (mSyncStateChangedListener == null && syncService != null) {
            mSyncStateChangedListener = () -> createContextReporterIfNeeded();
            syncService.addSyncStateChangedListener(mSyncStateChangedListener);
        }
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        markSessionResume();
        RecordUserAction.record("MobileComeToForeground");

        if (getActivityTab() != null) {
            LaunchMetrics.commitLaunchMetrics(getActivityTab().getWebContents());
        }
        ContentViewCore cvc = getContentViewCore();
        if (cvc != null) cvc.onResume();
        FeatureUtilities.setCustomTabVisible(isCustomTab());
        FeatureUtilities.setIsInMultiWindowMode(
                MultiWindowUtils.getInstance().isInMultiWindowMode(this));

        if (mPictureInPictureController != null) {
            mPictureInPictureController.cleanup(this);
        }
        VrShellDelegate.maybeRegisterVrEntryHook(this);
    }

    @Override
    protected void onUserLeaveHint() {
        super.onUserLeaveHint();

        if (mPictureInPictureController == null) {
            mPictureInPictureController = new PictureInPictureController();
        }
        mPictureInPictureController.attemptPictureInPicture(this);
    }

    @Override
    public void onPauseWithNative() {
        RecordUserAction.record("MobileGoToBackground");
        Tab tab = getActivityTab();
        if (tab != null) getTabContentManager().cacheTabThumbnail(tab);
        ContentViewCore cvc = getContentViewCore();
        if (cvc != null) cvc.onPause();
        VrShellDelegate.maybeUnregisterVrEntryHook(this);
        markSessionEnd();
        super.onPauseWithNative();
    }

    @Override
    public void onStopWithNative() {
        Tab tab = getActivityTab();
        if (tab != null && !hasWindowFocus()) tab.onActivityHidden();
        if (mAppMenuHandler != null) mAppMenuHandler.hideAppMenu();

        if (GSAState.getInstance(this).isGsaAvailable() && !SysUtils.isLowEndDevice()) {
            GSAAccountChangeListener.getInstance().disconnect();
            if (mSyncStateChangedListener != null) {
                ProfileSyncService syncService = ProfileSyncService.get();
                if (syncService != null) {
                    syncService.removeSyncStateChangedListener(mSyncStateChangedListener);
                }
                mSyncStateChangedListener = null;
            }
        }
        super.onStopWithNative();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        VrShellDelegate.maybeHandleVrIntentPreNative(this, intent);
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        if (mPictureInPictureController != null) {
            mPictureInPictureController.cleanup(this);
        }

        super.onNewIntentWithNative(intent);
        if (mIntentHandler.shouldIgnoreIntent(intent)) return;

        // We send this intent so that we can enter WebVr presentation mode if needed. This
        // call doesn't consume the intent because it also has the url that we need to load.
        VrShellDelegate.onNewIntentWithNative(this, intent);
        mIntentHandler.onNewIntent(intent);
    }

    /**
     * @return Whether the given activity contains a CustomTab.
     */
    public boolean isCustomTab() {
        return false;
    }

    /**
     * Actions that may be run at some point after startup. Place tasks that are not critical to the
     * startup path here.  This method will be called automatically.
     */
    private void onDeferredStartup() {
        initDeferredStartupForActivity();
        ProcessInitializationHandler.getInstance().initializeDeferredStartupTasks();
        DeferredStartupHandler.getInstance().queueDeferredTasksOnIdleHandler();
    }

    /**
     * All deferred startup tasks that require the activity rather than the app should go here.
     *
     * Overriding methods should queue tasks on the DeferredStartupHandler before or after calling
     * super depending on whether the tasks should run before or after these ones.
     */
    @CallSuper
    protected void initDeferredStartupForActivity() {
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityDestroyed()) return;
            BeamController.registerForBeam(ChromeActivity.this, () -> {
                Tab currentTab = getActivityTab();
                if (currentTab == null) return null;
                if (!currentTab.isUserInteractable()) return null;
                return currentTab.getUrl();
            });

            UpdateMenuItemHelper.getInstance().checkForUpdateOnBackgroundThread(
                    ChromeActivity.this);
        });

        final String simpleName = getClass().getSimpleName();
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityDestroyed()) return;
            if (mToolbarManager != null) {
                RecordHistogram.recordTimesHistogram(
                        "MobileStartup.ToolbarInflationTime." + simpleName,
                        mInflateInitialLayoutDurationMs, TimeUnit.MILLISECONDS);
                mToolbarManager.onDeferredStartup(getOnCreateTimestampMs(), simpleName);
            }

            if (MultiWindowUtils.getInstance().isInMultiWindowMode(ChromeActivity.this)) {
                onDeferredStartupForMultiWindowMode();
            }

            long intentTimestamp = IntentHandler.getTimestampFromIntent(getIntent());
            if (intentTimestamp != -1) {
                recordIntentToCreationTime(getOnCreateTimestampMs() - intentTimestamp);
            }
        });

        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityDestroyed()) return;
            ForcedSigninProcessor.checkCanSignIn(ChromeActivity.this);
        });
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityDestroyed() || mBottomSheetContentController == null) return;
            mBottomSheetContentController.initializeDefaultContent();
        });
    }

    /**
     * Actions that may be run at some point after startup for Android N multi-window mode. Should
     * be called from #onDeferredStartup() if the activity is in multi-window mode.
     */
    protected void onDeferredStartupForMultiWindowMode() {
        // If the Activity was launched in multi-window mode, record a user action and the screen
        // width.
        recordMultiWindowModeChangedUserAction(true);
        recordMultiWindowModeScreenWidth();
    }

    /**
     * Records the time it takes from creating an intent for {@link ChromeActivity} to activity
     * creation, including time spent in the framework.
     * @param timeMs The time from creating an intent to activity creation.
     */
    @CallSuper
    protected void recordIntentToCreationTime(long timeMs) {
        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime", timeMs, TimeUnit.MILLISECONDS);
    }

    @Override
    public void onStart() {
        if (AsyncTabParamsManager.hasParamsWithTabToReparent()) {
            mCompositorViewHolder.prepareForTabReparenting();
        }
        super.onStart();
        if (mContextReporter != null) mContextReporter.enable();

        if (mPartnerBrowserRefreshNeeded) {
            mPartnerBrowserRefreshNeeded = false;
            PartnerBrowserCustomizations.initializeAsync(getApplicationContext(),
                    PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS);
            PartnerBrowserCustomizations.setOnInitializeAsyncFinished(() -> {
                if (PartnerBrowserCustomizations.isIncognitoDisabled()) {
                    terminateIncognitoSession();
                }
            });
        }
        if (mCompositorViewHolder != null) mCompositorViewHolder.onStart();
        mSnackbarManager.onStart();

        // Explicitly call checkAccessibility() so things are initialized correctly when Chrome has
        // been re-started after closing due to the last tab being closed when homepage is enabled.
        // See crbug.com/541546.
        checkAccessibility();

        mUiMode = getResources().getConfiguration().uiMode;
        mScreenWidthDp = getResources().getConfiguration().screenWidthDp;
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mContextReporter != null) mContextReporter.disable();

        // We want to refresh partner browser provider every onStart().
        mPartnerBrowserRefreshNeeded = true;
        if (mCompositorViewHolder != null) mCompositorViewHolder.onStop();
        mSnackbarManager.onStop();
    }

    @Override
    @TargetApi(Build.VERSION_CODES.M)
    public void onProvideAssistContent(AssistContent outContent) {
        if (getAssistStatusHandler() == null || !getAssistStatusHandler().isAssistSupported()) {
            // No information is provided in incognito mode.
            return;
        }
        Tab tab = getActivityTab();
        if (tab != null && !isInOverviewMode()) {
            outContent.setWebUri(Uri.parse(tab.getUrl()));
        }
    }

    @Override
    public long getOnCreateTimestampMs() {
        return super.getOnCreateTimestampMs();
    }

    /**
     * This cannot be overridden in order to preserve destruction order.  Override
     * {@link #onDestroyInternal()} instead to perform clean up tasks.
     */
    @SuppressLint("NewApi")
    @Override
    protected final void onDestroy() {
        if (mReaderModeManager != null) {
            mReaderModeManager.destroy();
            mReaderModeManager = null;
        }

        if (mContextualSearchManager != null) {
            mContextualSearchManager.destroy();
            mContextualSearchManager = null;
        }

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        if (mCompositorViewHolder != null) {
            if (mCompositorViewHolder.getLayoutManager() != null) {
                mCompositorViewHolder.getLayoutManager().removeSceneChangeObserver(this);
            }
            mCompositorViewHolder.shutDown();
            mCompositorViewHolder = null;
        }

        onDestroyInternal();

        if (mToolbarManager != null) {
            mToolbarManager.destroy();
            mToolbarManager = null;
        }

        if (mBottomSheet != null) {
            mBottomSheet.destroy();
            mBottomSheet = null;
        }

        if (mBottomSheetContentController != null) {
            mBottomSheetContentController.destroy();
            mBottomSheetContentController = null;
        }

        if (mTabModelsInitialized) {
            TabModelSelector selector = getTabModelSelector();
            if (selector != null) selector.destroy();
        }

        if (mDidAddPolicyChangeListener) {
            CombinedPolicyProvider.get().removePolicyChangeListener(this);
            mDidAddPolicyChangeListener = false;
        }

        if (mTabContentManager != null) {
            mTabContentManager.destroy();
            mTabContentManager = null;
        }

        AccessibilityManager manager = (AccessibilityManager)
                getBaseContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        manager.removeAccessibilityStateChangeListener(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            manager.removeTouchExplorationStateChangeListener(mTouchExplorationStateChangeListener);
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
     * @return The unified manager for all snackbar related operations.
     */
    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    protected Drawable getBackgroundDrawable() {
        return new ColorDrawable(
                ApiCompatibilityUtils.getColor(getResources(), R.color.light_background_color));
    }

    private void maybeRemoveWindowBackground() {
        // Only need to do this logic once.
        if (mRemoveWindowBackgroundDone) return;

        // Remove the window background only after native init and window getting focus. It's done
        // after native init because before native init, a fake background gets shown. The window
        // focus dependency is because doing it earlier can cause drawing bugs, e.g. crbug/673831.
        if (!mNativeInitialized || !hasWindowFocus()) return;

        // The window background color is used as the resizing background color in Android N+
        // multi-window mode. See crbug.com/602366.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            getWindow().setBackgroundDrawable(new ColorDrawable(
                    ApiCompatibilityUtils.getColor(getResources(),
                            R.color.resizing_background_color)));
        } else {
            // Post the removeWindowBackground() call as a separate task, as doing it synchronously
            // here can cause redrawing glitches. See crbug.com/686662 for an example problem.
            Handler handler = new Handler();
            handler.post(() -> removeWindowBackground());
        }

        mRemoveWindowBackgroundDone = true;
    }

    @Override
    public void finishNativeInitialization() {
        mNativeInitialized = true;
        OfflineContentAggregatorNotificationBridgeUiFactory.instance();
        maybeRemoveWindowBackground();
        DownloadManagerService.getDownloadManagerService().onActivityLaunched();

        if (getSavedInstanceState() == null && getIntent() != null) {
            VrShellDelegate.onNewIntentWithNative(this, getIntent());
        }
        VrShellDelegate.onNativeLibraryAvailable();
        super.finishNativeInitialization();
    }

    /**
     * Called when the accessibility status of this device changes.  This might be triggered by
     * touch exploration or general accessibility status updates.  It is an aggregate of two other
     * accessibility update methods.
     * @see #onAccessibilityModeChanged(boolean)
     * @see #onTouchExplorationStateChanged(boolean)
     * @param enabled Whether or not accessibility and touch exploration are currently enabled.
     */
    protected void onAccessibilityModeChanged(boolean enabled) {
        InfoBarContainer.setIsAllowedToAutoHide(!enabled);
        if (mToolbarManager != null) mToolbarManager.onAccessibilityStatusChanged(enabled);
        if (mContextualSearchManager != null) {
            mContextualSearchManager.onAccessibilityModeChanged(enabled);
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item != null && onMenuOrKeyboardAction(item.getItemId(), true)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @VisibleForTesting
    public void setScreenshotCaptureSkippedForTesting(boolean value) {
        mScreenshotCaptureSkippedForTesting = value;
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    @VisibleForTesting
    public void onShareMenuItemSelected(final boolean shareDirectly, final boolean isIncognito) {
        final Tab currentTab = getActivityTab();
        if (currentTab == null) return;

        List<Class<? extends ShareActivity>> classesToEnable = new ArrayList<>(2);

        if (PrintShareActivity.featureIsAvailable(currentTab)) {
            classesToEnable.add(PrintShareActivity.class);
        }
        if (PhysicalWebShareActivity.featureIsAvailable()) {
            classesToEnable.add(PhysicalWebShareActivity.class);
        }

        if (!classesToEnable.isEmpty()) {
            OptionalShareTargetsManager.enableOptionalShareActivities(
                    this, classesToEnable,
                    () -> triggerShare(currentTab, shareDirectly, isIncognito));
            return;
        }

        triggerShare(currentTab, shareDirectly, isIncognito);
    }

    private void triggerShare(
            final Tab currentTab, final boolean shareDirectly, boolean isIncognito) {
        final Activity mainActivity = this;
        WebContents webContents = currentTab.getWebContents();

        RecordHistogram.recordBooleanHistogram(
                "OfflinePages.SharedPageWasOffline", OfflinePageUtils.isOfflinePage(currentTab));
        boolean canShareOfflinePage = OfflinePageBridge.isPageSharingEnabled();

        // Share an empty blockingUri in place of screenshot file. The file ready notification is
        // sent by onScreenshotReady call below when the file is written.
        final Uri blockingUri = (isIncognito || webContents == null)
                ? null
                : ChromeFileProvider.generateUriAndBlockAccess(mainActivity);
        ShareParams.Builder builder =
                new ShareParams.Builder(mainActivity, currentTab.getTitle(), currentTab.getUrl())
                        .setShareDirectly(shareDirectly)
                        .setSaveLastUsed(!shareDirectly)
                        .setScreenshotUri(blockingUri);
        if (canShareOfflinePage) {
            OfflinePageUtils.shareOfflinePage(builder, currentTab);
        } else {
            ShareHelper.share(builder.build());
            if (shareDirectly) {
                RecordUserAction.record("MobileMenuDirectShare");
            } else {
                RecordUserAction.record("MobileMenuShare");
            }
        }

        if (blockingUri == null) return;

        // Start screenshot capture and notify the provider when it is ready.
        ContentBitmapCallback callback = (bitmap, response) -> ShareHelper.saveScreenshotToDisk(
                bitmap, mainActivity,
                result -> {
                    // Unblock the file once it is saved to disk.
                    ChromeFileProvider.notifyFileReady(blockingUri, result);
                });
        if (mScreenshotCaptureSkippedForTesting) {
            callback.onFinishGetBitmap(null, ReadbackResponse.SURFACE_UNAVAILABLE);
        } else {
            webContents.getContentBitmapAsync(0, 0, callback);
        }
    }

    /**
     * @return Whether the activity is in overview mode.
     */
    public boolean isInOverviewMode() {
        return false;
    }

    /**
     * @return Whether the app menu should be shown.
     */
    public boolean shouldShowAppMenu() {
        // Do not show the menu if Contextual Search panel is opened.
        if (mContextualSearchManager != null && mContextualSearchManager.isSearchPanelOpened()) {
            return false;
        }

        // Do not show the menu if we are in find in page view.
        if (mFindToolbarManager != null && mFindToolbarManager.isShowing() && !isTablet()) {
            return false;
        }

        return true;
    }

    /**
    * Shows the app menu (if possible) for a key press on the keyboard with the correct anchor view
    * chosen depending on device configuration and the visible menu button to the user.
    */
    protected void showAppMenuForKeyboardEvent() {
        if (getAppMenuHandler() == null) return;

        boolean hasPermanentMenuKey = ViewConfiguration.get(this).hasPermanentMenuKey();
        getAppMenuHandler().showAppMenu(
                hasPermanentMenuKey ? null : getToolbarManager().getMenuButton(), false);
    }

    /**
     * Allows Activities that extend ChromeActivity to do additional hiding/showing of menu items.
     * @param menu Menu that is going to be shown when the menu button is pressed.
     */
    public void prepareMenu(Menu menu) {
    }

    protected IntentHandlerDelegate createIntentHandlerDelegate() {
        return new IntentHandlerDelegate() {
            @Override
            public void processWebSearchIntent(String query) {
                final Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
                searchIntent.putExtra(SearchManager.QUERY, query);
                Callback<Boolean> callback = result -> {
                    if (result != null && result) startActivity(searchIntent);
                };
                LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                        ChromeActivity.this, callback);
            }

            @Override
            public void processUrlViewIntent(String url, String referer, String headers,
                    TabOpenType tabOpenType, String externalAppId, int tabIdToBringToFront,
                    boolean hasUserGesture, Intent intent) {
            }
        };
    }

    /**
     * @return The resource id that contains how large the browser controls are.
     */
    public int getControlContainerHeightResource() {
        if (mBottomSheet != null) return R.dimen.bottom_control_container_height;
        return R.dimen.control_container_height;
    }

    @Override
    public final void onAccessibilityStateChanged(boolean enabled) {
        checkAccessibility();
    }

    private void checkAccessibility() {
        onAccessibilityModeChanged(AccessibilityUtil.isAccessibilityEnabled());
    }

    /**
     * @return A casted version of {@link #getApplication()}.
     */
    public ChromeApplication getChromeApplication() {
        return (ChromeApplication) getApplication();
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

        // Defense in depth against the UI being erroneously enabled.
        if (!mToolbarManager.getBookmarkBridge().isEditBookmarksEnabled()) {
            assert false;
            return;
        }

        // Note the use of getUserBookmarkId() over getBookmarkId() here: Managed bookmarks can't be
        // edited. If the current URL is only bookmarked by managed bookmarks, this will return
        // INVALID_BOOKMARK_ID, so the code below will fall back on adding a new bookmark instead.
        // TODO(bauerb): This does not take partner bookmarks into account.
        final long bookmarkId = tabToBookmark.getUserBookmarkId();

        final BookmarkModel bookmarkModel = new BookmarkModel();
        bookmarkModel.runAfterBookmarkModelLoaded(() -> {
            // Gives up the bookmarking if the tab is being destroyed.
            if (!tabToBookmark.isClosing() && tabToBookmark.isInitialized()) {
                // The BookmarkModel will be destroyed by BookmarkUtils#addOrEditBookmark() when
                // done.
                BookmarkId newBookmarkId = BookmarkUtils.addOrEditBookmark(bookmarkId,
                        bookmarkModel, tabToBookmark, getSnackbarManager(), ChromeActivity.this,
                        isCustomTab());
                // If a new bookmark was created, try to save an offline page for it.
                if (newBookmarkId != null && newBookmarkId.getId() != bookmarkId) {
                    OfflinePageUtils.saveBookmarkOffline(newBookmarkId, tabToBookmark);
                }
            } else {
                bookmarkModel.destroy();
            }
        });
    }

    /**
     * @return Whether the tab models have been fully initialized.
     */
    public boolean areTabModelsInitialized() {
        return mTabModelsInitialized;
    }

    /**
     * {@link TabModelSelector} no longer implements TabModel.  Use getTabModelSelector() or
     * getCurrentTabModel() depending on your needs.
     * @return The {@link TabModelSelector}, possibly null.
     */
    public TabModelSelector getTabModelSelector() {
        if (!mTabModelsInitialized) {
            throw new IllegalStateException(
                    "Attempting to access TabModelSelector before initialization");
        }
        return mTabModelSelector;
    }

    /**
     * Returns the {@link InsetObserverView} that has the current system window
     * insets information.
     * @return The {@link InsetObserverView}, possibly null.
     */
    public InsetObserverView getInsetObserverView() {
        return mInsetObserverView;
    }

    @Override
    public TabCreatorManager.TabCreator getTabCreator(boolean incognito) {
        if (!mTabModelsInitialized) {
            throw new IllegalStateException(
                    "Attempting to access TabCreator before initialization");
        }
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }

    /**
     * Convenience method that returns a tab creator for the currently selected {@link TabModel}.
     * @return A tab creator for the currently selected {@link TabModel}.
     */
    public TabCreatorManager.TabCreator getCurrentTabCreator() {
        return getTabCreator(getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Gets the {@link TabContentManager} instance which holds snapshots of the tabs in this model.
     * @return The thumbnail cache, possibly null.
     */
    public TabContentManager getTabContentManager() {
        return mTabContentManager;
    }

    /**
     * Sets the {@link TabContentManager} owned by this {@link ChromeActivity}.
     * @param tabContentManager A {@link TabContentManager} instance.
     */
    protected void setTabContentManager(TabContentManager tabContentManager) {
        mTabContentManager = tabContentManager;
    }

    /**
     * Gets the current (inner) TabModel.  This is a convenience function for
     * getModelSelector().getCurrentModel().  It is *not* equivalent to the former getModel()
     * @return Never null, if modelSelector or its field is uninstantiated returns a
     *         {@link EmptyTabModel} singleton
     */
    public TabModel getCurrentTabModel() {
        TabModelSelector modelSelector = getTabModelSelector();
        if (modelSelector == null) return EmptyTabModel.getInstance();
        return modelSelector.getCurrentModel();
    }

    /**
     * Returns the tab being displayed by this ChromeActivity instance. This allows differentiation
     * between ChromeActivity subclasses that swap between multiple tabs (e.g. ChromeTabbedActivity)
     * and subclasses that only display one Tab (e.g. FullScreenActivity and DocumentActivity).
     *
     * The default implementation grabs the tab currently selected by the TabModel, which may be
     * null if the Tab does not exist or the system is not initialized.
     */
    public Tab getActivityTab() {
        return TabModelUtils.getCurrentTab(getCurrentTabModel());
    }

    /**
     * @return The current ContentViewCore, or null if the tab does not exist or is not showing a
     *         ContentViewCore.
     */
    public ContentViewCore getCurrentContentViewCore() {
        return TabModelUtils.getCurrentContentViewCore(getCurrentTabModel());
    }

    /**
     * @return A {@link CompositorViewHolder} instance.
     */
    public CompositorViewHolder getCompositorViewHolder() {
        return mCompositorViewHolder;
    }

    /**
     * Gets the full screen manager.
     * @return The fullscreen manager, possibly null
     */
    public ChromeFullscreenManager getFullscreenManager() {
        if (!mCreatedFullscreenManager) {
            throw new IllegalStateException(
                    "Attempting to access FullscreenManager before it has been created.");
        }
        return mFullscreenManager;
    }

    /**
     * Sets the overlay mode.
     * Overlay mode means that we are currently using AndroidOverlays to display video, and
     * that the compositor's surface should support alpha and not be marked as opaque.
     */
    public void setOverlayMode(boolean useOverlayMode) {
        if (mCompositorViewHolder != null) mCompositorViewHolder.setOverlayMode(useOverlayMode);
    }

    /**
     * @return The content offset provider, may be null.
     */
    public ContentOffsetProvider getContentOffsetProvider() {
        return mCompositorViewHolder;
    }

    /**
     * @return The {@code ContextualSearchManager} or {@code null} if none;
     */
    public ContextualSearchManager getContextualSearchManager() {
        return mContextualSearchManager;
    }

    /**
     * @return The {@code ReaderModeManager} or {@code null} if none;
     */
    @VisibleForTesting
    public ReaderModeManager getReaderModeManager() {
        return mReaderModeManager;
    }

    /**
     * Create a full-screen manager to be used by this activity.
     * Note: This is called during {@link #postInflationStartup}, so native code may not have been
     * initialized, but Android Views will have been.
     * @return A {@link ChromeFullscreenManager} instance that's been created.
     */
    protected ChromeFullscreenManager createFullscreenManager() {
        return new ChromeFullscreenManager(this, false);
    }

    /**
     * Exits the fullscreen mode, if any. Does nothing if no fullscreen is present.
     * @return Whether the fullscreen mode is currently showing.
     */
    protected boolean exitFullscreenIfShowing() {
        ContentVideoView view = ContentVideoView.getContentVideoView();
        if (view != null && view.getContext() == this) {
            view.exitFullscreen(false);
            return true;
        }
        if (getFullscreenManager() != null
                && getFullscreenManager().getPersistentFullscreenMode()) {
            getFullscreenManager().setPersistentFullscreenMode(false);
            return true;
        }
        return false;
    }

    /**
     * Initializes the {@link CompositorViewHolder} with the relevant content it needs to properly
     * show content on the screen.
     * @param layoutManager             A {@link LayoutManager} instance.  This class is
     *                                  responsible for driving all high level screen content and
     *                                  determines which {@link Layout} is shown when.
     * @param urlBar                    The {@link View} representing the URL bar (must be
     *                                  focusable) or {@code null} if none exists.
     * @param contentContainer          A {@link ViewGroup} that can have content attached by
     *                                  {@link Layout}s.
     * @param controlContainer          A {@link ControlContainer} instance to draw.
     */
    protected void initializeCompositorContent(LayoutManager layoutManager, View urlBar,
            ViewGroup contentContainer, ControlContainer controlContainer) {
        if (mContextualSearchManager != null) {
            mContextualSearchManager.initialize(contentContainer);
            mContextualSearchManager.setSearchContentViewDelegate(layoutManager);
        }

        layoutManager.addSceneChangeObserver(this);
        mCompositorViewHolder.setLayoutManager(layoutManager);
        mCompositorViewHolder.setFocusable(false);
        mCompositorViewHolder.setControlContainer(controlContainer);
        mCompositorViewHolder.setFullscreenHandler(getFullscreenManager());
        mCompositorViewHolder.setUrlBar(urlBar);
        mCompositorViewHolder.onFinishNativeInitialization(getTabModelSelector(), this,
                getTabContentManager(), contentContainer, mContextualSearchManager);

        if (controlContainer != null && DeviceClassManager.enableToolbarSwipe()
                && getCompositorViewHolder().getLayoutManager().getTopSwipeHandler() != null) {
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
        if (mToolbarManager != null) mToolbarManager.onOrientationChange();
    }

    /**
     * Notified when the focus of the omnibox has changed.
     *
     * @param hasFocus Whether the omnibox currently has focus.
     */
    protected void onOmniboxFocusChanged(boolean hasFocus) {}

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mAppMenuHandler != null) mAppMenuHandler.hideAppMenu();
        super.onConfigurationChanged(newConfig);

        // We only handle VR UI mode changes. Any other changes should follow the default behavior
        // of recreating the activity.
        if (didChangeNonVrUiMode(mUiMode, newConfig.uiMode)) {
            recreate();
            return;
        }
        mUiMode = newConfig.uiMode;

        if (newConfig.screenWidthDp != mScreenWidthDp) {
            mScreenWidthDp = newConfig.screenWidthDp;
            final Activity activity = this;

            if (mRecordMultiWindowModeScreenWidthRunnable != null) {
                mHandler.removeCallbacks(mRecordMultiWindowModeScreenWidthRunnable);
            }

            // When exiting Android N multi-window mode, onConfigurationChanged() gets called before
            // isInMultiWindowMode() returns false. Delay to avoid recording width when exiting
            // multi-window mode. This also ensures that we don't record intermediate widths seen
            // only for a brief period of time.
            mRecordMultiWindowModeScreenWidthRunnable = () -> {
                mRecordMultiWindowModeScreenWidthRunnable = null;
                if (MultiWindowUtils.getInstance().isInMultiWindowMode(activity)) {
                    recordMultiWindowModeScreenWidth();
                }
            };
            mHandler.postDelayed(mRecordMultiWindowModeScreenWidthRunnable,
                    RECORD_MULTI_WINDOW_SCREEN_WIDTH_DELAY_MS);
        }
    }

    private static boolean didChangeNonVrUiMode(int oldMode, int newMode) {
        if (oldMode == newMode) return false;
        return isInVrUiMode(oldMode) == isInVrUiMode(newMode);
    }

    private static boolean isInVrUiMode(int uiMode) {
        // TODO(mthiesse): Use Configuration.UI_MODE_TYPE_VR_HEADSET when building against the O
        // sdk.
        final int uiModeTypeVrHeadset = 0x07;
        return (uiMode & Configuration.UI_MODE_TYPE_MASK) == uiModeTypeVrHeadset;
    }

    /**
     * Called by the system when the activity changes from fullscreen mode to multi-window mode
     * and visa-versa.
     * @param isInMultiWindowMode True if the activity is in multi-window mode.
     */
    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        recordMultiWindowModeChangedUserAction(isInMultiWindowMode);

        if (!isInMultiWindowMode
                && ApplicationStatus.getStateForActivity(this) == ActivityState.RESUMED) {
            // Start a new UMA session when exiting multi-window mode if the activity is currently
            // resumed. When entering multi-window Android recents gains focus, so ChromeActivity
            // will get a call to onPauseWithNative(), ending the current UMA session. When exiting
            // multi-window, however, if ChromeActivity is resumed it stays in that state.
            markSessionEnd();
            markSessionResume();
            FeatureUtilities.setIsInMultiWindowMode(
                    MultiWindowUtils.getInstance().isInMultiWindowMode(this));
        }

        VrShellDelegate.onMultiWindowModeChanged(isInMultiWindowMode);

        super.onMultiWindowModeChanged(isInMultiWindowMode);
    }

    /**
     * Records user actions associated with entering and exiting Android N multi-window mode
     * @param isInMultiWindowMode True if the activity is in multi-window mode.
     */
    protected void recordMultiWindowModeChangedUserAction(boolean isInMultiWindowMode) {
        if (isInMultiWindowMode) {
            RecordUserAction.record("Android.MultiWindowMode.Enter");
        } else {
            RecordUserAction.record("Android.MultiWindowMode.Exit");
        }
    }

    @Override
    public final void onBackPressed() {
        if (mNativeInitialized) RecordUserAction.record("SystemBack");

        TextBubble.onBackPressed();
        if (VrShellDelegate.onBackPressed()) return;
        if (mCompositorViewHolder != null) {
            LayoutManager layoutManager = mCompositorViewHolder.getLayoutManager();
            if (layoutManager != null && layoutManager.onBackPressed()) return;
        }

        ContentViewCore contentViewCore = getContentViewCore();
        if (contentViewCore != null && contentViewCore.isSelectActionBarShowing()) {
            contentViewCore.clearSelection();
            return;
        }

        if (mContextualSearchManager != null && mContextualSearchManager.onBackPressed()) return;

        if (handleBackPressed()) return;

        super.onBackPressed();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        // The conditions are expressed using ranges to capture intermediate levels possibly added
        // to the API in the future.
        if ((level >= TRIM_MEMORY_RUNNING_LOW && level < TRIM_MEMORY_UI_HIDDEN)
                || level >= TRIM_MEMORY_MODERATE) {
            mReferencePool.drain();
        }
    }

    private ContentViewCore getContentViewCore() {
        Tab tab = getActivityTab();
        if (tab == null) return null;
        return tab.getContentViewCore();
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        Tab currentTab = getActivityTab();
        if (currentTab == null) return;

        TabCreator tabCreator = getTabCreator(currentTab.isIncognito());
        if (tabCreator == null) return;

        tabCreator.createNewTab(
                new LoadUrlParams(searchUrl, PageTransition.LINK),
                TabModel.TabLaunchType.FROM_LINK, getActivityTab());
    }

    /**
     * @return The {@link AppMenuHandler} associated with this activity.
     */
    @VisibleForTesting
    public AppMenuHandler getAppMenuHandler() {
        return mAppMenuHandler;
    }

    /**
     * @return The {@link AppMenuPropertiesDelegate} associated with this activity.
     */
    @VisibleForTesting
    public AppMenuPropertiesDelegate getAppMenuPropertiesDelegate() {
        return mAppMenuPropertiesDelegate;
    }

    /**
     * Callback after UpdateMenuItemHelper#checkForUpdateOnBackgroundThread is complete.
     * @param updateAvailable Whether an update is available.
     */
    public void onCheckForUpdate(boolean updateAvailable) {
        if (UpdateMenuItemHelper.getInstance().shouldShowToolbarBadge(this)) {
            mToolbarManager.getToolbar().showAppMenuUpdateBadge();
            mCompositorViewHolder.requestRender();
        } else {
            mToolbarManager.getToolbar().removeAppMenuUpdateBadge(false);
        }
    }

    /**
     * Handles menu item selection and keyboard shortcuts.
     *
     * @param id The ID of the selected menu item (defined in main_menu.xml) or
     *           keyboard shortcut (defined in values.xml).
     * @param fromMenu Whether this was triggered from the menu.
     * @return Whether the action was handled.
     */
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.preferences_id) {
            PreferencesLauncher.launchSettingsPage(this, null);
            RecordUserAction.record("MobileMenuSettings");
        } else if (id == R.id.show_menu) {
            showAppMenuForKeyboardEvent();
        } else if (id == R.id.find_in_page_id) {
            if (mFindToolbarManager == null) return false;

            mFindToolbarManager.showToolbar();
            if (mContextualSearchManager != null) {
                getContextualSearchManager().hideContextualSearch(StateChangeReason.UNKNOWN);
            }
            if (fromMenu) {
                RecordUserAction.record("MobileMenuFindInPage");
            } else {
                RecordUserAction.record("MobileShortcutFindInPage");
            }
            return true;
        }

        if (id == R.id.update_menu_id) {
            UpdateMenuItemHelper.getInstance().onMenuItemClicked(this);
            return true;
        }

        final Tab currentTab = getActivityTab();

        if (id == R.id.help_id) {
            String url = currentTab != null
                    ? currentTab.getUrl()
                    : getBottomSheet() != null && mBottomSheet.isShowingNewTab()
                            ? UrlConstants.NTP_URL
                            : "";
            Profile profile = mTabModelSelector.isIncognitoSelected()
                    ? Profile.getLastUsedProfile().getOffTheRecordProfile()
                    : Profile.getLastUsedProfile().getOriginalProfile();
            startHelpAndFeedback(url, "MobileMenuFeedback", profile);
            return true;
        }

        // All the code below assumes currentTab is not null, so return early if it is null.
        if (currentTab == null) {
            return false;
        } else if (id == R.id.forward_menu_id) {
            if (currentTab.canGoForward()) {
                currentTab.goForward();
                RecordUserAction.record("MobileMenuForward");
            }
        } else if (id == R.id.bookmark_this_page_id) {
            addOrEditBookmark(currentTab);
            RecordUserAction.record("MobileMenuAddToBookmarks");
        } else if (id == R.id.offline_page_id) {
            DownloadUtils.downloadOfflinePage(this, currentTab);
            RecordUserAction.record("MobileMenuDownloadPage");
        } else if (id == R.id.reload_menu_id) {
            if (currentTab.isLoading()) {
                currentTab.stopLoading();
                RecordUserAction.record("MobileMenuStop");
            } else {
                currentTab.reload();
                RecordUserAction.record("MobileMenuReload");
            }
        } else if (id == R.id.info_menu_id) {
            PageInfoPopup.show(this, currentTab, null, PageInfoPopup.OPENED_FROM_MENU);
        } else if (id == R.id.open_history_menu_id) {
            if (NewTabPage.isNTPUrl(currentTab.getUrl())) {
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_HISTORY_MANAGER);
            }
            RecordUserAction.record("MobileMenuHistory");
            HistoryManagerUtils.showHistoryManager(this, currentTab);
            StartupMetrics.getInstance().recordOpenedHistory();
        } else if (id == R.id.share_menu_id || id == R.id.direct_share_menu_id) {
            onShareMenuItemSelected(id == R.id.direct_share_menu_id,
                    getCurrentTabModel().isIncognito());
        } else if (id == R.id.print_id) {
            PrintingController printingController = PrintingControllerImpl.getInstance();
            if (printingController != null && !printingController.isBusy()
                    && PrefServiceBridge.getInstance().isPrintingEnabled()) {
                printingController.startPrint(new TabPrinter(currentTab),
                        new PrintManagerDelegateImpl(this));
                RecordUserAction.record("MobileMenuPrint");
            }
        } else if (id == R.id.add_to_homescreen_id) {
            // Record whether or not we have finished installability checks for this page when the
            // user clicks the add to homescren menu item. This will let us determine how effective
            // an on page-load check will be in speeding up WebAPK installation.
            currentTab.getAppBannerManager().recordMenuItemAddToHomescreen();

            AddToHomescreenManager addToHomescreenManager =
                    new AddToHomescreenManager(this, currentTab);
            addToHomescreenManager.start();
            RecordUserAction.record("MobileMenuAddToHomescreen");
        } else if (id == R.id.open_webapk_id) {
            Context context = ContextUtils.getApplicationContext();
            String packageName = WebApkValidator.queryWebApkPackage(context, currentTab.getUrl());
            Intent launchIntent = WebApkNavigationClient.createLaunchWebApkIntent(
                    packageName, currentTab.getUrl(), false);
            try {
                context.startActivity(launchIntent);
                RecordUserAction.record("MobileMenuOpenWebApk");
                WebApkUma.recordWebApkOpenAttempt(WebApkUma.WEBAPK_OPEN_LAUNCH_SUCCESS);
            } catch (ActivityNotFoundException e) {
                WebApkUma.recordWebApkOpenAttempt(WebApkUma.WEBAPK_OPEN_ACTIVITY_NOT_FOUND);
                Toast.makeText(context, R.string.open_webapk_failed, Toast.LENGTH_SHORT).show();
            }
        } else if (id == R.id.request_desktop_site_id || id == R.id.request_desktop_site_check_id) {
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
        } else {
            return false;
        }
        return true;
    }

    /**
     * Shows HelpAndFeedback and records the user action as well.
     * @param url The URL of the tab the user is currently on.
     * @param recordAction The user action to record.
     * @param profile The current {@link Profile}.
     */
    public void startHelpAndFeedback(String url, String recordAction, Profile profile) {
        // Since reading back the compositor is asynchronous, we need to do the readback
        // before starting the GoogleHelp.
        String helpContextId = HelpAndFeedback.getHelpContextIdFromUrl(
                this, url, getCurrentTabModel().isIncognito());
        HelpAndFeedback.getInstance(this).show(this, helpContextId, profile, url);
        RecordUserAction.record(recordAction);
    }

    /**
     * Add a view to the set of views that obscure the content of all tabs for
     * accessibility. As long as this set is nonempty, all tabs should be
     * hidden from the accessibility tree.
     *
     * @param view The view that obscures the contents of all tabs.
     */
    public void addViewObscuringAllTabs(View view) {
        mViewsObscuringAllTabs.add(view);

        Tab tab = getActivityTab();
        if (tab != null) tab.updateAccessibilityVisibility();
    }

    /**
     * Remove a view that previously obscured the content of all tabs.
     *
     * @param view The view that no longer obscures the contents of all tabs.
     */
    public void removeViewObscuringAllTabs(View view) {
        mViewsObscuringAllTabs.remove(view);

        Tab tab = getActivityTab();
        if (tab != null) tab.updateAccessibilityVisibility();
    }

    /**
     * Returns whether or not any views obscure all tabs.
     */
    public boolean isViewObscuringAllTabs() {
        return !mViewsObscuringAllTabs.isEmpty();
    }

    private void markSessionResume() {
        // Start new session for UMA.
        if (mUmaSessionStats == null) {
            mUmaSessionStats = new UmaSessionStats(this);
        }

        UmaSessionStats.updateMetricsServiceState();
        mUmaSessionStats.startNewSession(getTabModelSelector());
    }

    /**
     * Mark that the UMA session has ended.
     */
    private void markSessionEnd() {
        if (mUmaSessionStats == null) {
            // If you hit this assert, please update crbug.com/172653 on how you got there.
            assert false;
            return;
        }
        // Record session metrics.
        mUmaSessionStats.logMultiWindowStats(windowArea(), displayArea(),
                TabWindowManager.getInstance().getNumberOfAssignedTabModelSelectors());
        mUmaSessionStats.logAndEndSession();
    }

    private int windowArea() {
        Window window = getWindow();
        if (window != null) {
            View view =  window.getDecorView();
            return view.getWidth() * view.getHeight();
        }
        return -1;
    }

    private int displayArea() {
        if (getResources() != null && getResources().getDisplayMetrics() != null) {
            DisplayMetrics metrics = getResources().getDisplayMetrics();
            return metrics.heightPixels * metrics.widthPixels;
        }
        return -1;
    }

    protected final void postDeferredStartupIfNeeded() {
        if (!mNativeInitialized) {
            // Native hasn't loaded yet.  Queue it up for later.
            mDeferredStartupQueued = true;
            return;
        }
        mDeferredStartupQueued = false;

        if (!mDeferredStartupPosted) {
            mDeferredStartupPosted = true;
            onDeferredStartup();
        }
    }

    @Override
    public void terminateIncognitoSession() {}

    @Override
    public void onTabSelectionHinted(int tabId) { }

    @Override
    public void onSceneChange(Layout layout) { }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        // See enableHardwareAcceleration()
        if (mSetWindowHWA) {
            mSetWindowHWA = false;
            getWindow().setWindowManager(
                    getWindow().getWindowManager(),
                    getWindow().getAttributes().token,
                    getComponentName().flattenToString(),
                    true /* hardwareAccelerated */);
        }
    }

    private boolean shouldDisableHardwareAcceleration() {
        // Low end devices should disable hardware acceleration for memory gains.
        if (SysUtils.isLowEndDevice()) return true;

        // Turning off hardware acceleration reduces crash rates. See http://crbug.com/651918
        // GT-S7580 on JDQ39 accounts for 42% of crashes in libPowerStretch.so on dev and beta.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.JELLY_BEAN_MR1
                && Build.MODEL.equals("GT-S7580")) {
            return true;
        }
        // SM-N9005 on JSS15J accounts for 44% of crashes in libPowerStretch.so on stable channel.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.JELLY_BEAN_MR2
                && Build.MODEL.equals("SM-N9005")) {
            return true;
        }
        return false;
    }

    private void enableHardwareAcceleration() {
        // HW acceleration is disabled in the manifest and may be re-enabled here.
        if (!shouldDisableHardwareAcceleration()) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED);

            // When HW acceleration is enabled manually for an activity, child windows (e.g.
            // dialogs) don't inherit HW acceleration state. However, when HW acceleration is
            // enabled in the manifest, child windows do inherit HW acceleration state. That
            // looks like a bug, so I filed b/23036374
            //
            // In the meanwhile the workaround is to call
            //   window.setWindowManager(..., hardwareAccelerated=true)
            // to let the window know that it's HW accelerated. However, since there is no way
            // to know 'appToken' argument until window's view is attached to the window (!!),
            // we have to do the workaround in onAttachedToWindow()
            mSetWindowHWA = true;
        }
    }

    /** @return the theme ID to use. */
    public static int getThemeId() {
        boolean useLowEndTheme =
                SysUtils.isLowEndDevice() && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
        return (useLowEndTheme ? R.style.MainTheme_LowEnd : R.style.MainTheme);
    }

    /**
     * Looks up the Chrome activity of the given web contents. This can be null. Should never be
     * cached, because web contents can change activities, e.g., when user selects "Open in Chrome"
     * menu item.
     *
     * @param webContents The web contents for which to lookup the Chrome activity.
     * @return Possibly null Chrome activity that should never be cached.
     */
    @Nullable public static ChromeActivity fromWebContents(@Nullable WebContents webContents) {
        if (webContents == null) return null;

        if (webContents.isDestroyed()) return null;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;

        Activity activity = window.getActivity().get();
        if (activity == null) return null;
        if (!(activity instanceof ChromeActivity)) return null;

        return (ChromeActivity) activity;
    }

    private void setLowEndTheme() {
        if (getThemeId() == R.style.MainTheme_LowEnd) setTheme(R.style.MainTheme_LowEnd);
    }

    /**
     * Records UMA histograms for the current screen width. Should only be called when the activity
     * is in Android N multi-window mode.
     */
    protected void recordMultiWindowModeScreenWidth() {
        if (!DeviceFormFactor.isTablet()) return;

        RecordHistogram.recordBooleanHistogram(
                "Android.MultiWindowMode.IsTabletScreenWidthBelow600",
                mScreenWidthDp < DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP);

        if (mScreenWidthDp < DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP) {
            RecordHistogram.recordLinearCountHistogram(
                    "Android.MultiWindowMode.TabletScreenWidth", mScreenWidthDp, 1,
                    DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP, 50);
        }
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        if (super.onActivityResultWithNative(requestCode, resultCode, intent)) return true;
        if (VrShellDelegate.onActivityResultWithNative(requestCode, resultCode)) return true;
        return false;
    }

    /**
     * Called when VR mode is entered using this activity. 2D UI components that steal focus or
     * draw over VR contents should be hidden in this call.
     */
    public void onEnterVr() {}

    /**
     * Called when VR mode using this activity is exited. Any state set for VR should be restored
     * in this call, including showing 2D UI that was hidden.
     */
    public void onExitVr() {}

    /**
     * Whether this Activity supports moving a {@link Tab} to the {@link FullscreenActivity} when it
     * enters fullscreen.
     */
    public boolean supportsFullscreenActivity() {
        return false;
    }

    /**
     * @return the reference pool for this activity.
     */
    public DiscardableReferencePool getReferencePool() {
        return mReferencePool;
    }
}
