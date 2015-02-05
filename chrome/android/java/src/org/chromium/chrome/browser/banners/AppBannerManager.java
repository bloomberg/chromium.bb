// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.PendingIntent;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;

/**
 * Manages an AppBannerView for a Tab and its ContentView.
 *
 * The AppBannerManager manages a single AppBannerView, dismissing it when the user navigates to a
 * new page or creating a new one when it detects that the current webpage is requesting a banner to
 * be built. The actual observation of the WebContents (which triggers the automatic creation and
 * removal of banners, among other things) is done by the native-side AppBannerManager.
 *
 * This Java-side class owns its native-side counterpart, which is basically used to grab resources
 * from the network.
 */
@JNINamespace("banners")
public class AppBannerManager extends EmptyTabObserver
        implements AppBannerView.Observer, AppDetailsDelegate.Observer {
    private static final String TAG = "AppBannerManager";

    /** Retrieves information about a given package. */
    private static AppDetailsDelegate sAppDetailsDelegate;

    /** Pointer to the native side AppBannerManager. */
    private final long mNativePointer;

    /** Tab that the AppBannerView/AppBannerManager is owned by. */
    private final Tab mTab;

    /** ContentViewCore that the AppBannerView/AppBannerManager is currently attached to. */
    private ContentViewCore mContentViewCore;

    /** Current banner being shown. */
    private AppBannerView mBannerView;

    /** Data about the app being advertised. */
    private AppData mAppData;

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
        nativeDestroy(mNativePointer);
        resetState();
    }

    /**
     * Updates which ContentView and WebContents the AppBannerView is monitoring.
     */
    private void updatePointers() {
        if (mContentViewCore != mTab.getContentViewCore())
            mContentViewCore = mTab.getContentViewCore();
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
    private void prepareBanner(String url, String packageName) {
        if (sAppDetailsDelegate == null || !isBannerForCurrentPage(url)) return;

        int iconSize = getPreferredIconSize();
        sAppDetailsDelegate.getAppDetailsAsynchronously(this, url, packageName, iconSize);
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

    @Override
    public void onBannerRemoved(AppBannerView banner) {
        if (mBannerView != banner) return;
        resetState();
    }

    @Override
    public void onBannerBlocked(AppBannerView banner, String url, String packageName) {
        if (mBannerView != banner) return;
        nativeBlockBanner(mNativePointer, url, packageName);
    }

    @Override
    public void onBannerDismissEvent(AppBannerView banner, int eventType) {
        if (mBannerView != banner) return;
        nativeRecordDismissEvent(eventType);
    }

    @Override
    public void onBannerInstallEvent(AppBannerView banner, int eventType) {
        if (mBannerView != banner) return;
        nativeRecordInstallEvent(eventType);
    }

    @Override
    public boolean onFireIntent(AppBannerView banner, PendingIntent intent) {
        if (mBannerView != banner) return false;
        return mTab.getWindowAndroid().showIntent(intent, banner, R.string.low_memory_error);
    }

    /**
     * Resets all of the state, killing off any running tasks.
     */
    private void resetState() {
        if (mBannerView != null) {
            mBannerView.destroy();
            mBannerView = null;
        }

        mAppData = null;
    }

    /**
     * Checks to see if the banner is for the currently displayed page.
     * @param bannerUrl URL that requested a banner.
     * @return          True if the user is still on the same page.
     */
    private boolean isBannerForCurrentPage(String bannerUrl) {
        return mContentViewCore != null
                && TextUtils.equals(mContentViewCore.getWebContents().getUrl(), bannerUrl);
    }

    private static native boolean nativeIsEnabled();
    private native long nativeInit();
    private native void nativeDestroy(long nativeAppBannerManager);
    private native void nativeReplaceWebContents(long nativeAppBannerManager,
            WebContents webContents);
    private native void nativeBlockBanner(
            long nativeAppBannerManager, String url, String packageName);
    private native boolean nativeFetchIcon(long nativeAppBannerManager, String imageUrl);

    // UMA tracking.
    private static native void nativeRecordDismissEvent(int metric);
    private static native void nativeRecordInstallEvent(int metric);
}
