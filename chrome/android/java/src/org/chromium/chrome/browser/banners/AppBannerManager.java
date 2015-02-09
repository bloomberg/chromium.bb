// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Looper;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.infobar.AppBannerInfoBar;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manages an AppBannerInfoBar for a Tab.
 *
 * The AppBannerManager manages a single AppBannerInfoBar, creating a new one when it detects that
 * the current webpage is requesting a banner to be built. The actual observation of the WebContents
 * (which triggers the automatic creation and removal of banners, among other things) is done by the
 * native-side AppBannerManager.
 *
 * This Java-side class owns its native-side counterpart, which is basically used to grab resources
 * from the network.
 */
@JNINamespace("banners")
public class AppBannerManager extends EmptyTabObserver {
    private static final String TAG = "AppBannerManager";

    /** Retrieves information about a given package. */
    private static AppDetailsDelegate sAppDetailsDelegate;

    /** Pointer to the native side AppBannerManager. */
    private final long mNativePointer;

    /** Tab that the AppBannerView/AppBannerManager is owned by. */
    private final Tab mTab;

    /** Monitors an installation in progress. */
    private InstallerDelegate mInstallTask;

    /** Monitors for application state changes. */
    private final ApplicationStatus.ApplicationStateListener mListener;

    /**
     * Checks if app banners are enabled.
     * @return True if banners are enabled, false otherwise.
     */
    public static boolean isEnabled() {
        return nativeIsEnabled();
    }

    /**
     * Sets the delegate that provides information about a given package.
     * @param delegate Delegate to use.  Previously set ones are destroyed.
     */
    public static void setAppDetailsDelegate(AppDetailsDelegate delegate) {
        if (sAppDetailsDelegate != null) sAppDetailsDelegate.destroy();
        sAppDetailsDelegate = delegate;
    }

    /**
     * Constructs an AppBannerManager for the given tab.
     * @param tab Tab that the AppBannerManager will be attached to.
     */
    public AppBannerManager(Tab tab) {
        mNativePointer = nativeInit();
        mTab = tab;
        updatePointers();
        mListener = createApplicationStateListener();
        ApplicationStatus.registerApplicationStateListener(mListener);
    }

