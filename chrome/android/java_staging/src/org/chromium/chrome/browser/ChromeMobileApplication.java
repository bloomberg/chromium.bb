// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.util.Log;
import android.view.View;

import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.PathUtils;
import org.chromium.base.ResourceExtractor;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.banners.AppDetailsDelegate;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.document.IncognitoDocumentActivity;
import org.chromium.chrome.browser.document.TabDelegateImpl;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.firstrun.FirstRunActivityStaging;
import org.chromium.chrome.browser.gsa.GSAHelper;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.invalidation.UniqueIdInvalidationClientNameGenerator;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.metrics.VariationsSession;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omaha.RequestGenerator;
import org.chromium.chrome.browser.omaha.UpdateInfoBarHelper;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyManager;
import org.chromium.chrome.browser.policy.PolicyManager.PolicyChangeListener;
import org.chromium.chrome.browser.policy.providers.AppRestrictionsProvider;
import org.chromium.chrome.browser.preferences.AccessibilityPreferences;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesStaging;
import org.chromium.chrome.browser.printing.PrintingControllerFactory;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.services.GoogleServicesManager;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.signin.AccountManagementFragmentDelegateImpl;
import org.chromium.chrome.browser.smartcard.EmptyPKCS11AuthenticationManager;
import org.chromium.chrome.browser.smartcard.PKCS11AuthenticationManager;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.browser.tab.AuthenticatorNavigationInterceptor;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.StorageDelegate;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.browser.ContentViewStatics;
import org.chromium.content.browser.DownloadController;
import org.chromium.printing.PrintingController;
import org.chromium.ui.UiUtils;

import java.util.Locale;

/**
 * Per-process class that manages classes and functions shared by all of Chrome's Activities.
 */
public class ChromeMobileApplication extends ChromiumApplication {
    private static final String PREF_LOCALE = "locale";
    private static final float FLOAT_EPSILON = 0.001f;

    private static DocumentTabModelSelector sDocumentTabModelSelector;

    /**
     * This class allows pausing scripts & network connections when we
     * go to the background and resume when we are back in foreground again.
     * TODO(pliard): Get rid of this class once JavaScript timers toggling is done directly on
     * the native side by subscribing to the system monitor events.
     */
    private static class BackgroundProcessing {
        private class SuspendRunnable implements Runnable {
            @Override
            public void run() {
                mSuspendRunnable = null;
                assert !mWebKitTimersAreSuspended;
                mWebKitTimersAreSuspended = true;
                ContentViewStatics.setWebKitSharedTimersSuspended(true);
            }
        }

        private static final int SUSPEND_TIMERS_AFTER_MS = 5 * 60 * 1000;
        private final Handler mHandler = new Handler();
        private boolean mWebKitTimersAreSuspended = false;
        private SuspendRunnable mSuspendRunnable;

        private void onDestroy() {
            if (mSuspendRunnable != null) {
                mHandler.removeCallbacks(mSuspendRunnable);
                mSuspendRunnable = null;
            }
        }

        private void suspendTimers() {
            if (mSuspendRunnable == null) {
                mSuspendRunnable = new SuspendRunnable();
                mHandler.postDelayed(mSuspendRunnable, SUSPEND_TIMERS_AFTER_MS);
            }
        }

        private void startTimers() {
            if (mSuspendRunnable != null) {
                mHandler.removeCallbacks(mSuspendRunnable);
                mSuspendRunnable = null;
            } else if (mWebKitTimersAreSuspended) {
                ContentViewStatics.setWebKitSharedTimersSuspended(false);
                mWebKitTimersAreSuspended = false;
            }
        }
    }

    private static final String[] CHROME_MANDATORY_PAKS = {
        "resources.pak",
        "chrome_100_percent.pak",
    };
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";
    private static final String DEV_TOOLS_SERVER_SOCKET_PREFIX = "chrome";
    private static final String SESSIONS_UUID_PREF_KEY = "chromium.sync.sessions.id";

    private final BackgroundProcessing mBackgroundProcessing = new BackgroundProcessing();
    private final PowerBroadcastReceiver mPowerBroadcastReceiver = new PowerBroadcastReceiver();
    private final UpdateInfoBarHelper mUpdateInfoBarHelper = new UpdateInfoBarHelper();

