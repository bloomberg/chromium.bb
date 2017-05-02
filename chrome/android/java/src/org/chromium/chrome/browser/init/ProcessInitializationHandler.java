// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;

import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeBackupAgent;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.DevToolsServer;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.download.DownloadController;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.firstrun.ForcedSigninProcessor;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.invalidation.UniqueIdInvalidationClientNameGenerator;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.media.MediaCaptureNotificationService;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.photo_picker.PhotoPickerDialog;
import org.chromium.chrome.browser.physicalweb.PhysicalWeb;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.searchwidget.SearchWidgetProvider;
import org.chromium.chrome.browser.services.AccountsChangedReceiver;
import org.chromium.chrome.browser.services.GoogleServicesManager;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.components.signin.AccountManagerHelper;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderFactory;
import org.chromium.printing.PrintDocumentAdapterWrapper;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.PhotoPickerListener;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Handles the initialization dependences of the browser process.  This is meant to handle the
 * initialization that is not tied to any particular Activity, and the logic that should only be
 * triggered a single time for the lifetime of the browser process.
 */
public class ProcessInitializationHandler {

    private static final String SESSIONS_UUID_PREF_KEY = "chromium.sync.sessions.id";
    private static final String DEV_TOOLS_SERVER_SOCKET_PREFIX = "chrome";

    private static ProcessInitializationHandler sInstance;

    private boolean mInitializedPreNative;
    private boolean mInitializedPostNative;
    private boolean mInitializedDeferredStartupTasks;
    private DevToolsServer mDevToolsServer;

    /**
     * @return The ProcessInitializationHandler for use during the lifetime of the browser process.
     */
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
     * Initializes the deferred startup tasks that should only be triggered once per browser process
     * lifetime.
     */
    public final void initializeDeferredStartupTasks() {
        ThreadUtils.checkUiThread();
        if (mInitializedDeferredStartupTasks) return;
        handleDeferredStartupTasksInitialization();
        mInitializedDeferredStartupTasks = true;
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
