// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.SystemClock;
import android.support.annotation.WorkerThread;
import android.view.View;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;

import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PowerMonitor;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AfterStartupTaskUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeBackupAgent;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.DevToolsServer;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.bookmarkswidget.BookmarkWidgetProvider;
import org.chromium.chrome.browser.crash.LogcatExtractionRunnable;
import org.chromium.chrome.browser.crash.MinidumpUploadService;
import org.chromium.chrome.browser.download.DownloadController;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.firstrun.ForcedSigninProcessor;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.invalidation.UniqueIdInvalidationClientNameGenerator;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.media.MediaCaptureNotificationService;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.notifications.channels.ChannelsUpdater;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.photo_picker.PhotoPickerDialog;
import org.chromium.chrome.browser.physicalweb.PhysicalWeb;
import org.chromium.chrome.browser.precache.PrecacheLauncher;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.chrome.browser.services.AccountsChangedReceiver;
import org.chromium.chrome.browser.services.GoogleServicesManager;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.chrome.browser.webapps.WebApkVersionManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.signin.AccountManagerHelper;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderFactory;
import org.chromium.printing.PrintDocumentAdapterWrapper;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.PhotoPickerListener;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.SelectFileDialog;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * Handles the initialization dependences of the browser process.  This is meant to handle the
 * initialization that is not tied to any particular Activity, and the logic that should only be
 * triggered a single time for the lifetime of the browser process.
 */
public class ProcessInitializationHandler {
    private static final String TAG = "ProcessInitHandler";

    private static final String SESSIONS_UUID_PREF_KEY = "chromium.sync.sessions.id";
    private static final String DEV_TOOLS_SERVER_SOCKET_PREFIX = "chrome";

    /** Prevents race conditions when deleting snapshot database. */
    private static final Object SNAPSHOT_DATABASE_LOCK = new Object();
    private static final String SNAPSHOT_DATABASE_REMOVED = "snapshot_database_removed";
    private static final String SNAPSHOT_DATABASE_NAME = "snapshots.db";

    private static ProcessInitializationHandler sInstance;

    private boolean mInitializedPreNative;
    private boolean mInitializedPostNative;
    private boolean mInitializedDeferredStartupTasks;
    private DevToolsServer mDevToolsServer;

    /**
     * @return The ProcessInitializationHandler for use during the lifetime of the browser process.
     */
    @SuppressFBWarnings("LI_LAZY_INIT_STATIC")
    public static ProcessInitializationHandler getInstance() {
        ThreadUtils.checkUiThread();
        if (sInstance == null) {
            sInstance = AppHooks.get().createProcessInitializationHandler();
        }
        return sInstance;
    }

    /**
     * Initializes the any dependencies that must occur before native library has been loaded.
     * <p>
     * Adding anything expensive to this must be avoided as it would delay the Chrome startup path.
     * <p>
     * All entry points that do not rely on {@link ChromeBrowserInitializer} must call this on
     * startup.
     */
    public final void initializePreNative() {
        ThreadUtils.checkUiThread();
        if (mInitializedPreNative) return;
        handlePreNativeInitialization();
        mInitializedPreNative = true;
    }

