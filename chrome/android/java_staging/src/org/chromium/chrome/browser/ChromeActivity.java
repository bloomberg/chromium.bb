// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.SearchManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.os.MessageQueue;
import android.os.StrictMode;
import android.os.SystemClock;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Menu;
import android.view.MenuItem;
import android.view.Surface;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityManager.TouchExplorationStateChangeListener;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gsa.ContextReporter;
import org.chromium.chrome.browser.gsa.GSAServiceClient;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.nfc.BeamController;
import org.chromium.chrome.browser.nfc.BeamProvider;
import org.chromium.chrome.browser.omaha.OmahaClient;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.policy.PolicyManager.PolicyChangeListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.content.browser.ContentReadbackHandler;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.readback_types.ReadbackResponse;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

import java.lang.reflect.Field;

/**
 * The main Chrome activity.  This exposes extra methods that relate to the {@link TabModelSelector}
 * and lists of tabs.
 */
public abstract class ChromeActivity extends AsyncInitializationActivity implements
        TabCreatorManager, AccessibilityStateChangeListener, PolicyChangeListener {

    private static final String SNAPSHOT_DATABASE_REMOVED = "snapshot_database_removed";
    private static final String SNAPSHOT_DATABASE_NAME = "snapshots.db";

    /** Delay in ms after first page load finishes before we initiate deferred startup actions. */
    private static final int DEFERRED_STARTUP_DELAY_MS = 1000;

    /**
     * Timeout in ms for reading PartnerBrowserCustomizations provider.
     */
    private static final int PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS = 10000;

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private TabCreatorManager.TabCreator mRegularTabCreator;
    private TabCreatorManager.TabCreator mIncognitoTabCreator;
    private TabContentManager mTabContentManager;
    private UmaSessionStats mUmaSessionStats;
    private ContextReporter mContextReporter;
    protected GSAServiceClient mGSAServiceClient;

    private int mCurrentOrientation = Surface.ROTATION_0;
    private boolean mPartnerBrowserRefreshNeeded = false;

    protected IntentHandler mIntentHandler;
    protected boolean mIsTablet = false;
    private long mLastUserInteractionTime;

    /** Whether onDeferredStartup() has been run. */
    private boolean mDeferredStartupNotified;

    // The class cannot implement TouchExplorationStateChangeListener,
    // because it is only available for Build.VERSION_CODES.KITKAT and later.
    // We have to instantiate the TouchExplorationStateChangeListner object in the code.
    private TouchExplorationStateChangeListener mTouchExplorationStateChangeListener;

    // Observes when sync becomes ready to create the mContextReporter.
    private ProfileSyncService.SyncStateChangedListener mSyncStateChangedListener;

    @SuppressLint("NewApi")
    @Override
    protected void onDestroy() {
        getChromeApplication().removePolicyChangeListener(this);
        if (mTabContentManager != null) mTabContentManager.destroy();
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();

        AccessibilityManager manager = (AccessibilityManager)
                getBaseContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        manager.removeAccessibilityStateChangeListener(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            manager.removeTouchExplorationStateChangeListener(mTouchExplorationStateChangeListener);
        }

        super.onDestroy();
    }

    /**
     * {@link TabModelSelector} no longer implements TabModel.  Use getTabModelSelector() or
     * getCurrentTabModel() depending on your needs.
     * @return The {@link TabModelSelector}, possibly null.
     */
    public TabModelSelector getTabModelSelector() {
        return mTabModelSelector;
    }

    /**
     * Sets the {@link TabModelSelector} owned by this {@link ChromeActivity}.
     * @param tabModelSelector A {@link TabModelSelector} instance.
     */
    protected void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;

        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(tabModelSelector) {
            @Override
            public void onLoadStopped(Tab tab) {
                postDeferredStartupIfNeeded();
                showUpdateInfoBarIfNecessary();
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                postDeferredStartupIfNeeded();
                showUpdateInfoBarIfNecessary();
            }

            @Override
            public void onCrash(Tab tab, boolean sadTabShown) {
                postDeferredStartupIfNeeded();
            }
        };
    }

    @Override
    public TabCreatorManager.TabCreator getTabCreator(boolean incognito) {
        return incognito ? mIncognitoTabCreator : mRegularTabCreator;
    }

    /**
     * Sets the {@link ChromeTabCreator}s owned by this {@link ChromeActivity}.
     * @param regularTabCreator A {@link ChromeTabCreator} instance.
     */
    public void setTabCreators(TabCreatorManager.TabCreator regularTabCreator,
            TabCreatorManager.TabCreator incognitoTabCreator) {
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
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
     * Gets the full screen manager.
     * @return The fullscreen manager, possibly null
     */
    public FullscreenManager getFullscreenManager() {
        return null;
    }

    /**
     * @return The content offset provider, may be null.
     */
    public ContentOffsetProvider getContentOffsetProvider() {
        return null;
    }

    /**
     * @return The content readback handler, may be null.
     */
    public ContentReadbackHandler getContentReadbackHandler() {
        return null;
    }

    @Override
    public void preInflationStartup() {
        super.preInflationStartup();
        ApplicationInitialization.enableFullscreenFlags(
                getResources(), this, getControlContainerHeightResource());
        mIsTablet = DeviceFormFactor.isTablet(this);
        getWindow().setBackgroundDrawableResource(R.color.light_background_color);
    }

    @SuppressLint("NewApi")
    @Override
    public void postInflationStartup() {
        super.postInflationStartup();
        // Low end device UI should be allowed only after a fresh install or when the data has
        // been cleared. This must happen before anyone calls SysUtils.isLowEndDevice() or
        // SysUtils.isLowEndDevice() will always return the wrong value.
        // Temporarily allowing disk access. TODO: Fix. See http://crbug.com/473352
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            if (OmahaClient.isFreshInstallOrDataHasBeenCleared(this)) {
                ChromePreferenceManager.getInstance(this).setAllowLowEndDeviceUi();
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        if (!ChromePreferenceManager.getInstance(this).getAllowLowEndDeviceUi()) {
            CommandLine.getInstance().appendSwitch(
                    BaseSwitches.DISABLE_LOW_END_DEVICE_MODE);
        }

        AccessibilityManager manager = (AccessibilityManager)
                getBaseContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        manager.addAccessibilityStateChangeListener(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            mTouchExplorationStateChangeListener = new TouchExplorationStateChangeListener() {
                @Override
                public void onTouchExplorationStateChanged(boolean enabled) {
                    checkAccessibility();
                }
            };
            manager.addTouchExplorationStateChangeListener(mTouchExplorationStateChangeListener);
        }

        // Make the activity listen to policy change events
        getChromeApplication().addPolicyChangeListener(this);
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
    }

    @Override
    public void initializeState() {
        super.initializeState();
        IntentHandler.setTestIntentsEnabled(
                CommandLine.getInstance().hasSwitch(ContentSwitches.ENABLE_TEST_INTENTS));
        mIntentHandler = new IntentHandler(createIntentHandlerDelegate(), getPackageName());
    }

    @Override
    public void finishNativeInitialization() {
        // Set up the initial orientation of the device.
        checkOrientation();
        findViewById(android.R.id.content).addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(View v, int left, int top, int right, int bottom,
                            int oldLeft, int oldTop, int oldRight, int oldBottom) {
                        checkOrientation();
                    }
                });
        super.finishNativeInitialization();
    }

    @Override
    public void onStart() {
        super.onStart();
        if (mContextReporter != null) mContextReporter.enable();

        if (mPartnerBrowserRefreshNeeded) {
            mPartnerBrowserRefreshNeeded = false;
            PartnerBrowserCustomizations.initializeAsync(getApplicationContext(),
                    PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS);
            PartnerBrowserCustomizations.setOnInitializeAsyncFinished(new Runnable() {
                @Override
                public void run() {
                    if (PartnerBrowserCustomizations.isIncognitoDisabled()) {
                        terminateIncognitoSession();
                    }
                }
            });
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mContextReporter != null) mContextReporter.disable();

        // We want to refresh partner browser provider every onStart().
        mPartnerBrowserRefreshNeeded = true;
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        getChromeApplication().onStartWithNative();
        Tab tab = getActivityTab();
        if (tab != null) tab.onActivityStart();
        FeatureUtilities.setDocumentModeEnabled(FeatureUtilities.isDocumentMode(this));
        WarmupManager.getInstance().clearWebContentsIfNecessary();

        if (GSAState.getInstance(this).isGsaAvailable()) {
            mGSAServiceClient = new GSAServiceClient(this);
            mGSAServiceClient.connect();
            createContextReporterIfNeeded();
        } else {
            ContextReporter.reportStatus(ContextReporter.STATUS_GSA_NOT_AVAILABLE);
        }
    }

    private void createContextReporterIfNeeded() {
        if (mContextReporter != null || getActivityTab() == null) return;

        final ProfileSyncService syncService = ProfileSyncService.get(this);

        if (syncService.isSyncingUrlsWithKeystorePassphrase()) {
            mContextReporter = ((ChromeMobileApplication) getApplicationContext()).createGsaHelper()
                    .getContextReporter(this);

            if (mSyncStateChangedListener != null) {
                syncService.removeSyncStateChangedListener(mSyncStateChangedListener);
                mSyncStateChangedListener = null;
            }

            return;
        } else {
            ContextReporter.reportSyncStatus(syncService);
        }

        if (mSyncStateChangedListener == null) {
            mSyncStateChangedListener = new ProfileSyncService.SyncStateChangedListener() {
                @Override
                public void syncStateChanged() {
                    createContextReporterIfNeeded();
                }
            };
            syncService.addSyncStateChangedListener(mSyncStateChangedListener);
        }
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        markSessionResume();

        if (getActivityTab() != null) {
            LaunchMetrics.commitLaunchMetrics(getActivityTab().getWebContents());
        }
    }

    @Override
    public void onPauseWithNative() {
        markSessionEnd();
        super.onPauseWithNative();
    }

    @Override
    public void onStopWithNative() {
        if (mGSAServiceClient != null) {
            mGSAServiceClient.disconnect();
            mGSAServiceClient = null;
            if (mSyncStateChangedListener != null) {
                ProfileSyncService syncService = ProfileSyncService.get(this);
                syncService.removeSyncStateChangedListener(mSyncStateChangedListener);
                mSyncStateChangedListener = null;
            }
        }

        super.onStopWithNative();
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        super.onNewIntentWithNative(intent);
        if (mIntentHandler.shouldIgnoreIntent(this, intent)) return;

        mIntentHandler.onNewIntent(this, intent);
    }

    /**
     * Called when the orientation of the device changes.  The orientation is checked/detected on
     * root view layouts.
     * @param orientation One of {@link Surface#ROTATION_0} (no rotation),
     *                    {@link Surface#ROTATION_90}, {@link Surface#ROTATION_180}, or
     *                    {@link Surface#ROTATION_270}.
     */
    protected void onOrientationChange(int orientation) {
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
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item != null && onMenuOrKeyboardAction(item.getItemId(), true)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onUserInteraction() {
        mLastUserInteractionTime = SystemClock.elapsedRealtime();
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
        return false;
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     *
     * @param currentTab The {@link Tab} a user is watching.
     * @param windowAndroid The {@link WindowAndroid} currentTab is linked to.
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    public void onShareMenuItemSelected(final Tab currentTab,
            final WindowAndroid windowAndroid, final boolean shareDirectly, boolean isIncognito) {
        if (currentTab == null) return;

        final Activity mainActivity = this;
        ContentReadbackHandler.GetBitmapCallback bitmapCallback =
                new ContentReadbackHandler.GetBitmapCallback() {
                    @Override
                    public void onFinishGetBitmap(Bitmap bitmap, int reponse) {
                        ShareHelper.share(shareDirectly, mainActivity, currentTab.getTitle(),
                                currentTab.getUrl(), bitmap);
                        if (shareDirectly) {
                            RecordUserAction.record("MobileMenuDirectShare");
                        } else {
                            RecordUserAction.record("MobileMenuShare");
                        }
                    }
                };
        ContentReadbackHandler readbackHandler = getContentReadbackHandler();
        if (isIncognito || readbackHandler == null || windowAndroid == null
                || currentTab.getContentViewCore() == null) {
            bitmapCallback.onFinishGetBitmap(null, ReadbackResponse.SURFACE_UNAVAILABLE);
        } else {
            readbackHandler.getContentBitmapAsync(1, new Rect(), currentTab.getContentViewCore(),
                    Bitmap.Config.ARGB_8888, bitmapCallback);
        }
    }

    /**
     * @return Whether the activity is running in tablet mode.
     */
    public boolean isTablet() {
        return mIsTablet;
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
        return false;
    }

    /**
     * Allows Activities that extend ChromeActivity to do additional hiding/showing of menu items.
     * @param menu Menu that is going to be shown when the menu button is pressed.
     */
    public void prepareMenu(Menu menu) {
    }

    /**
     * @return timestamp when the last user interaction was made.
     */
    public long getLastUserInteractionTime() {
        return mLastUserInteractionTime;
    }

    protected IntentHandlerDelegate createIntentHandlerDelegate() {
        return new IntentHandlerDelegate() {
            @Override
            public void processWebSearchIntent(String query) {
                Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
                searchIntent.putExtra(SearchManager.QUERY, query);
                startActivity(searchIntent);
            }

            @Override
            public void processUrlViewIntent(String url, String referer, String headers,
                    TabOpenType tabOpenType, String externalAppId, int tabIdToBringToFront,
                    Intent intent) {
            }
        };
    }

    /**
     * @return The resource id that contains how large the top controls are.
     */
    protected int getControlContainerHeightResource() {
        return R.dimen.control_container_height;
    }

    private void markSessionResume() {
        // Start new session for UMA.
        if (mUmaSessionStats == null) {
            mUmaSessionStats = new UmaSessionStats(this);
        }

        mUmaSessionStats.updateMetricsServiceState();
        // In DocumentMode we need the application-level TabModelSelector instead of per
        // activity which only manages a single tab.
        if (FeatureUtilities.isDocumentMode(this)) {
            mUmaSessionStats.startNewSession(
                    ChromeMobileApplication.getDocumentTabModelSelector());
        } else {
            mUmaSessionStats.startNewSession(getTabModelSelector());
        }
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

    @Override
    public final void onAccessibilityStateChanged(boolean enabled) {
        checkAccessibility();
    }

    private void checkAccessibility() {
        onAccessibilityModeChanged(DeviceClassManager.isAccessibilityModeEnabled(this));
    }

    private void checkOrientation() {
        WindowManager wm = getWindowManager();
        if (wm == null) return;

        Display display = wm.getDefaultDisplay();
        if (display == null) return;

        int oldOrientation = mCurrentOrientation;
        mCurrentOrientation = display.getRotation();

        if (oldOrientation != mCurrentOrientation) onOrientationChange(mCurrentOrientation);
    }

    /**
     * Removes the window background.
     */
    protected void removeWindowBackground() {
        boolean removeWindowBackground = true;
        try {
            Field field = Settings.Secure.class.getField(
                    "ACCESSIBILITY_DISPLAY_MAGNIFICATION_ENABLED");
            field.setAccessible(true);

            if (field.getType() == String.class) {
                String accessibilityMagnificationSetting = (String) field.get(null);
                // When Accessibility magnification is turned on, setting a null window
                // background causes the overlaid android views to stretch when panning.
                // (crbug/332994)
                if (Settings.Secure.getInt(
                        getContentResolver(), accessibilityMagnificationSetting) == 1) {
                    removeWindowBackground = false;
                }
            }
        } catch (SettingNotFoundException e) {
            // Window background is removed if an exception occurs.
        } catch (NoSuchFieldException e) {
            // Window background is removed if an exception occurs.
        } catch (IllegalAccessException e) {
            // Window background is removed if an exception occurs.
        } catch (IllegalArgumentException e) {
            // Window background is removed if an exception occurs.
        }
        if (removeWindowBackground) getWindow().setBackgroundDrawable(null);
    }

    /**
     * @return A casted version of {@link #getApplication()}.
     */
    public ChromeMobileApplication getChromeApplication() {
        return (ChromeMobileApplication) getApplication();
    }

    /**
     * @return Whether the update infobar may be shown.
     */
    public boolean mayShowUpdateInfoBar() {
        return true;
    }

    /**
     * Actions that may be run at some point after startup. Place tasks that are not critical to the
     * startup path here.  This method will be called automatically and should not be called
     * directly by subclasses.  Overriding methods should call super.onDeferredStartup().
     */
    protected void onDeferredStartup() {
        boolean crashDumpUploadingDisabled = getIntent() != null
                && getIntent().hasExtra(
                        ChromeTabbedActivity.INTENT_EXTRA_DISABLE_CRASH_DUMP_UPLOADING);
        DeferredStartupHandler.getInstance()
                .onDeferredStartup(getChromeApplication(), crashDumpUploadingDisabled);

        BeamController.registerForBeam(this, new BeamProvider() {
            @Override
            public String getTabUrlForBeam() {
                if (isOverlayVisible()) return null;
                if (getActivityTab() == null) return null;
                return getActivityTab().getUrl();
            }
        });

        getChromeApplication().getUpdateInfoBarHelper().checkForUpdateOnBackgroundThread(this);

        removeSnapshotDatabase();
    }

    private void postDeferredStartupIfNeeded() {
        if (!mDeferredStartupNotified) {
            // We want to perform deferred startup tasks a short time after the first page
            // load completes, but only when the main thread Looper has become idle.
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    if (!mDeferredStartupNotified && !isActivityDestroyed()) {
                        mDeferredStartupNotified = true;
                        Looper.myQueue().addIdleHandler(new MessageQueue.IdleHandler() {
                            @Override
                            public boolean queueIdle() {
                                onDeferredStartup();
                                return false;  // Remove this idle handler.
                            }
                        });
                    }
                }
            }, DEFERRED_STARTUP_DELAY_MS);
        }
    }

    private void showUpdateInfoBarIfNecessary() {
        getChromeApplication().getUpdateInfoBarHelper().showUpdateInfobarIfNecessary(this);
    }

    /**
     * Determines whether the ContentView is currently visible and not hidden by an overlay
     * @return true if the ContentView is fully hidden by another view (i.e. the tab stack)
     */
    public boolean isOverlayVisible() {
        return false;
    }

    /**
     * Deletes the snapshot database which is no longer used because the feature has been removed
     * in Chrome M41.
     */
    private void removeSnapshotDatabase() {
        // Temporarily allowing disk access. TODO: Fix. See http://crbug.com/493181
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        StrictMode.allowThreadDiskWrites();
        try {
            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
            if (!prefs.getBoolean(SNAPSHOT_DATABASE_REMOVED, false)) {
                deleteDatabase(SNAPSHOT_DATABASE_NAME);
                prefs.edit().putBoolean(SNAPSHOT_DATABASE_REMOVED, true).apply();
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public void terminateIncognitoSession() {}

    /**
     * @return The {@code ContextualSearchManager} or {@code null} if none;
     */
    public ContextualSearchManager getContextualSearchManager() {
        return null;
    }
}
