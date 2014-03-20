// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.content.browser.ContentView;
import org.chromium.content_public.browser.WebContents;

/**
 * Java implementation of FeedbackReporterAndroid.
 */
@JNINamespace("dom_distiller::android")
public final class FeedbackReporter implements FeedbackReportingView.FeedbackObserver {

    private final long mNativePointer;
    private final Tab mTab;
    private ContentView mContentView;
    private FeedbackReportingView mFeedbackReportingView;
    private String mOverlayUrl;

    /**
     * @return whether the DOM Distiller feature is enabled.
     */
    public static boolean isEnabled() {
        return nativeIsEnabled();
    }

    /**
     * Records feedback for the distilled content.
     *
     * @param good whether the perceived quality of the distillation of a web page was good.
     */
    private static void recordQuality(boolean good) {
        nativeReportQuality(good);
    }

    /**
     * Creates the FeedbackReporter, adds itself as a TabObserver, and ensures
     * references to ContentView and WebContents are up to date.
     *
     * @param tab the tab where the overlay should be displayed.
     */
    public FeedbackReporter(Tab tab) {
        mNativePointer = nativeInit();
        mTab = tab;
        mTab.addObserver(createTabObserver());
        updatePointers();
    }

    @Override
    public void onYesPressed(FeedbackReportingView view) {
        if (view != mFeedbackReportingView) return;
        recordQuality(true);
        dismissOverlay();
    }

    @Override
    public void onNoPressed(FeedbackReportingView view) {
        if (view != mFeedbackReportingView) return;
        recordQuality(false);
        dismissOverlay();
    }

    /**
     * Start showing the overlay.
     */
    private void showOverlay() {
        mFeedbackReportingView = FeedbackReportingView.create(mContentView, this);
    }

    /**
     * Dismiss the overlay which is currently being displayed.
     */
    @CalledByNative
    private void dismissOverlay() {
        if (mFeedbackReportingView != null) mFeedbackReportingView.dismiss();
        mOverlayUrl = null;
        mFeedbackReportingView = null;
    }

    @CalledByNative
    private String getCurrentOverlayUrl() {
        return mOverlayUrl;
    }

    /**
     * Updates which ContentView and WebContents the FeedbackReporter is monitoring.
     */
    private void updatePointers() {
        mContentView = mTab.getContentView();
        nativeReplaceWebContents(mNativePointer, mTab.getWebContents());
    }

    /**
     * Creates a TabObserver for monitoring a Tab, used to react to changes in the ContentView
     * or to trigger its own destruction.
     *
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
            public void onUpdateUrl(Tab tab, String url) {
                boolean reportable = nativeIsReportableUrl(url);
                if (reportable) {
                    mOverlayUrl = url;
                    showOverlay();
                } else {
                    dismissOverlay();
                }
            }

            @Override
            public void onDestroyed(Tab tab) {
                nativeDestroy(mNativePointer);
                mContentView = null;
            }
        };
    }

    private static native boolean nativeIsEnabled();

    private static native boolean nativeIsReportableUrl(String url);

    private static native void nativeReportQuality(boolean good);

    private native long nativeInit();

    private native void nativeDestroy(long nativeFeedbackReporterAndroid);

    private native void nativeReplaceWebContents(
            long nativeFeedbackReporterAndroid, WebContents webContents);
}
