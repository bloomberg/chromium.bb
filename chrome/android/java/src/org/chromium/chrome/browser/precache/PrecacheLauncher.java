// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;

import java.util.concurrent.Callable;

/** Class that interacts with the PrecacheManager to control precache cycles. */
public abstract class PrecacheLauncher {
    /** Pointer to the native PrecacheLauncher object. Set to 0 when uninitialized. */
    private long mNativePrecacheLauncher;

    /** Destroy the native PrecacheLauncher, releasing the memory that it was using. */
    public void destroy() {
        if (mNativePrecacheLauncher != 0) {
            nativeDestroy(mNativePrecacheLauncher);
            mNativePrecacheLauncher = 0;
        }
    }

    /** Starts a precache cycle. */
    public void start() {
        // Lazily initialize the native PrecacheLauncher.
        if (mNativePrecacheLauncher == 0) {
            mNativePrecacheLauncher = nativeInit();
        }
        nativeStart(mNativePrecacheLauncher);
    }

    /** Cancel the precache cycle if one is ongoing. */
    public void cancel() {
        // Lazily initialize the native PrecacheLauncher.
        if (mNativePrecacheLauncher == 0) {
            mNativePrecacheLauncher = nativeInit();
        }
        nativeCancel(mNativePrecacheLauncher);
    }

    /** Called when a precache cycle completes. */
    protected abstract void onPrecacheCompleted();

    /**
     * Called by native code when the precache cycle completes. This method exists because an
     * abstract method cannot be directly called from native.
     */
    @CalledByNative
    private void onPrecacheCompletedCallback() {
        onPrecacheCompleted();
    }

    /**
     * Returns true if precaching is enabled by either Finch or the command line flag, and also by
     * the predictive network actions preference and current network connection type.
     *
     * @param privacyPreferencesManager Singleton that manages the prefetch bandwidth preference.
     */
    public static boolean isPrecachingEnabled(
            final PrivacyPreferencesManager privacyPreferencesManager) {
        if (!nativeIsPrecachingEnabled()) return false;
        // privacyPreferencesManager.shouldPrerender() can only be executed on the UI thread.
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return privacyPreferencesManager.shouldPrerender();
            }
        });
    }

    /**
     * If precaching is enabled, then allow the PrecacheService to be launched and signal Chrome
     * when conditions are right to start precaching. If precaching is disabled, prevent the
     * PrecacheService from ever starting.
     *
     * @param privacyPreferencesManager Singleton that manages the prefetch bandwidth preference.
     * @param context The context of the PrecacheServiceLauncher.
     */
    public static void updatePrecachingEnabled(PrivacyPreferencesManager privacyPreferencesManager,
            Context context) {
        PrecacheServiceLauncher.setIsPrecachingEnabled(context,
                isPrecachingEnabled(privacyPreferencesManager));
    }

    private native long nativeInit();
    private native void nativeDestroy(long nativePrecacheLauncher);
    private native void nativeStart(long nativePrecacheLauncher);
    private native void nativeCancel(long nativePrecacheLauncher);
    private static native boolean nativeIsPrecachingEnabled();
}