    /**
     * Performs the shared class initialization.
     */
    protected void handlePreNativeInitialization() {
        ChromeApplication application = (ChromeApplication) ContextUtils.getApplicationContext();

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
                return activity != null
                        && MultiWindowUtils.getInstance().isLegacyMultiWindow(activity);
            }
        });

        // Initialize the AccountManagerHelper with the correct AccountManagerDelegate. Must be done
        // only once and before AccountMangerHelper.get(...) is called to avoid using the
        // default AccountManagerDelegate.
        AccountManagerHelper.initializeAccountManagerHelper(
                AppHooks.get().createAccountManagerDelegate());

        // Set the unique identification generator for invalidations.  The
        // invalidations system can start and attempt to fetch the client ID
        // very early.  We need this generator to be ready before that happens.
        UniqueIdInvalidationClientNameGenerator.doInitializeAndInstallGenerator(application);

        // Set minimum Tango log level. This sets an in-memory static field, and needs to be
        // set in the ApplicationContext instead of an activity, since Tango can be woken up
        // by the system directly though messages from GCM.
        AndroidLogger.setMinimumAndroidLogLevel(Log.WARN);

        // Set up the identification generator for sync. The ID is actually generated
        // in the SyncController constructor.
        UniqueIdentificationGeneratorFactory.registerGenerator(SyncController.GENERATOR_ID,
                new UuidBasedUniqueIdentificationGenerator(
                        application, SESSIONS_UUID_PREF_KEY), false);

        // Indicate that we can use the GMS location provider.
        LocationProviderFactory.useGmsCoreLocationProvider();
    }

    /**
     * Initializes any dependencies that must occur after the native library has been loaded.
     */
    public final void initializePostNative() {
        ThreadUtils.checkUiThread();
        if (mInitializedPostNative) return;
        handlePostNativeInitialization();
        mInitializedPostNative = true;
    }

    /**
     * Performs the post native initialization.
     */
    protected void handlePostNativeInitialization() {
        final ChromeApplication application =
                (ChromeApplication) ContextUtils.getApplicationContext();

        DataReductionProxySettings.reconcileDataReductionProxyEnabledState(application);
        ChromeActivitySessionTracker.getInstance().initializeWithNative();
        ChromeApplication.removeSessionCookies();
        AppBannerManager.setAppDetailsDelegate(AppHooks.get().createAppDetailsDelegate());
        ChromeLifetimeController.initialize();

        PrefServiceBridge.getInstance().migratePreferences(application);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.NEW_PHOTO_PICKER)) {
            UiUtils.setPhotoPickerDelegate(new UiUtils.PhotoPickerDelegate() {
                private PhotoPickerDialog mDialog;

                @Override
                public void showPhotoPicker(
                        Context context, PhotoPickerListener listener, boolean allowMultiple) {
                    mDialog = new PhotoPickerDialog(context, listener, allowMultiple);
                    mDialog.show();
                }

                @Override
                public void dismissPhotoPicker() {
                    mDialog.dismiss();
                    mDialog = null;
                }
            });
        }

        SearchWidgetProvider.initialize();
    }

    /**
     * Handle application level deferred startup tasks that can be lazily done after all
     * the necessary initialization has been completed. Should only be triggered once per browser
     * process lifetime. Any calls requiring network access should probably go here.
     *
     * Keep these tasks short and break up long tasks into multiple smaller tasks, as they run on
     * the UI thread and are blocking. Remember to follow RAIL guidelines, as much as possible, and
     * that most devices are quite slow, so leave enough buffer.
     */
    public final void initializeDeferredStartupTasks() {
        ThreadUtils.checkUiThread();
        if (mInitializedDeferredStartupTasks) return;
        mInitializedDeferredStartupTasks = true;

        RecordHistogram.recordLongTimesHistogram("UMA.Debug.EnableCrashUpload.DeferredStartUptime2",
                SystemClock.uptimeMillis() - UmaUtils.getForegroundStartTime(),
                TimeUnit.MILLISECONDS);

        handleDeferredStartupTasksInitialization();
    }

    /**
     * Performs the deferred startup task initialization.
     */
    protected void handleDeferredStartupTasksInitialization() {
        final ChromeApplication application =
                (ChromeApplication) ContextUtils.getApplicationContext();
        DeferredStartupHandler deferredStartupHandler = DeferredStartupHandler.getInstance();

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                // Punt all tasks that may block on disk off onto a background thread.
                initAsyncDiskTask(application);

                DefaultBrowserInfo.initBrowserFetcher();

                AfterStartupTaskUtils.setStartupComplete();

                PartnerBrowserCustomizations.setOnInitializeAsyncFinished(new Runnable() {
                    @Override
                    public void run() {
                        String homepageUrl = HomepageManager.getHomepageUri(application);
                        LaunchMetrics.recordHomePageLaunchMetrics(
                                HomepageManager.isHomepageEnabled(application),
                                NewTabPage.isNTPUrl(homepageUrl), homepageUrl);
                    }
                });

                PartnerBookmarksShim.kickOffReading(application);

                PowerMonitor.create();

                ShareHelper.clearSharedImages();

                OfflinePageUtils.clearSharedOfflineFiles(application);

                SelectFileDialog.clearCapturedCameraFiles();

                if (ChannelsUpdater.getInstance().shouldUpdateChannels()) {
                    initChannelsAsync();
                }
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                // Clear any media notifications that existed when Chrome was last killed.
                MediaCaptureNotificationService.clearMediaNotifications(application);

                startModerateBindingManagementIfNeeded(application);

                recordKeyboardLocaleUma(application);
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                // Start or stop Physical Web
                PhysicalWeb.onChromeStart();
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                LocaleManager.getInstance().recordStartupMetrics();
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                // Starts syncing with GSA.
                AppHooks.get().createGsaHelper().startSync();
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                // Record the saved restore state in a histogram
                ChromeBackupAgent.recordRestoreHistogram();
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                ForcedSigninProcessor.start(application, null);
                AccountsChangedReceiver.addObserver(
                        new AccountsChangedReceiver.AccountsChangedObserver() {
                            @Override
                            public void onAccountsChanged() {
                                ThreadUtils.runOnUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        ForcedSigninProcessor.start(application, null);
                                    }
                                });
                            }
                        });
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                GoogleServicesManager.get(application).onMainActivityStart();
                RevenueStats.getInstance();
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                mDevToolsServer = new DevToolsServer(DEV_TOOLS_SERVER_SOCKET_PREFIX);
                mDevToolsServer.setRemoteDebuggingEnabled(
                        true, DevToolsServer.Security.ALLOW_DEBUG_PERMISSION);
            }
        });

        deferredStartupHandler.addDeferredTask(new Runnable() {
            @Override
            public void run() {
                // Add process check to diagnose http://crbug.com/606309. Remove this after the bug
                // is fixed.
                assert !CommandLine.getInstance().hasSwitch(ContentSwitches.SWITCH_PROCESS_TYPE);
                if (!CommandLine.getInstance().hasSwitch(ContentSwitches.SWITCH_PROCESS_TYPE)) {
                    DownloadController.setDownloadNotificationService(
                            DownloadManagerService.getDownloadManagerService());
                }

                if (ApiCompatibilityUtils.isPrintingSupported()) {
                    String errorText = application.getResources().getString(
                            R.string.error_printing_failed);
                    PrintingControllerImpl.create(new PrintDocumentAdapterWrapper(), errorText);
                }
            }
        });
    }

    private void initChannelsAsync() {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                ChannelsUpdater.getInstance().updateChannels();
                return null;
            }
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private void initAsyncDiskTask(final Context context) {
        new AsyncTask<Void, Void, Void>() {
            /**
             * The threshold after which it's no longer appropriate to try to attach logcat output
             * to a minidump file.
             * Note: This threshold of 12 hours was chosen fairly imprecisely, based on the
             * following intuition: On the one hand, Chrome can only access its own logcat output,
             * so the most recent lines should be relevant when available. On a typical device,
             * multiple hours of logcat output are available. On the other hand, it's important to
             * provide an escape hatch in case the logcat extraction code itself crashes, as
             * described in the doesCrashMinidumpNeedLogcat() documentation. Since this is a fairly
             * small and relatively frequently-executed piece of code, crashes are expected to be
             * unlikely; so it's okay for the escape hatch to be hard to use -- it's intended as an
             * extreme last resort.
             */
            private static final long LOGCAT_RELEVANCE_THRESHOLD_IN_HOURS = 12;

            private long mAsyncTaskStartTime;

            @Override
            protected Void doInBackground(Void... params) {
                try {
                    TraceEvent.begin("ChromeBrowserInitializer.onDeferredStartup.doInBackground");
                    mAsyncTaskStartTime = SystemClock.uptimeMillis();

                    initCrashReporting();

                    // Initialize the WebappRegistry if it's not already initialized. Must be in
                    // async task due to shared preferences disk access on N.
                    WebappRegistry.getInstance();

                    // Force a widget refresh in order to wake up any possible zombie widgets.
                    // This is needed to ensure the right behavior when the process is suddenly
                    // killed.
                    BookmarkWidgetProvider.refreshAllWidgets(context);

                    // Initialize whether or not precaching is enabled.
                    PrecacheLauncher.updatePrecachingEnabled(context);

                    if (ChromeWebApkHost.isEnabled()) {
                        WebApkVersionManager.updateWebApksIfNeeded();
                    }

                    removeSnapshotDatabase(context);

                    // Warm up all web app shared prefs. This must be run after the WebappRegistry
                    // instance is initialized.
                    WebappRegistry.warmUpSharedPrefs();

                    return null;
                } finally {
                    TraceEvent.end("ChromeBrowserInitializer.onDeferredStartup.doInBackground");
                }
            }

            @Override
            protected void onPostExecute(Void params) {
                // Must be run on the UI thread after the WebappRegistry has been completely warmed.
                WebappRegistry.getInstance().unregisterOldWebapps(System.currentTimeMillis());

                RecordHistogram.recordLongTimesHistogram(
                        "UMA.Debug.EnableCrashUpload.DeferredStartUpAsyncTaskDuration",
                        SystemClock.uptimeMillis() - mAsyncTaskStartTime, TimeUnit.MILLISECONDS);
            }

            /**
             * Initializes the crash reporting system. More specifically, enables the crash
             * reporting system if it is user-permitted, and initiates uploading of any pending
             * crash reports. Also updates some UMA metrics and performs cleanup in the local crash
             * minidump storage directory.
             */
            private void initCrashReporting() {
                RecordHistogram.recordLongTimesHistogram("UMA.Debug.EnableCrashUpload.Uptime3",
                        mAsyncTaskStartTime - UmaUtils.getForegroundStartTime(),
                        TimeUnit.MILLISECONDS);

                // Crash reports can be uploaded as part of a background service even while the main
                // Chrome activity is not running, and hence regular metrics reporting is not
                // possible. Instead, metrics are temporarily written to prefs; export those prefs
                // to UMA metrics here.
                MinidumpUploadService.storeBreakpadUploadStatsInUma(
                        ChromePreferenceManager.getInstance());

                // Likewise, this is a good time to process and clean up any pending or stale crash
                // reports left behind by previous runs.
                CrashFileManager crashFileManager =
                        new CrashFileManager(ContextUtils.getApplicationContext().getCacheDir());
                crashFileManager.cleanOutAllNonFreshMinidumpFiles();

                // Finally, uploading any pending crash reports.
                File[] minidumps = crashFileManager.getAllMinidumpFiles(
                        MinidumpUploadService.MAX_TRIES_ALLOWED);
                int numMinidumpsSansLogcat = 0;
                for (File minidump : minidumps) {
                    if (CrashFileManager.isMinidumpMIMEFirstTry(minidump.getName())) {
                        ++numMinidumpsSansLogcat;
                    }
                }
                // TODO(isherman): These two histograms are intended to be temporary, and can
                // probably be removed around the M60 timeframe: http://crbug.com/699785
                RecordHistogram.recordSparseSlowlyHistogram(
                        "Stability.Android.PendingMinidumpsOnStartup", minidumps.length);
                RecordHistogram.recordSparseSlowlyHistogram(
                        "Stability.Android.PendingMinidumpsOnStartup.SansLogcat",
                        numMinidumpsSansLogcat);
                if (minidumps.length == 0) return;

                Log.i(TAG, "Attempting to upload %d accumulated crash dumps.", minidumps.length);
                File mostRecentMinidump = minidumps[0];
                if (doesCrashMinidumpNeedLogcat(mostRecentMinidump)) {
                    AsyncTask.THREAD_POOL_EXECUTOR.execute(
                            new LogcatExtractionRunnable(mostRecentMinidump));

                    // The JobScheduler will schedule uploads for all of the available minidumps
                    // once the logcat is attached. But if the JobScheduler API is not being used,
                    // then the logcat extraction process will only initiate an upload for the first
                    // minidump; it's required to manually initiate uploads for all of the remaining
                    // minidumps.
                    if (!MinidumpUploadService.shouldUseJobSchedulerForUploads()) {
                        List<File> remainingMinidumps =
                                Arrays.asList(minidumps).subList(1, minidumps.length);
                        for (File minidump : remainingMinidumps) {
                            MinidumpUploadService.tryUploadCrashDump(minidump);
                        }
                    }
                } else if (MinidumpUploadService.shouldUseJobSchedulerForUploads()) {
                    MinidumpUploadService.scheduleUploadJob();
                } else {
                    MinidumpUploadService.tryUploadAllCrashDumps();
                }
            }

            /**
             * Returns whether or not it's appropriate to try to extract recent logcat output and
             * include that logcat output alongside the given {@param minidump} in a crash report.
             * Logcat output should only be extracted if (a) it hasn't already been extracted for
             * this minidump file, and (b) the minidump is fairly fresh. The freshness check is
             * important for two reasons: (1) First of all, it helps avoid including irrelevant
             * logcat output for a crash report. (2) Secondly, it provides an escape hatch that can
             * help circumvent a possible infinite crash loop, if the code responsible for
             * extracting and appending the logcat content is itself crashing. That is, the user can
             * wait 12 hours prior to relaunching Chrome, at which point this potential crash loop
             * would be circumvented.
             * @return Whether to try to include logcat output in the crash report corresponding to
             *     the given minidump.
             */
            private boolean doesCrashMinidumpNeedLogcat(File minidump) {
                if (!CrashFileManager.isMinidumpMIMEFirstTry(minidump.getName())) return false;

                long ageInMillis = new Date().getTime() - minidump.lastModified();
                long ageInHours = TimeUnit.HOURS.convert(ageInMillis, TimeUnit.MILLISECONDS);
                return ageInHours < LOGCAT_RELEVANCE_THRESHOLD_IN_HOURS;
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Deletes the snapshot database which is no longer used because the feature has been removed
     * in Chrome M41.
     */
    @WorkerThread
    private void removeSnapshotDatabase(Context context) {
        synchronized (SNAPSHOT_DATABASE_LOCK) {
            SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
            if (!prefs.getBoolean(SNAPSHOT_DATABASE_REMOVED, false)) {
                context.deleteDatabase(SNAPSHOT_DATABASE_NAME);
                prefs.edit().putBoolean(SNAPSHOT_DATABASE_REMOVED, true).apply();
            }
        }
    }

    private void startModerateBindingManagementIfNeeded(Context context) {
        // Moderate binding doesn't apply to low end devices.
        if (SysUtils.isLowEndDevice()) return;
        ChildProcessLauncher.startModerateBindingManagement(context);
    }

    @SuppressWarnings("deprecation") // InputMethodSubtype.getLocale() deprecated in API 24
    private void recordKeyboardLocaleUma(Context context) {
        InputMethodManager imm =
                (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
        List<InputMethodInfo> ims = imm.getEnabledInputMethodList();
        ArrayList<String> uniqueLanguages = new ArrayList<>();
        for (InputMethodInfo method : ims) {
            List<InputMethodSubtype> submethods =
                    imm.getEnabledInputMethodSubtypeList(method, true);
            for (InputMethodSubtype submethod : submethods) {
                if (submethod.getMode().equals("keyboard")) {
                    String language = submethod.getLocale().split("_")[0];
                    if (!uniqueLanguages.contains(language)) {
                        uniqueLanguages.add(language);
                    }
                }
            }
        }
        RecordHistogram.recordCountHistogram("InputMethod.ActiveCount", uniqueLanguages.size());

        InputMethodSubtype currentSubtype = imm.getCurrentInputMethodSubtype();
        Locale systemLocale = Locale.getDefault();
        if (currentSubtype != null && currentSubtype.getLocale() != null && systemLocale != null) {
            String keyboardLanguage = currentSubtype.getLocale().split("_")[0];
            boolean match = systemLocale.getLanguage().equalsIgnoreCase(keyboardLanguage);
            RecordHistogram.recordBooleanHistogram("InputMethod.MatchesSystemLanguage", match);
        }
    }
}
