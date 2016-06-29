// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.SystemClock;
import android.support.annotation.UiThread;
import android.support.annotation.WorkerThread;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.PowerMonitor;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.bookmarkswidget.BookmarkWidgetProvider;
import org.chromium.chrome.browser.crash.CrashFileManager;
import org.chromium.chrome.browser.crash.MinidumpUploadService;
import org.chromium.chrome.browser.media.MediaCaptureNotificationService;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.physicalweb.PhysicalWeb;
import org.chromium.chrome.browser.precache.PrecacheLauncher;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.webapps.WebApkVersionManager;
import org.chromium.content.browser.ChildProcessLauncher;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * Handler for application level tasks to be completed on deferred startup.
 */
public class DeferredStartupHandler {
    /** Prevents race conditions when deleting snapshot database. */
    private static final Object SNAPSHOT_DATABASE_LOCK = new Object();
    private static final String SNAPSHOT_DATABASE_REMOVED = "snapshot_database_removed";
    private static final String SNAPSHOT_DATABASE_NAME = "snapshots.db";

    private static class Holder {
        private static final DeferredStartupHandler INSTANCE = new DeferredStartupHandler();
    }

    private boolean mDeferredStartupComplete;
    private final Context mAppContext;

    /**
     * This class is an application specific object that handles the deferred startup.
     * @return The singleton instance of {@link DeferredStartupHandler}.
     */
    public static DeferredStartupHandler getInstance() {
        return Holder.INSTANCE;
    }

    private DeferredStartupHandler() {
        mAppContext = ContextUtils.getApplicationContext();
    }

    /**
     * Handle application level deferred startup tasks that can be lazily done after all
     * the necessary initialization has been completed. Any calls requiring network access should
     * probably go here.
     */
    @UiThread
    public void onDeferredStartupForApp() {
        if (mDeferredStartupComplete) return;
        ThreadUtils.assertOnUiThread();

        long startDeferredStartupTime = SystemClock.uptimeMillis();

        RecordHistogram.recordLongTimesHistogram("UMA.Debug.EnableCrashUpload.DeferredStartUptime",
                startDeferredStartupTime - UmaUtils.getMainEntryPointTime(),
                TimeUnit.MILLISECONDS);

        // Punt all tasks that may block the UI thread off onto a background thread.
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                try {
                    TraceEvent.begin("ChromeBrowserInitializer.onDeferredStartup.doInBackground");
                    long asyncTaskStartTime = SystemClock.uptimeMillis();
                    boolean crashDumpDisabled = CommandLine.getInstance().hasSwitch(
                            ChromeSwitches.DISABLE_CRASH_DUMP_UPLOAD);
                    if (!crashDumpDisabled) {
                        RecordHistogram.recordLongTimesHistogram(
                                "UMA.Debug.EnableCrashUpload.Uptime2",
                                asyncTaskStartTime - UmaUtils.getMainEntryPointTime(),
                                TimeUnit.MILLISECONDS);
                        PrivacyPreferencesManager.getInstance().enablePotentialCrashUploading();
                        MinidumpUploadService.tryUploadAllCrashDumps(mAppContext);
                    }
                    CrashFileManager crashFileManager =
                            new CrashFileManager(mAppContext.getCacheDir());
                    crashFileManager.cleanOutAllNonFreshMinidumpFiles();

                    MinidumpUploadService.storeBreakpadUploadStatsInUma(
                            ChromePreferenceManager.getInstance(mAppContext));

                    // Force a widget refresh in order to wake up any possible zombie widgets.
                    // This is needed to ensure the right behavior when the process is suddenly
                    // killed.
                    BookmarkWidgetProvider.refreshAllWidgets(mAppContext);

                    // Initialize whether or not precaching is enabled.
                    PrecacheLauncher.updatePrecachingEnabled(mAppContext);

                    if (CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_WEBAPK)) {
                        WebApkVersionManager.updateWebApksIfNeeded();
                    }

                    removeSnapshotDatabase();

                    cacheIsChromeDefaultBrowser();

                    RecordHistogram.recordLongTimesHistogram(
                            "UMA.Debug.EnableCrashUpload.DeferredStartUpDurationAsync",
                            SystemClock.uptimeMillis() - asyncTaskStartTime,
                            TimeUnit.MILLISECONDS);

                    return null;
                } finally {
                    TraceEvent.end("ChromeBrowserInitializer.onDeferredStartup.doInBackground");
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        AfterStartupTaskUtils.setStartupComplete();

        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(new Runnable() {
            @Override
            public void run() {
                String homepageUrl = HomepageManager.getHomepageUri(mAppContext);
                LaunchMetrics.recordHomePageLaunchMetrics(
                        HomepageManager.isHomepageEnabled(mAppContext),
                        NewTabPage.isNTPUrl(homepageUrl), homepageUrl);
            }
        });

        // TODO(aruslan): http://b/6397072 This will be moved elsewhere
        PartnerBookmarksShim.kickOffReading(mAppContext);

        PowerMonitor.create(mAppContext);

        ShareHelper.clearSharedImages(mAppContext);

        // Clear any media notifications that existed when Chrome was last killed.
        MediaCaptureNotificationService.clearMediaNotifications(mAppContext);

        startModerateBindingManagementIfNeeded();

        recordKeyboardLocaleUma();

        ChromeApplication application = (ChromeApplication) mAppContext;
        // Starts syncing with GSA.
        application.createGsaHelper().startSync();

        application.initializeSharedClasses();

        // Start or stop Physical Web
        PhysicalWeb.onChromeStart(application);

        mDeferredStartupComplete = true;

        RecordHistogram.recordLongTimesHistogram(
                "UMA.Debug.EnableCrashUpload.DeferredStartUpDuration",
                SystemClock.uptimeMillis() - startDeferredStartupTime,
                TimeUnit.MILLISECONDS);
    }

