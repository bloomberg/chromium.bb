// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentActivity;
import android.util.Log;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.child_accounts.ChildAccountService;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.ProtectedContentPreferences;
import org.chromium.chrome.browser.preferences.autofill.AutofillPreferences;
import org.chromium.chrome.browser.preferences.password.ManageSavedPasswordsPreferences;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferences;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.services.AndroidEduOwnerCheckCallback;
import org.chromium.content.app.ContentApplication;
import org.chromium.content.browser.BrowserStartupController;

import java.util.concurrent.Callable;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 */
public abstract class ChromiumApplication extends ContentApplication {

    private static final String TAG = "ChromiumApplication";

    /**
     * Returns whether the Activity is being shown in multi-window mode.
     */
    public boolean isMultiWindow(Activity activity) {
        return false;
    }

    /**
     * Initiate AndroidEdu device check.
     * @param callback Callback that should receive the results of the AndroidEdu device check.
     */
    public void checkIsAndroidEduDevice(AndroidEduOwnerCheckCallback callback) {
    }

    /**
     * Returns the class name of the Settings activity.
     */
    public abstract String getSettingsActivityName();

    /**
     * Returns the class name of the FirstRun activity.
     */
    public String getFirstRunActivityName() {
        return FirstRunActivity.class.getName();
    }

    /**
     * Open Chrome Sync settings page.
     * @param accountName the name of the account that is being synced.
     */
    public void openSyncSettings(String accountName) {
        // TODO(aurimas): implement this once SyncCustomizationFragment is upstreamed.
    }