    // Used to trigger variation changes (such as seed fetches) upon application foregrounding.
    private VariationsSession mVariationsSession;

    private DevToolsServer mDevToolsServer;

    private boolean mIsStarted;
    private boolean mInitializedSharedClasses;
    private boolean mIsProcessInitialized;

    private ChromeLifetimeController mChromeLifetimeController;
    private PrintingController mPrintingController;
    private PolicyManager mPolicyManager = new PolicyManager();

    /**
     * This is called once per ChromeMobileApplication instance, which get created per process
     * (browser OR renderer).  Don't stick anything in here that shouldn't be called multiple times
     * during Chrome's lifetime.
     */
    @Override
    public void onCreate() {
        UmaUtils.recordMainEntryPointTime();
        super.onCreate();
        UiUtils.setKeyboardShowingDelegate(new UiUtils.KeyboardShowingDelegate() {
            @Override
            public boolean disableKeyboardCheck(Context context, View view) {
                Activity activity = null;
                if (context instanceof Activity) {
                    activity = (Activity) context;
                } else if (view != null && view.getContext() instanceof Activity) {
                    activity = (Activity) view.getContext();
                }

                // For multiwindow mode we do not track keyboard visibility.
                return activity != null && isMultiWindow(activity);
            }
        });

        // Set the unique identification generator for invalidations.  The
        // invalidations system can start and attempt to fetch the client ID
        // very early.  We need this generator to be ready before that happens.
        UniqueIdInvalidationClientNameGenerator.doInitializeAndInstallGenerator(this);

        // Set minimum Tango log level. This sets an in-memory static field, and needs to be
        // set in the ApplicationContext instead of an activity, since Tango can be woken up
        // by the system directly though messages from GCM.
        AndroidLogger.setMinimumAndroidLogLevel(Log.WARN);

        // Set up the identification generator for sync. The ID is actually generated
        // in the SyncController constructor.
        UniqueIdentificationGeneratorFactory.registerGenerator(SyncController.GENERATOR_ID,
                new UuidBasedUniqueIdentificationGenerator(this, SESSIONS_UUID_PREF_KEY), false);

        AccountManagementFragmentDelegateImpl.registerAccountManagementFragmentDelegate();
    }