    private void startModerateBindingManagementIfNeeded() {
        // Moderate binding doesn't apply to low end devices.
        if (SysUtils.isLowEndDevice()) return;

        boolean moderateBindingTillBackgrounded =
                FieldTrialList.findFullName("ModerateBindingOnBackgroundTabCreation")
                        .equals("Enabled");
        ChildProcessLauncher.startModerateBindingManagement(
                mAppContext, moderateBindingTillBackgrounded);
    }

    /**
     * Caches whether Chrome is set as a default browser on the device.
     */
    @WorkerThread
    private void cacheIsChromeDefaultBrowser() {
        // Retrieve whether Chrome is default in background to avoid strict mode checks.
        Intent intent = new Intent(Intent.ACTION_VIEW,
                Uri.parse("http://www.madeupdomainforcheck123.com/"));
        ResolveInfo info = mAppContext.getPackageManager().resolveActivity(intent, 0);
        boolean isDefault = (info != null && info.match != 0
                && mAppContext.getPackageName().equals(info.activityInfo.packageName));
        ChromePreferenceManager.getInstance(mAppContext).setCachedChromeDefaultBrowser(isDefault);
    }

    /**
     * Deletes the snapshot database which is no longer used because the feature has been removed
     * in Chrome M41.
     */
    @WorkerThread
    private void removeSnapshotDatabase() {
        synchronized (SNAPSHOT_DATABASE_LOCK) {
            SharedPreferences prefs =
                    ContextUtils.getAppSharedPreferences();
            if (!prefs.getBoolean(SNAPSHOT_DATABASE_REMOVED, false)) {
                mAppContext.deleteDatabase(SNAPSHOT_DATABASE_NAME);
                prefs.edit().putBoolean(SNAPSHOT_DATABASE_REMOVED, true).apply();
            }
        }
    }

    private void recordKeyboardLocaleUma() {
        InputMethodManager imm =
                (InputMethodManager) mAppContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        List<InputMethodInfo> ims = imm.getEnabledInputMethodList();
        ArrayList<String> uniqueLanguages = new ArrayList<String>();
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

    /**
    * @return Whether deferred startup has been completed.
    */
    @VisibleForTesting
    public boolean isDeferredStartupComplete() {
        return mDeferredStartupComplete;
    }
}