    /**
     * Opens a protected content settings page, if available.
     */
    @CalledByNative
    protected void openProtectedContentSettings() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT;
        PreferencesLauncher.launchSettingsPage(this,
                ProtectedContentPreferences.class.getName());
    }

    @CalledByNative
    protected void showAutofillSettings() {
        PreferencesLauncher.launchSettingsPage(this,
                AutofillPreferences.class.getName());
    }

    @CalledByNative
    protected void showPasswordSettings() {
        PreferencesLauncher.launchSettingsPage(this,
                ManageSavedPasswordsPreferences.class.getName());
    }

    /**
     * Opens the single origin settings page for the given URL.
     *
     * @param url The URL to show the single origin settings for. This is a complete url
     *            including scheme, domain, port, path, etc.
     */
    protected void showSingleOriginSettings(String url) {
        Bundle fragmentArgs = SingleWebsitePreferences.createFragmentArgsForSite(url);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                this, SingleWebsitePreferences.class.getName());
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        startActivity(intent);
    }

    /**
     * For extending classes to carry out tasks that initialize the browser process.
     * Should be called almost immediately after the native library has loaded to initialize things
     * that really, really have to be set up early.  Avoid putting any long tasks here.
     */
    public void initializeProcess() {
        DataReductionProxySettings.initialize(getApplicationContext());
    }

    @Override
    public void initCommandLine() {
        ChromeCommandLineInitUtil.initChromeCommandLine(this);
    }

    /**
     * Start the browser process asynchronously. This will set up a queue of UI
     * thread tasks to initialize the browser process.
     *
     * Note that this can only be called on the UI thread.
     *
     * @param callback the callback to be called when browser startup is complete.
     * @throws ProcessInitException
     */
    public void startChromeBrowserProcessesAsync(BrowserStartupController.StartupCallback callback)
            throws ProcessInitException {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread";
        Context applicationContext = getApplicationContext();
        BrowserStartupController.get(applicationContext, LibraryProcessType.PROCESS_BROWSER)
                .startBrowserProcessesAsync(callback);
    }

    /**
     * Loads native Libraries synchronously and starts Chrome browser processes.
     * Must be called on the main thread.
     *
     * @param initGoogleServicesManager when true the GoogleServicesManager is initialized.
     */
    public void startBrowserProcessesAndLoadLibrariesSync(
            Context context, boolean initGoogleServicesManager)
            throws ProcessInitException {
        ThreadUtils.assertOnUiThread();
        initCommandLine();
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized(this, true);
        startChromeBrowserProcessesSync(initGoogleServicesManager);
    }

    /**
     * Make sure the process is initialized as Browser process instead of
     * ContentView process. If this is not called from the main thread, an event
     * will be posted and return will be blocked waiting for that event to
     * complete.
     * @param initGoogleServicesManager when true the GoogleServicesManager is initialized.
     */
    public void startChromeBrowserProcessesSync(final boolean initGoogleServicesManager)
            throws ProcessInitException {
        final Context context = getApplicationContext();
        int loadError = ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() {
                try {
                    // Kick off checking for a child account with an empty callback.
                    ChildAccountService childAccountService =
                            ChildAccountService.getInstance(context);
                    childAccountService.checkHasChildAccount(
                            new ChildAccountService.HasChildAccountCallback() {
                                @Override
                                public void onChildAccountChecked(boolean hasChildAccount) {
                                }
                            });
                    BrowserStartupController.get(context, LibraryProcessType.PROCESS_BROWSER)
                            .startBrowserProcessesSync(false);
                    if (initGoogleServicesManager) initializeGoogleServicesManager();
                    // Wait until ChildAccountManager finishes its check.
                    childAccountService.waitUntilFinished();
                    return LoaderErrors.LOADER_ERROR_NORMAL_COMPLETION;
                } catch (ProcessInitException e) {
                    Log.e(TAG, "Unable to load native library.", e);
                    return e.getErrorCode();
                }
            }
        });
        if (loadError != LoaderErrors.LOADER_ERROR_NORMAL_COMPLETION) {
            throw new ProcessInitException(loadError);
        }
    }

    /**
     * Shows an error dialog following a startup error, and then exits the application.
     * @param e The exception reported by Chrome initialization.
     */
    public static void reportStartupErrorAndExit(final ProcessInitException e) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (ApplicationStatus.getStateForActivity(activity) == ActivityState.DESTROYED
                || !(activity instanceof FragmentActivity)) {
            return;
        }
        int errorCode = e.getErrorCode();
        int msg;
        switch (errorCode) {
            case LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_LOAD_FAILED:
                msg = R.string.os_version_missing_features;
                break;
            case LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_WRONG_VERSION:
                msg = R.string.incompatible_libraries;
                break;
            default:
                msg = R.string.native_startup_failed;
        }
        final String message = activity.getResources().getString(msg);

        DialogFragment dialog = new DialogFragment() {
            @Override
            public Dialog onCreateDialog(Bundle savedInstanceState) {
                AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(getActivity());
                dialogBuilder
                        .setMessage(message)
                        .setCancelable(true)
                        .setPositiveButton(getResources().getString(android.R.string.ok),
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        System.exit(-1);
                                    }
                                })
                        .setOnCancelListener(new DialogInterface.OnCancelListener() {
                            @Override
                            public void onCancel(DialogInterface dialog) {
                                System.exit(-1);
                            }
                        });
                return dialogBuilder.create();
            }
        };
        dialog.show(
                ((FragmentActivity) activity).getSupportFragmentManager(), "InvalidStartupDialog");
    }

    /**
     * For extending classes to override and initialize GoogleServicesManager
     */
    protected void initializeGoogleServicesManager() {
        // TODO(yusufo): Make this private when GoogleServicesManager is upstreamed.
    }

    /**
     * Returns an instance of LocationSettings to be installed as a singleton.
     */
    public LocationSettings createLocationSettings() {
        // Using an anonymous subclass as the constructor is protected.
        // This is done to deter instantiation of LocationSettings elsewhere without using the
        // getInstance() helper method.
        return new LocationSettings(this){};
    }

    /**
     * Opens the UI to clear browsing data.
     * @param tab The tab that triggered the request.
     */
    @CalledByNative
    protected void openClearBrowsingData(Tab tab) {
        Activity activity = tab.getWindowAndroid().getActivity().get();
        if (activity == null) {
            Log.e(TAG,
                    "Attempting to open clear browsing data for a tab without a valid activity");
            return;
        }
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(activity,
                PrivacyPreferences.class.getName());
        Bundle arguments = new Bundle();
        arguments.putBoolean(PrivacyPreferences.SHOW_CLEAR_BROWSING_DATA_EXTRA, true);
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, arguments);
        activity.startActivity(intent);
    }

    /**
     * @return Whether parental controls are enabled.  Returning true will disable
     *         incognito mode.
     */
    @CalledByNative
    protected boolean areParentalControlsEnabled() {
        return PartnerBrowserCustomizations.isIncognitoDisabled();
    }

    // TODO(yfriedman): This is too widely available. Plumb this through ChromeNetworkDelegate
    // instead.
    protected abstract PKCS11AuthenticationManager getPKCS11AuthenticationManager();

    /**
     * @return The user agent string of Chrome.
     */
    public static String getBrowserUserAgent() {
        return nativeGetBrowserUserAgent();
    }

    /**
     * The host activity should call this during its onPause() handler to ensure
     * all state is saved when the app is suspended.  Calling ChromiumApplication.onStop() does
     * this for you.
     */
    public static void flushPersistentData() {
        try {
            TraceEvent.begin("ChromiumApplication.flushPersistentData");
            nativeFlushPersistentData();
        } finally {
            TraceEvent.end("ChromiumApplication.flushPersistentData");
        }
    }

    private static native String nativeGetBrowserUserAgent();
    private static native void nativeFlushPersistentData();
}
