// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.content.browser.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

/**
 * Manages an AppBannerView for a Tab and its ContentView.
 *
 * The AppBannerManager manages a single AppBannerView, dismissing it when the user navigates to a
 * new page or creating a new one when it detects that the current webpage is requesting a banner
 * to be built.  The actual observation of the WebContents (which triggers the automatic creation
 * and removal of banners, among other things) is done by the native-side AppBannerManager.
 *
 * This Java-side class owns its native-side counterpart, which is basically used to grab resources
 * from the network.
 */
public class AppBannerManager implements AppBannerView.Observer, InstallerDelegate.Observer,
        AppDetailsDelegate.Observer, IntentCallback {
    private static final String TAG = "AppBannerManager";

    /** Retrieves information about a given package. */
    private static AppDetailsDelegate sAppDetailsDelegate;

    /** Pointer to the native side AppBannerManager. */
    private final long mNativePointer;

    /** Tab that the AppBannerView/AppBannerManager is owned by. */
    private final Tab mTab;

    /** ContentView that the AppBannerView/AppBannerManager is currently attached to. */
    private ContentView mContentView;

    /** Current banner being shown. */
    private AppBannerView mBannerView;

    /** Data about the app being advertised. */
    private AppData mAppData;

    /** Task to get details about an app. */
    private AppDetailsDelegate.DetailsTask mDetailsTask;

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
        mTab.addObserver(createTabObserver());
        updatePointers();
    }

    /**
     * Creates a TabObserver for monitoring a Tab, used to react to changes in the ContentView
     * or to trigger its own destruction.
     * @return TabObserver that can be used to monitor a Tab.
     */
    private TabObserver createTabObserver() {
        return new EmptyTabObserver() {
            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad,
                    boolean didFinishLoad) {
                updatePointers();
            }

            @Override
            public void onContentChanged(Tab tab) {
                updatePointers();
            }

            @Override
            public void onDestroyed(Tab tab) {
                nativeDestroy(mNativePointer);
                resetState();
            }
        };
    }

    /**
     * Updates which ContentView and WebContents the AppBannerView is monitoring.
     */
    private void updatePointers() {
        if (mContentView != mTab.getContentView()) mContentView = mTab.getContentView();
        nativeReplaceWebContents(mNativePointer, mTab.getWebContents());
    }

    /**
     * Grabs package information for the banner asynchronously.
     * @param url         URL for the page that is triggering the banner.
     * @param packageName Name of the package that is being advertised.
     */
    @CalledByNative
    private void prepareBanner(String url, String packageName) {
        // Get rid of whatever banner is there currently.
        if (mBannerView != null) dismissCurrentBanner();

        if (sAppDetailsDelegate == null || !isBannerForCurrentPage(url)) return;

        mDetailsTask = sAppDetailsDelegate.createTask(
                this, mContentView.getContext(), url, packageName);
        mDetailsTask.execute();
    }

    /**
     * Called when data about the package has been retrieved, which includes the url for the app's
     * icon but not the icon Bitmap itself.  Kicks off a background task to retrieve it.
     * @param data Data about the app.  Null if the task failed.
     */
    @Override
    public void onAppDetailsRetrieved(AppData data) {
        if (data == null || !isBannerForCurrentPage(data.siteUrl())) return;

        mAppData = data;
        String imageUrl = data.imageUrl();
        if (TextUtils.isEmpty(imageUrl) || !nativeFetchIcon(mNativePointer, imageUrl)) resetState();
    }

    /**
     * Called when all the data required to show a banner has finally been retrieved.
     * Creates the banner and shows it, as long as the banner is still meant for the current page.
     * @param imageUrl URL of the icon.
     * @param appIcon  Bitmap containing the icon itself.
     */
    @CalledByNative
    private void createBanner(String imageUrl, Bitmap appIcon) {
        if (mAppData == null || !isBannerForCurrentPage(mAppData.siteUrl())) return;

        if (!TextUtils.equals(mAppData.imageUrl(), imageUrl)) {
            resetState();
            return;
        }

        mAppData.setIcon(new BitmapDrawable(mContentView.getContext().getResources(), appIcon));
        mBannerView = AppBannerView.create(mContentView, this, mAppData);
    }

    /**
     * Dismisses whatever banner is currently being displayed.
     * This is treated as an automatic dismissal and not one that blocks the banner from appearing
     * in the future.
     */
    @CalledByNative
    private void dismissCurrentBanner() {
        if (mBannerView != null) mBannerView.dismiss();
        resetState();
    }

    @Override
    public void onButtonClicked(AppBannerView banner) {
        if (mBannerView != banner) return;

        if (mAppData.installState() == AppData.INSTALL_STATE_NOT_INSTALLED) {
            // The user initiated an install.
            WindowAndroid window = mTab.getWindowAndroid();
            if (window.showIntent(mAppData.installIntent(), this, R.string.low_memory_error)) {
                // Temporarily hide the banner.
                mBannerView.createVerticalSnapAnimation(false);
            } else {
                Log.e(TAG, "Failed to fire install intent.");
                dismissCurrentBanner();
            }
        } else if (mAppData.installState() == AppData.INSTALL_STATE_INSTALLED) {
            // The app is installed.  Open it.
            String packageName = mAppData.packageName();
            PackageManager packageManager = mContentView.getContext().getPackageManager();
            Intent appIntent = packageManager.getLaunchIntentForPackage(packageName);
            try {
                mContentView.getContext().startActivity(appIntent);
            } catch (ActivityNotFoundException e) {
                Log.e(TAG, "Failed to find app package: " + packageName);
            }
            dismissCurrentBanner();
        }
    }

    @Override
    public void onBannerClicked(AppBannerView banner) {
        if (mContentView == null || mBannerView == null || mBannerView != banner) {
            return;
        }

        try {
            // Send the user to the app's Play store page.
            IntentSender sender = banner.getAppData().detailsIntent().getIntentSender();
            mContentView.getContext().startIntentSender(sender, new Intent(), 0, 0, 0);
        } catch (IntentSender.SendIntentException e) {
            Log.e(TAG, "Failed to launch details intent.");
        }

        dismissCurrentBanner();
    }

    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode,
            ContentResolver contentResolver, Intent data) {
        if (mContentView == null || mBannerView == null || mBannerView.getAppData() != mAppData) {
            return;
        }

        mBannerView.createVerticalSnapAnimation(true);
        if (resultCode == Activity.RESULT_OK) {
            // The user chose to install the app.  Watch the PackageManager to see when it finishes
            // installing it.
            mAppData.beginTrackingInstallation(mContentView.getContext(), this);
            mBannerView.updateButtonState();
        }
    }

    @Override
    public void onInstallFinished(InstallerDelegate monitor, boolean success) {
        if (mBannerView == null || mAppData == null || mAppData.installTask() != monitor) return;

        if (success) {
            // Let the user open the app from here.
            mAppData.setInstallState(AppData.INSTALL_STATE_INSTALLED);
            mBannerView.updateButtonState();
        } else {
            dismissCurrentBanner();
        }
    }

    @Override
    public void onBannerDismissed(AppBannerView banner) {
        if (mBannerView != banner) return;

        // If the user swiped the banner off of the screen, block it from being shown again.
        boolean swipedAway = Math.abs(banner.getTranslationX()) >= banner.getWidth();
        if (swipedAway) {
            nativeBlockBanner(mNativePointer, mAppData.siteUrl(), mAppData.packageName());
        }

        resetState();
    }

    /**
     * Resets all of the state, killing off any running tasks.
     */
    private void resetState() {
        if (mDetailsTask != null) {
            mDetailsTask.cancel(true);
            mDetailsTask = null;
        }

        if (mAppData != null) {
            mAppData.destroy();
            mAppData = null;
        }

        mBannerView = null;
    }

    /**
     * Checks to see if the banner is for the currently displayed page.
     * @param bannerUrl URL that requested a banner.
     * @return          True if the user is still on the same page.
     */
    private boolean isBannerForCurrentPage(String bannerUrl) {
        return mContentView != null && TextUtils.equals(mContentView.getUrl(), bannerUrl);
    }

    private static native boolean nativeIsEnabled();
    private native long nativeInit();
    private native void nativeDestroy(long nativeAppBannerManager);
    private native void nativeReplaceWebContents(
            long nativeAppBannerManager, WebContents webContents);
    private native void nativeBlockBanner(
            long nativeAppBannerManager, String url, String packageName);
    private native boolean nativeFetchIcon(long nativeAppBannerManager, String imageUrl);
}
