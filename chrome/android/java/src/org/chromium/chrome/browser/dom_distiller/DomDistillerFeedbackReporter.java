// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;

/**
 * Java implementation of dom_distiller::android::FeedbackReporterAndroid.
 */
@JNINamespace("dom_distiller::android")
public final class DomDistillerFeedbackReporter implements
        DomDistillerFeedbackReportingView.FeedbackObserver {

    private static ExternalFeedbackReporter sExternalFeedbackReporter =
            new NoOpExternalFeedbackReporter();

    public static void setExternalFeedbackReporter(ExternalFeedbackReporter reporter) {
        sExternalFeedbackReporter = reporter;
    }

    private static class NoOpExternalFeedbackReporter implements ExternalFeedbackReporter {
        @Override
        public void reportFeedback(Activity activity, String url, boolean good) {
        }
    }

    private final long mNativePointer;
    private final Tab mTab;

    private ContentViewCore mContentViewCore;
    private DomDistillerFeedbackReportingView mReportingView;

    /**
     * @return whether the DOM Distiller feature is enabled.
     */
    public static boolean isEnabled() {
        return false;
    }

    /**
     * Creates the FeedbackReporter, adds itself as a TabObserver, and ensures
     * references to ContentView and WebContents are up to date.
     *
     * @param tab the tab where the overlay should be displayed.
     */
    public DomDistillerFeedbackReporter(Tab tab) {
        mNativePointer = nativeInit();
        mTab = tab;
        mTab.addObserver(createTabObserver());
        updatePointers();
    }

    @Override
    public void onYesPressed(DomDistillerFeedbackReportingView view) {
        if (view != mReportingView) return;
        recordQuality(true);
        dismissOverlay();
    }

    @Override
    public void onNoPressed(DomDistillerFeedbackReportingView view) {
        if (view != mReportingView) return;
        recordQuality(false);
        dismissOverlay();
    }

    /**
     * Records feedback for the distilled content.
     *
     * @param good whether the perceived quality of the distillation of a web page was good.
     */
    private void recordQuality(boolean good) {
        nativeReportQuality(good);
        if (!good) {
            Activity activity = mTab.getWindowAndroid().getActivity().get();
            String url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(
                   mContentViewCore.getWebContents().getUrl());
            sExternalFeedbackReporter.reportFeedback(activity, url, good);
        }
    }

    /**
     * Start showing the overlay.
     */
    @CalledByNative
    private void showOverlay() {
        mReportingView = DomDistillerFeedbackReportingView.create(mContentViewCore, this);
    }

    /**
     * Dismiss the overlay which is currently being displayed.
     */
    @CalledByNative
    private void dismissOverlay() {
        if (mReportingView != null) {
            mReportingView.dismiss(true);
            mReportingView = null;
        }
    }

    /**
     * Updates which ContentViewCore and WebContents the FeedbackReporter is monitoring.
     */
    private void updatePointers() {
        mContentViewCore = mTab.getContentViewCore();
        nativeReplaceWebContents(mNativePointer, mTab.getWebContents());
    }

    /**
     * Creates a TabObserver for monitoring a Tab, used to react to changes in the ContentViewCore
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
            public void onDestroyed(Tab tab) {
                nativeDestroy(mNativePointer);
                mContentViewCore = null;
            }
        };
    }

    private static native boolean nativeIsEnabled();

    private static native void nativeReportQuality(boolean good);

    private native long nativeInit();

    private native void nativeDestroy(long nativeFeedbackReporterAndroid);

    private native void nativeReplaceWebContents(
            long nativeFeedbackReporterAndroid, WebContents webContents);
}
