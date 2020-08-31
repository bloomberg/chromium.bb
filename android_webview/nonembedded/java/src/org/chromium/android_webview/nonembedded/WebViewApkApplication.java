// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import org.chromium.android_webview.AwLocaleConfig;
import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.metrics.AwNonembeddedUmaRecorder;
import org.chromium.android_webview.devui.util.WebViewPackageHelper;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.embedder_support.application.FontPreloadingWorkaround;
import org.chromium.ui.base.ResourceBundle;

/**
 * Application subclass for SystemWebView and Trichrome.
 *
 * Application subclass is used by renderer processes, services, and content providers that run
 * under the WebView APK's package.
 *
 * None of this code runs in an application which simply uses WebView.
 */
@JNINamespace("android_webview")
public class WebViewApkApplication extends Application {
    // Called by the framework for ALL processes. Runs before ContentProviders are created.
    // Quirk: context.getApplicationContext() returns null during this method.
    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        ContextUtils.initApplicationContext(this);
        maybeInitProcessGlobals();

        // MonochromeApplication has its own locale configuration already, so call this here
        // rather than in maybeInitProcessGlobals.
        ResourceBundle.setAvailablePakLocales(
                new String[] {}, AwLocaleConfig.getWebViewSupportedPakLocales());
    }

    @Override
    public void onCreate() {
        super.onCreate();
        FontPreloadingWorkaround.maybeInstallWorkaround(this);
    }

    /**
     * Initializes globals needed for components that run in the "webview_apk" or "webview_service"
     * process.
     *
     * This is also called by MonochromeApplication, so the initialization here will run
     * for those processes regardless of whether the WebView is standalone or Monochrome.
     */
    public static void maybeInitProcessGlobals() {
        if (isWebViewProcess()) {
            PathUtils.setPrivateDataDirectorySuffix("webview", "WebView");
            CommandLineUtil.initCommandLine();
            // disable using a native recorder in this process because native lib isn't loaded.
            UmaRecorderHolder.setAllowNativeUmaRecorder(false);
            UmaRecorderHolder.setNonNativeDelegate(new AwNonembeddedUmaRecorder());
        }
    }

    // Returns true if running in the "webview_apk" or "webview_service" process.
    public static boolean isWebViewProcess() {
        // Either "webview_service", or "webview_apk".
        // "webview_service" is meant to be very light-weight and never load the native library.
        return ContextUtils.getProcessName().contains(":webview_");
    }

    /**
     * Post a non-blocking, low priority background task that shows a launcher icon for WebView
     * DevTools if this Monochrome package is the current selected WebView provider for the system
     * otherwise it hides that icon. This works only for Monochrome and shouldn't be used for other
     * WebView providers. Other WebView Providers (Standalone and Trichrome) will always have
     * launcher icons whether they are the current selected providers or not.
     *
     * Should be guarded by process type checks and should only be called if it's a webview process
     * or a browser process.
     */
    public static void postDeveloperUiLauncherIconTask() {
        PostTask.postTask(TaskTraits.BEST_EFFORT, () -> {
            Context context = ContextUtils.getApplicationContext();
            try {
                ComponentName devToolsLauncherActivity = new ComponentName(
                        context, "org.chromium.android_webview.devui.MonochromeLauncherActivity");
                int oldIconState = context.getPackageManager().getComponentEnabledSetting(
                        devToolsLauncherActivity);

                // Enable the icon if this is the current WebView provider, otherwise set the icon
                // back to default (disabled) state.
                boolean shouldShowIcon =
                        WebViewPackageHelper.isCurrentSystemWebViewImplementation(context);
                int newIconState = shouldShowIcon ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                                                  : PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;

                if (oldIconState == newIconState) return;

                context.getPackageManager().setComponentEnabledSetting(
                        devToolsLauncherActivity, newIconState, PackageManager.DONT_KILL_APP);
                RecordHistogram.recordBooleanHistogram(
                        "Android.WebView.DevUi.MonochromeIconStateToggled", shouldShowIcon);
            } catch (IllegalArgumentException e) {
                // If MonochromeLauncherActivity doesn't exist, Dynamically showing/hiding DevTools
                // launcher icon is not enabled in this package; e.g when it is a stable channel.
            }
        });
    }

    /**
     * Performs minimal native library initialization required when running as a stand-alone APK.
     * @return True if the library was loaded, false if running as webview stub.
     */
    static synchronized boolean initializeNative() {
        try {
            if (LibraryLoader.getInstance().isInitialized()) {
                return true;
            }
            // Should not call LibraryLoader.initialize() since this will reset UmaRecorder
            // delegate.
            LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_WEBVIEW);
            LibraryLoader.getInstance().loadNow();
        } catch (Throwable unused) {
            // Happens for WebView Stub. Throws NoClassDefFoundError because of no
            // NativeLibraries.java being generated.
            return false;
        }
        LibraryLoader.getInstance().switchCommandLineForWebView();
        WebViewApkApplicationJni.get().initializePakResources();
        return true;
    }

    @NativeMethods
    interface Natives {
        void initializePakResources();
    }
}
