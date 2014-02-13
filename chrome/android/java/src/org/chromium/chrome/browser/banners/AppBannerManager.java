// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.TabBase;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.content.browser.ContentView;
import org.chromium.content_public.browser.WebContents;

/**
 * Manages an AppBannerView for a TabBase and its ContentView.
 *
 * The AppBannerManager manages a single AppBannerView, dismissing it when the user navigates to a
 * new page or creating a new one when it detects that the current webpage is requesting a banner
 * to be built.  The actual observeration of the WebContents (which triggers the automatic creation
 * and removal of banners, among other things) is done by the native-side AppBannerManager.
 *
 * This Java-side class owns its native-side counterpart.
 */
public class AppBannerManager {
    /** Pointer to the native side AppBannerManager. */
    private final long mNativePointer;

    /** TabBase that the AppBannerView/AppBannerManager is owned by. */
    private final TabBase mTabBase;

    /** ContentView that the AppBannerView/AppBannerManager is currently attached to. */
    private ContentView mContentView;

    /**
     * Constructs an AppBannerManager for the given tab.
     * @param tab Tab that the AppBannerManager will be attached to.
     */
    public AppBannerManager(TabBase tab) {
        mNativePointer = nativeInit();
        mTabBase = tab;
        mTabBase.addObserver(createTabObserver());
        updatePointers();
    }

    /**
     * Creates a TabObserver for monitoring a TabBase, used to react to changes in the ContentView
     * or to trigger its own destruction.
     * @return TabObserver that can be used to monitor a TabBase.
     */
    private TabObserver createTabObserver() {
        return new EmptyTabObserver() {
            @Override
            public void onWebContentsSwapped(TabBase tab, boolean didStartLoad,
                    boolean didFinishLoad) {
                updatePointers();
            }

            @Override
            public void onContentChanged(TabBase tab) {
                updatePointers();
            }

            @Override
            public void onDestroyed(TabBase tab) {
                nativeDestroy(mNativePointer);
            }
        };
    }

    /**
     * Updates which ContentView and WebContents the AppBannerView is monitoring.
     */
    private void updatePointers() {
        if (mContentView != mTabBase.getContentView()) mContentView = mTabBase.getContentView();
        nativeReplaceWebContents(mNativePointer, mTabBase.getWebContents());
    }

    /**
     * Checks if app banners are enabled.
     * @return True if banners are enabled, false otherwise.
     */
    public static boolean isEnabled() {
        return nativeIsEnabled();
    }

    private static native boolean nativeIsEnabled();
    private native long nativeInit();
    private native void nativeDestroy(long nativeAppBannerManager);
    private native void nativeReplaceWebContents(
            long nativeAppBannerManager, WebContents webContents);
}