    @Override
    protected void initializeLibraryDependencies() {
        // The ResourceExtractor is only needed by the browser process, but this will have no
        // impact on the renderer process construction.
        ResourceExtractor.setMandatoryPaksToExtract(R.array.locale_paks, CHROME_MANDATORY_PAKS);
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, this);
    }

    @Override
    protected void initializeGoogleServicesManager() {
        GoogleServicesManager.get(getApplicationContext());
    }

    @Override
    public void initCommandLine() {
        ChromeCommandLineInitUtil.initChromeCommandLine(this);
    }

    /**
     * The host activity should call this after the native library has loaded to ensure classes
     * shared by Activities in the same process are properly initialized.
     */
    public void initializeSharedClasses() {
        if (mInitializedSharedClasses) return;
        mInitializedSharedClasses = true;

        GoogleServicesManager.get(this).onMainActivityStart();
        SyncController.get(this).onMainActivityStart();
        RevenueStats.getInstance();
        ShortcutHelper.setFullScreenAction(ChromeLauncherActivity.ACTION_START_WEBAPP);

        getPKCS11AuthenticationManager().initialize(ChromeMobileApplication.this);

        mDevToolsServer = new DevToolsServer(DEV_TOOLS_SERVER_SOCKET_PREFIX);
        mDevToolsServer.setRemoteDebuggingEnabled(
                true, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);

        startApplicationActivityTracker();

        DownloadController.setDownloadNotificationService(
                DownloadManagerService.getDownloadManagerService(this));

        if (ApiCompatibilityUtils.isPrintingSupported()) {
            mPrintingController = PrintingControllerFactory.create(getApplicationContext());
        }
    }

    /**
     * @return The Application's PowerBroadcastReceiver.
     */
    @VisibleForTesting
    public PowerBroadcastReceiver getPowerBroadcastReceiver() {
        return mPowerBroadcastReceiver;
    }

    /**
     * Update the font size after changing the Android accessibility system setting.  Doing so kills
     * the Activities but it doesn't kill the ChromeMobileApplication, so this should be called in
     * {@link #onStart} instead of {@link #initialize}.
     */
    private void updateFontSize() {
        // This method is currently broken. http://crbug.com/439108
        // Skip it (with the consequence of not updating the text scaling factor when the user
        // changes system font size) rather than incurring the broken behavior.
        // TODO(newt): fix this.
        if (true) return;

        FontSizePrefs fontSizePrefs = FontSizePrefs.getInstance(getApplicationContext());

        // Set font scale factor as the product of the system and browser scale settings.
        float browserTextScale = PreferenceManager
                .getDefaultSharedPreferences(this)
                .getFloat(AccessibilityPreferences.PREF_TEXT_SCALE, 1.0f);
        float fontScale = getResources().getConfiguration().fontScale * browserTextScale;

        float scaleDelta = Math.abs(fontScale - fontSizePrefs.getFontScaleFactor());
        if (scaleDelta >= FLOAT_EPSILON) {
            fontSizePrefs.setFontScaleFactor(fontScale);
        }

        // If force enable zoom has not been manually set, set it automatically based on
        // font scale factor.
        boolean shouldForceZoom =
                fontScale >= AccessibilityPreferences.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER;
        if (!fontSizePrefs.getUserSetForceEnableZoom()
                && fontSizePrefs.getForceEnableZoom() != shouldForceZoom) {
            fontSizePrefs.setForceEnableZoom(shouldForceZoom);
        }
    }

    /**
     * Update the accept languages after changing Android locale setting. Doing so kills the
     * Activities but it doesn't kill the ChromeMobileApplication, so this should be called in
     * {@link #onStart} instead of {@link #initialize}.
     */
    private void updateAcceptLanguages() {
        PrefServiceBridge instance = PrefServiceBridge.getInstance();
        String localeString = Locale.getDefault().toString();  // ex) en_US, de_DE, zh_CN_#Hans
        if (hasLocaleChanged(localeString)) {
            instance.resetAcceptLanguages(localeString);
            // Clear cache so that accept-languages change can be applied immediately.
            // TODO(changwan): The underlying BrowsingDataRemover::Remove() is an asynchronous call.
            // So cache-clearing may not be effective if URL rendering can happen before
            // OnBrowsingDataRemoverDone() is called, in which case we may have to reload as well.
            // Check if it can happen.
            instance.clearBrowsingData(null, false, true /* cache */, false, false, false);
        }
    }

    private boolean hasLocaleChanged(String newLocale) {
        String previousLocale = PreferenceManager.getDefaultSharedPreferences(this).getString(
                PREF_LOCALE, "");

        if (!previousLocale.equals(newLocale)) {
            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
            SharedPreferences.Editor editor = prefs.edit();
            editor.putString(PREF_LOCALE, newLocale);
            editor.apply();
            return true;
        }
        return false;
    }

    /**
     * Should be called almost immediately after the native library has loaded to initialize things
     * that really, really have to be set up early.  Avoid putting any long tasks here.
     */
    @Override
    public void initializeProcess() {
        if (mIsProcessInitialized) return;
        mIsProcessInitialized = true;
        assert !mIsStarted;

        super.initializeProcess();

        mVariationsSession = createVariationsSession();
        removeSessionCookies();
        ApplicationStatus.registerApplicationStateListener(createApplicationStateListener());
        AppBannerManager.setAppDetailsDelegate(createAppDetailsDelegate());
        mChromeLifetimeController = new ChromeLifetimeController(this);

        mPolicyManager.initializeNative();
        registerPolicyProviders(mPolicyManager);

        PrefServiceBridge.getInstance().migratePreferences(this);
    }

    /**
     * Each top-level activity (ChromeTabbedActivity, FullscreenActivity) should call this during
     * its onStart phase. When called for the first time, this marks the beginning of a foreground
     * session and calls onForegroundSessionStart(). Subsequent calls are noops until
     * onForegroundSessionEnd() is called, to handle changing top-level Chrome activities in one
     * foreground session.
     */
    public void onStartWithNative() {
        if (mIsStarted) return;
        mIsStarted = true;

        assert mIsProcessInitialized;

        onForegroundSessionStart();
    }

    /**
     * Called when a top-level Chrome activity (ChromeTabbedActivity, FullscreenActivity) is
     * started in foreground. It will not be called again when other Chrome activities take over
     * (see onStart()), that is, when correct activity calls startActivity() for another Chrome
     * activity.
     */
    private void onForegroundSessionStart() {
        ChildProcessLauncher.onBroughtToForeground();
        mBackgroundProcessing.startTimers();
        updatePasswordEchoState();
        updateFontSize();
        updateAcceptLanguages();
        changeAppStatus(true);
        mVariationsSession.start(getApplicationContext());

        mPowerBroadcastReceiver.registerReceiver(this);
        mPowerBroadcastReceiver.runActions(this, true);
    }

    /**
     * Called when last of Chrome activities is stopped, ending the foreground session. This will
     * not be called when a Chrome activity is stopped because another Chrome activity takes over.
     * This is ensured by ActivityStatus, which switches to track new activity when its started and
     * will not report the old one being stopped (see createStateListener() below).
     */
    private void onForegroundSessionEnd() {
        if (!mIsStarted) return;
        mBackgroundProcessing.suspendTimers();
        flushPersistentData();
        mIsStarted = false;
        changeAppStatus(false);

        try {
            mPowerBroadcastReceiver.unregisterReceiver(this);
        } catch (IllegalArgumentException e) {
            // This may happen when onStop get called very early in UI test.
        }

        ChildProcessLauncher.onSentToBackground();
        IntentHandler.clearPendingReferrer();
    }

    /**
     * Called after onForegroundSessionEnd() indicating that the activity whose onStop() ended the
     * last foreground session was destroyed.
     */
    private void onForegroundActivityDestroyed() {
        if (ApplicationStatus.isEveryActivityDestroyed()) {
            mBackgroundProcessing.onDestroy();
            stopApplicationActivityTracker();
            PartnerBrowserCustomizations.destroy();
            ShareHelper.clearSharedScreenshots(this);
            mPolicyManager.destroy();
            mPolicyManager = null;
        }
    }

    private ApplicationStateListener createApplicationStateListener() {
        return new ApplicationStateListener() {
            @Override
            public void onApplicationStateChange(int newState) {
                if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
                    onForegroundSessionEnd();
                } else if (newState == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                    onForegroundActivityDestroyed();
                }
            }
        };
    }

    /**
     * Returns a new instance of VariationsSession.
     */
    public VariationsSession createVariationsSession() {
        return new VariationsSession();
    }

    /**
     * Return a {@link AuthenticatorNavigationInterceptor} for the given {@link Tab}.
     * This can be null if there are no applicable interceptor to be built.
     */
    @SuppressWarnings("unused")
    public AuthenticatorNavigationInterceptor createAuthenticatorNavigationInterceptor(Tab tab) {
        return null;
    }

    /**
     * Starts the application activity tracker.
     */
    protected void startApplicationActivityTracker() {}

    /**
     * Stops the application activity tracker.
     */
    protected void stopApplicationActivityTracker() {}

    // TODO(newt): delete this method after upstreaming. Callers can use
    // MultiWindowUtils.getInstance() instead.
    @Override
    public boolean isMultiWindow(Activity activity) {
        return MultiWindowUtils.getInstance().isMultiWindow(activity);
    }

    // TODO(newt): delete this after upstreaming.
    @Override
    public String getSettingsActivityName() {
        return PreferencesStaging.class.getName();
    }

    // TODO(aurimas): delete this after upstreaming.
    @Override
    public String getFirstRunActivityName() {
        return FirstRunActivityStaging.class.getName();
    }

    /**
     * Honor the Android system setting about showing the last character of a password for a short
     * period of time.
     */
    private void updatePasswordEchoState() {
        boolean systemEnabled = Settings.System.getInt(
                getApplicationContext().getContentResolver(),
                Settings.System.TEXT_SHOW_PASSWORD, 1) == 1;
        if (PrefServiceBridge.getInstance().getPasswordEchoEnabled() == systemEnabled) return;

        PrefServiceBridge.getInstance().setPasswordEchoEnabled(systemEnabled);
    }

    @Override
    protected PKCS11AuthenticationManager getPKCS11AuthenticationManager() {
        return EmptyPKCS11AuthenticationManager.getInstance();
    }

    /**
     * @return Instance of printing controller that is shared among all chromium activities. May
     *         return null if printing is not supported on the platform.
     */
    public PrintingController getPrintingController() {
        return mPrintingController;
    }

    /**
     * @return The UpdateInfoBarHelper used to inform the user about updates.
     */
    public UpdateInfoBarHelper getUpdateInfoBarHelper() {
        return mUpdateInfoBarHelper;
    }

    /**
     * @return An instance of {@link GSAHelper} that handles the start point of chrome's integration
     *         with GSA.
     */
    public GSAHelper createGsaHelper() {
        return new GSAHelper();
    }

    @VisibleForTesting
    public PolicyManager getPolicyManagerForTesting() {
        return mPolicyManager;
    }

    /**
     * Registers various policy providers with the policy manager.
     * Providers are registered in increasing order of precedence so overrides should call this
     * method in the end for this method to maintain the highest precedence.
     * @param manager The {@link PolicyManager} to register the providers with.
     */
    protected void registerPolicyProviders(PolicyManager manager) {
        manager.registerProvider(new AppRestrictionsProvider(getApplicationContext()));
    }

    /**
     * Add a listener to be notified upon policy changes.
     */
    public void addPolicyChangeListener(PolicyChangeListener listener) {
        mPolicyManager.addPolicyChangeListener(listener);
    }

    /**
     * Remove a listener to be notified upon policy changes.
     */
    public void removePolicyChangeListener(PolicyChangeListener listener) {
        mPolicyManager.removePolicyChangeListener(listener);
    }

    /**
     * @return An instance of PolicyAuditor that notifies the policy system of the user's activity.
     * Only applicable when the user has a policy active, that is tracking the activity.
     */
    public PolicyAuditor getPolicyAuditor() {
        // This class has a protected constructor to prevent accidental instantiation.
        return new PolicyAuditor() {};
    }

    /**
     * @return An instance of MultiWindowUtils to be installed as a singleton.
     */
    public MultiWindowUtils createMultiWindowUtils() {
        return new MultiWindowUtils();
    }

    /**
     * @return An instance of RequestGenerator to be used for Omaha XML creation.  Will be null if
     *         a generator is unavailable.
     */
    public RequestGenerator createOmahaRequestGenerator() {
        return null;
    }

    /**
     * @return An instance of AppDetailsDelegate that can be queried about app information for the
     *         App Banner feature.  Will be null if one is unavailable.
     */
    protected AppDetailsDelegate createAppDetailsDelegate() {
        return null;
    }

    /**
     * Returns the Singleton instance of the DocumentTabModelSelector.
     * TODO(dfalcantara): Find a better place for this once we differentiate between activity and
     *                    application-level TabModelSelectors.
     * @return The DocumentTabModelSelector for the application.
     */
    @SuppressFBWarnings("LI_LAZY_INIT_STATIC")
    public static DocumentTabModelSelector getDocumentTabModelSelector() {
        ThreadUtils.assertOnUiThread();
        if (sDocumentTabModelSelector == null) {
            sDocumentTabModelSelector = new DocumentTabModelSelector(
                    new ActivityDelegate(DocumentActivity.class, IncognitoDocumentActivity.class),
                    new StorageDelegate(), new TabDelegateImpl(false), new TabDelegateImpl(true));
        }
        return sDocumentTabModelSelector;
    }

    /**
     * @return Whether or not the Singleton has been initialized.
     */
    @VisibleForTesting
    public static boolean isDocumentTabModelSelectorInitializedForTests() {
        return sDocumentTabModelSelector != null;
    }

    /**
     * @return An instance of RevenueStats to be installed as a singleton.
     */
    public RevenueStats createRevenueStatsInstance() {
        return new RevenueStats();
    }
}
