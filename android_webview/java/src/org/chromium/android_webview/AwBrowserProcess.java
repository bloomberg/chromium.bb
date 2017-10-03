// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.AsyncTask;
import android.os.Build;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.StrictMode;

import org.chromium.android_webview.command_line.CommandLineUtil;
import org.chromium.android_webview.crash.CrashReceiverService;
import org.chromium.android_webview.crash.ICrashReceiverService;
import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.ChildProcessCreationParams;
import org.chromium.content.browser.ChildProcessLauncherHelper;
import org.chromium.policy.CombinedPolicyProvider;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;

/**
 * Wrapper for the steps needed to initialize the java and native sides of webview chromium.
 */
@JNINamespace("android_webview")
public final class AwBrowserProcess {
    public static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "webview";

    private static final String TAG = "AwBrowserProcess";
    private static final String EXCLUSIVE_LOCK_FILE = "webview_data.lock";
    private static RandomAccessFile sLockFile;
    private static FileLock sExclusiveFileLock;
    private static String sWebViewPackageName;

    /**
     * Loads the native library, and performs basic static construction of objects needed
     * to run webview in this process. Does not create threads; safe to call from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     */
    public static void loadLibrary() {
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            LibraryLoader libraryLoader = LibraryLoader.get(LibraryProcessType.PROCESS_WEBVIEW);
            libraryLoader.loadNow();
            // Switch the command line implementation from Java to native.
            // It's okay for the WebView to do this before initialization because we have
            // setup the JNI bindings by this point.
            libraryLoader.switchCommandLineForWebView();
        } catch (ProcessInitException e) {
            throw new RuntimeException("Cannot load WebView", e);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Configures child process launcher. This is required only if child services are used in
     * WebView.
     */
    public static void configureChildProcessLauncher(String packageName,
            boolean isExternalService) {
        final boolean bindToCaller = true;
        final boolean ignoreVisibilityForImportance = true;
        ChildProcessCreationParams.registerDefault(new ChildProcessCreationParams(packageName,
                isExternalService, LibraryProcessType.PROCESS_WEBVIEW_CHILD, bindToCaller,
                ignoreVisibilityForImportance));
    }

    /**
     * Starts the chromium browser process running within this process. Creates threads
     * and performs other per-app resource allocations; must not be called from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     */
    public static void start() {
        final Context appContext = ContextUtils.getApplicationContext();
        tryObtainingDataDirLock(appContext);
        // We must post to the UI thread to cover the case that the user
        // has invoked Chromium startup by using the (thread-safe)
        // CookieManager rather than creating a WebView.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            boolean multiProcess = CommandLine.getInstance().hasSwitch(
                    AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
            if (multiProcess) {
                ChildProcessLauncherHelper.warmUp(appContext);
            }
            // The policies are used by browser startup, so we need to register the policy
            // providers before starting the browser process. This only registers java objects
            // and doesn't need the native library.
            CombinedPolicyProvider.get().registerProvider(new AwPolicyProvider(appContext));

            // Check android settings but only when safebrowsing is enabled.
            AwSafeBrowsingConfigHelper.maybeInitSafeBrowsingFromSettings(appContext);

            try {
                BrowserStartupController.get(LibraryProcessType.PROCESS_WEBVIEW)
                        .startBrowserProcessesSync(!multiProcess);
            } catch (ProcessInitException e) {
                throw new RuntimeException("Cannot initialize WebView", e);
            }
        });