    private ApplicationStatus.ApplicationStateListener createApplicationStateListener() {
        return new ApplicationStatus.ApplicationStateListener() {
            @Override
            public void onApplicationStateChange(int newState) {
                if (!ApplicationStatus.hasVisibleActivities()) return;
                nativeUpdateInstallState(mNativePointer);
            }
        };
    }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad,
            boolean didFinishLoad) {
        updatePointers();
    }

    @Override
    public void onContentChanged(Tab tab) {
        updatePointers();
    }

    /**
     * Destroys the native AppBannerManager.
     */
    public void destroy() {
        if (mInstallTask != null) {
            mInstallTask.cancel();
            mInstallTask = null;
        }
        ApplicationStatus.unregisterApplicationStateListener(mListener);
        nativeDestroy(mNativePointer);
    }

    /**
     * Updates which WebContents the native AppBannerManager is monitoring.
     */
    private void updatePointers() {
        nativeReplaceWebContents(mNativePointer, mTab.getWebContents());
    }

    @CalledByNative
    private int getPreferredIconSize() {
        return ApplicationStatus.getApplicationContext().getResources().getDimensionPixelSize(
                R.dimen.app_banner_icon_size);
    }

    /**
     * Grabs package information for the banner asynchronously.
     * @param url         URL for the page that is triggering the banner.
     * @param packageName Name of the package that is being advertised.
     */
    @CalledByNative
    private void fetchAppDetails(String url, String packageName) {
        if (sAppDetailsDelegate == null) return;
        int iconSize = getPreferredIconSize();
        sAppDetailsDelegate.getAppDetailsAsynchronously(
                createAppDetailsObserver(), url, packageName, iconSize);
    }

    private AppDetailsDelegate.Observer createAppDetailsObserver() {
        return new AppDetailsDelegate.Observer() {
            /**
             * Called when data about the package has been retrieved, which includes the url for the
             * app's icon but not the icon Bitmap itself.
             * @param data Data about the app.  Null if the task failed.
             */
            @Override
            public void onAppDetailsRetrieved(AppData data) {
                if (data == null) return;

                String imageUrl = data.imageUrl();
                if (TextUtils.isEmpty(imageUrl)) return;

                nativeOnAppDetailsRetrieved(
                        mNativePointer, data, data.title(), data.packageName(), data.imageUrl());
            }
        };
    }

    @CalledByNative
    private boolean installOrOpenNativeApp(AppData appData) {
        Context context = ApplicationStatus.getApplicationContext();
        String packageName = appData.packageName();
        PackageManager packageManager = context.getPackageManager();

        if (InstallerDelegate.isInstalled(packageManager, packageName)) {
            // Open the app.
            Intent launchIntent = packageManager.getLaunchIntentForPackage(packageName);
            if (launchIntent == null) return true;
            context.startActivity(launchIntent);
            return true;
        } else {
            // Try installing the app.  If the installation was kicked off, return false to prevent
            // the infobar from disappearing.
            return !mTab.getWindowAndroid().showIntent(
                    appData.installIntent(), createIntentCallback(appData), null);
        }
    }

    private WindowAndroid.IntentCallback createIntentCallback(final AppData appData) {
        return new WindowAndroid.IntentCallback() {
            @Override
            public void onIntentCompleted(WindowAndroid window, int resultCode,
                    ContentResolver contentResolver, Intent data) {
                boolean isInstalling = resultCode == Activity.RESULT_OK;
                if (isInstalling) {
                    // Start monitoring the install.
                    PackageManager pm =
                            ApplicationStatus.getApplicationContext().getPackageManager();
                    mInstallTask = new InstallerDelegate(
                            Looper.getMainLooper(), pm, createInstallerDelegateObserver(),
                            appData.packageName());
                    mInstallTask.start();
                }

                nativeOnInstallIntentReturned(mNativePointer, isInstalling);
            }
        };
    }

    private InstallerDelegate.Observer createInstallerDelegateObserver() {
        return new InstallerDelegate.Observer() {
            @Override
            public void onInstallFinished(InstallerDelegate task, boolean success) {
                if (mInstallTask != task) return;
                mInstallTask = null;
                nativeOnInstallFinished(mNativePointer, success);
            }
        };
    }

    @CalledByNative
    private void showAppDetails(AppData appData) {
        mTab.getWindowAndroid().showIntent(appData.detailsIntent(), null, null);
    }

    @CalledByNative
    private int determineInstallState(AppData data) {
        if (mInstallTask != null) return AppBannerInfoBar.INSTALL_STATE_INSTALLING;

        PackageManager pm = ApplicationStatus.getApplicationContext().getPackageManager();
        boolean isInstalled = InstallerDelegate.isInstalled(pm, data.packageName());
        return isInstalled ? AppBannerInfoBar.INSTALL_STATE_INSTALLED
                : AppBannerInfoBar.INSTALL_STATE_NOT_INSTALLED;
    }

    private static native boolean nativeIsEnabled();
    private native long nativeInit();
    private native void nativeDestroy(long nativeAppBannerManager);
    private native void nativeReplaceWebContents(long nativeAppBannerManager,
            WebContents webContents);
    private native boolean nativeOnAppDetailsRetrieved(long nativeAppBannerManager, AppData data,
            String title, String packageName, String imageUrl);
    private native void nativeOnInstallIntentReturned(
            long nativeAppBannerManager, boolean isInstalling);
    private native void nativeOnInstallFinished(long nativeAppBannerManager, boolean success);
    private native void nativeUpdateInstallState(long nativeAppBannerManager);

    // UMA tracking.
    private static native void nativeRecordDismissEvent(int metric);
    private static native void nativeRecordInstallEvent(int metric);
}
