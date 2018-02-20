// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.os.Looper;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Handles the promotion and installation of an app specified by the current web page. This object
 * is created by and owned by the native AppBannerUiDelegate.
 */
@JNINamespace("banners")
@JNIAdditionalImport(InstallerDelegate.class)
public class AppBannerUiDelegateAndroid {
    /** Pointer to the native AppBannerUiDelegateAndroid. */
    private long mNativePointer;

    /** Delegate which does the actual monitoring of an in-progress installation. */
    private InstallerDelegate mInstallerDelegate;

    private AppBannerUiDelegateAndroid(long nativePtr) {
        mNativePointer = nativePtr;
    }

    /**
     * Creates the installer delegate with the specified observer. Must be called prior to using
     * this object */
    @CalledByNative
    public void createInstallerDelegate(InstallerDelegate.Observer observer) {
        mInstallerDelegate = new InstallerDelegate(Looper.getMainLooper(), observer);
    }

    @CalledByNative
    private void destroy() {
        mInstallerDelegate.destroy();
        mInstallerDelegate = null;
        mNativePointer = 0;
    }

    @CalledByNative
    private boolean installOrOpenNativeApp(Tab tab, AppData appData, String referrer) {
        return mInstallerDelegate.installOrOpenNativeApp(tab, appData, referrer);
    }

    @CalledByNative
    private void showAppDetails(Tab tab, AppData appData) {
        tab.getWindowAndroid().showIntent(appData.detailsIntent(), null, null);
    }

    @CalledByNative
    private int determineInstallState(String packageName) {
        return mInstallerDelegate.determineInstallState(packageName);
    }

    @CalledByNative
    private static AppBannerUiDelegateAndroid create(long nativePtr) {
        return new AppBannerUiDelegateAndroid(nativePtr);
    }
}