        // Only run cleanup task on N+ since on earlier versions there are no extra pak files.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            // Cleanup task to remove unnecessary extra pak files (crbug.com/752510).
            // TODO(zpeng): Remove cleanup code after at least M64 (crbug.com/756580).
            AsyncTask.THREAD_POOL_EXECUTOR.execute(() -> {
                File extraPaksDir = new File(PathUtils.getDataDirectory(), "paks");
                if (extraPaksDir.exists()) {
                    for (File pakFile: extraPaksDir.listFiles()) {
                        pakFile.delete();
                    }
                    extraPaksDir.delete();
                }
            });
        }
    }

    private static void tryObtainingDataDirLock(Context context) {
        // Too many apps rely on this at present to make this fatal,
        // even though it's known to be unsafe.
        boolean dieOnFailure = false;

        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            String dataPath = PathUtils.getDataDirectory();
            File lockFile = new File(dataPath, EXCLUSIVE_LOCK_FILE);
            boolean success = false;
            try {
                // Note that the file is kept open intentionally.
                sLockFile = new RandomAccessFile(lockFile, "rw");
                sExclusiveFileLock = sLockFile.getChannel().tryLock();
                success = sExclusiveFileLock != null;
            } catch (IOException e) {
                Log.w(TAG, "Failed to create lock file " + lockFile, e);
            }
            if (!success) {
                final String error = "Using WebView from more than one process at once in a single "
                        + "app is not supported. https://crbug.com/558377";
                if (dieOnFailure) {
                    throw new RuntimeException(error);
                } else {
                    Log.w(TAG, error);
                }
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    public static void setWebViewPackageName(String webViewPackageName) {
        assert sWebViewPackageName == null || sWebViewPackageName.equals(webViewPackageName);
        sWebViewPackageName = webViewPackageName;
    }

    public static String getWebViewPackageName() {
        if (sWebViewPackageName == null) return ""; // May be null in testing.
        return sWebViewPackageName;
    }

    /**
     * Trigger minidump copying, which in turn triggers minidump uploading.
     */
    @CalledByNative
    private static void triggerMinidumpUploading() {
        handleMinidumpsAndSetMetricsConsent(sWebViewPackageName, false /* updateMetricsConsent */);
    }

    /**
     * Trigger minidump uploading, and optionaly also update the metrics-consent value depending on
     * whether the Android Checkbox is toggled on.
     * @param updateMetricsConsent whether to update the metrics-consent value to represent the
     * Android Checkbox toggle.
     */
    public static void handleMinidumpsAndSetMetricsConsent(
            final String webViewPackageName, final boolean updateMetricsConsent) {
        final boolean enableMinidumpUploadingForTesting = CommandLine.getInstance().hasSwitch(
                CommandLineUtil.CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH);
        if (enableMinidumpUploadingForTesting) {
            AwBrowserProcess.handleMinidumps(webViewPackageName, true /* enabled */);
        }

        PlatformServiceBridge.getInstance().queryMetricsSetting(enabled -> {
            ThreadUtils.assertOnUiThread();
            if (updateMetricsConsent) {
                AwMetricsServiceClient.setConsentSetting(
                        ContextUtils.getApplicationContext(), enabled);
            }

            if (!enableMinidumpUploadingForTesting) {
                AwBrowserProcess.handleMinidumps(webViewPackageName, enabled);
            }
        });
    }

    /**
     * Pass Minidumps to a separate Service declared in the WebView provider package.
     * That Service will copy the Minidumps to its own data directory - at which point we can delete
     * our copies in the app directory.
     * @param userApproved whether we have user consent to upload crash data - if we do, copy the
     * minidumps, if we don't, delete them.
     */
    public static void handleMinidumps(
            final String webViewPackageName, final boolean userApproved) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                final Context appContext = ContextUtils.getApplicationContext();
                final File crashSpoolDir = new File(appContext.getCacheDir().getPath(), "WebView");
                if (!crashSpoolDir.isDirectory()) return null;
                final CrashFileManager crashFileManager = new CrashFileManager(crashSpoolDir);

                // The lifecycle of a minidump in the app directory is very simple: foo.dmpNNNNN --
                // where NNNNN is a Process ID (PID) -- gets created, and is either deleted or
                // copied over to the shared crash directory for all WebView-using apps.
                final File[] minidumpFiles = crashFileManager.getMinidumpsSansLogcat();
                if (minidumpFiles.length == 0) return null;

                // Delete the minidumps if the user doesn't allow crash data uploading.
                if (!userApproved) {
                    for (File minidump : minidumpFiles) {
                        if (!minidump.delete()) {
                            Log.w(TAG, "Couldn't delete file " + minidump.getAbsolutePath());
                        }
                    }
                    return null;
                }

                final Intent intent = new Intent();
                intent.setClassName(webViewPackageName, CrashReceiverService.class.getName());

                ServiceConnection connection = new ServiceConnection() {
                    @Override
                    public void onServiceConnected(ComponentName className, IBinder service) {
                        // Pass file descriptors, pointing to our minidumps, to the minidump-copying
                        // service so that the contents of the minidumps will be copied to WebView's
                        // data directory. Delete our direct File-references to the minidumps after
                        // creating the file-descriptors to resign from retrying to copy the
                        // minidumps if anything goes wrong - this makes sense given that a failure
                        // to copy a file usually means that retrying won't succeed either, e.g. the
                        // disk being full, or the file system being corrupted.
                        final ParcelFileDescriptor[] minidumpFds =
                                new ParcelFileDescriptor[minidumpFiles.length];
                        try {
                            for (int i = 0; i < minidumpFiles.length; ++i) {
                                try {
                                    minidumpFds[i] = ParcelFileDescriptor.open(
                                            minidumpFiles[i], ParcelFileDescriptor.MODE_READ_ONLY);
                                } catch (FileNotFoundException e) {
                                    minidumpFds[i] = null; // This is slightly ugly :)
                                }
                                if (!minidumpFiles[i].delete()) {
                                    Log.w(TAG, "Couldn't delete file "
                                            + minidumpFiles[i].getAbsolutePath());
                                }
                            }
                            try {
                                ICrashReceiverService.Stub.asInterface(service).transmitCrashes(
                                        minidumpFds);
                            } catch (RemoteException e) {
                                // TODO(gsennton): add a UMA metric here to ensure we aren't losing
                                // too many minidumps because of this.
                            }
                        } finally {
                            // Close FDs
                            for (int i = 0; i < minidumpFds.length; ++i) {
                                try {
                                    if (minidumpFds[i] != null) minidumpFds[i].close();
                                } catch (IOException e) {
                                }
                            }
                            appContext.unbindService(this);
                        }
                    }

                    @Override
                    public void onServiceDisconnected(ComponentName className) {}
                };
                if (!appContext.bindService(intent, connection, Context.BIND_AUTO_CREATE)) {
                    Log.w(TAG, "Could not bind to Minidump-copying Service " + intent);
                }
                return null;
            }
            // To avoid any potential synchronization issues we post all minidump-copying actions to
            // the same thread to be run serially.
        }
                .executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    // Do not instantiate this class.
    private AwBrowserProcess() {}
}
