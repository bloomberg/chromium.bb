// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Serves as a compound observer proxy for dispatching WebContentsObserver callbacks,
 * avoiding redundant JNI-related work when there are multiple Java-based observers.
 */
@JNINamespace("content")
@MainDex
class WebContentsObserverProxy extends WebContentsObserver {
    private long mNativeWebContentsObserverProxy;
    private final ObserverList<WebContentsObserver> mObservers;
    private final RewindableIterator<WebContentsObserver> mObserversIterator;

    /**
     * Constructs a new WebContentsObserverProxy for a given WebContents
     * instance. A native WebContentsObserver instance will be created, which
     * will observe the native counterpart to the provided WebContents.
     *
     * @param webContents The WebContents instance to observe.
     */
    public WebContentsObserverProxy(WebContentsImpl webContents) {
        ThreadUtils.assertOnUiThread();
        mNativeWebContentsObserverProxy = nativeInit(webContents);
        mObservers = new ObserverList<WebContentsObserver>();
        mObserversIterator = mObservers.rewindableIterator();
    }

    /**
     * Add an observer to the list of proxied observers.
     * @param observer The WebContentsObserver instance to add.
     */
    void addObserver(WebContentsObserver observer) {
        assert mNativeWebContentsObserverProxy != 0;
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer from the list of proxied observers.
     * @param observer The WebContentsObserver instance to remove.
     */
    void removeObserver(WebContentsObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Whether there are any active, proxied observers.
     */
    boolean hasObservers() {
        return !mObservers.isEmpty();
    }

    /**
     * @return The list of proxied observers.
     */
    @VisibleForTesting
    public ObserverList.RewindableIterator<WebContentsObserver> getObserversForTesting() {
        return mObservers.rewindableIterator();
    }

    @Override
    @CalledByNative
    public void renderViewReady() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderViewReady();
        }
    }

    @Override
    @CalledByNative
    public void renderProcessGone(boolean wasOomProtected) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().renderProcessGone(wasOomProtected);
        }
    }

    @Override
    @CalledByNative
    public void didFinishNavigation(
            boolean isMainFrame, boolean isErrorPage, boolean hasCommitted) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFinishNavigation(isMainFrame, isErrorPage, hasCommitted);
        }
    }

    @Override
    @CalledByNative
    public void didStartLoading(String url) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartLoading(url);
        }
    }

    @Override
    @CalledByNative
    public void didStopLoading(String url) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStopLoading(url);
        }
    }

    @Override
    @CalledByNative
    public void didFailLoad(boolean isProvisionalLoad, boolean isMainFrame, int errorCode,
            String description, String failingUrl, boolean wasIgnoredByHandler) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFailLoad(isProvisionalLoad, isMainFrame, errorCode,
                    description, failingUrl, wasIgnoredByHandler);
        }
    }

    @Override
    @CalledByNative
    public void didNavigateMainFrame(String url, String baseUrl,
            boolean isNavigationToDifferentPage, boolean isFragmentNavigation, int statusCode) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didNavigateMainFrame(
                    url, baseUrl, isNavigationToDifferentPage, isFragmentNavigation, statusCode);
        }
    }

    @Override
    @CalledByNative
    public void didFirstVisuallyNonEmptyPaint() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFirstVisuallyNonEmptyPaint();
        }
    }

    @Override
    @CalledByNative
    public void didNavigateAnyFrame(String url, String baseUrl, boolean isReload) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didNavigateAnyFrame(url, baseUrl, isReload);
        }
    }

    @Override
    @CalledByNative
    public void documentAvailableInMainFrame() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().documentAvailableInMainFrame();
        }
    }

    @Override
    @CalledByNative
    public void didStartProvisionalLoadForFrame(long frameId, long parentFrameId,
            boolean isMainFrame, String validatedUrl, boolean isErrorPage, boolean isIframeSrcdoc) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartProvisionalLoadForFrame(
                    frameId, parentFrameId, isMainFrame, validatedUrl, isErrorPage, isIframeSrcdoc);
        }
    }

    @Override
    @CalledByNative
    public void didCommitProvisionalLoadForFrame(
            long frameId, boolean isMainFrame, String url, int transitionType) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didCommitProvisionalLoadForFrame(
                    frameId, isMainFrame, url, transitionType);
        }
    }

    @Override
    @CalledByNative
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didFinishLoad(frameId, validatedUrl, isMainFrame);
        }
    }

    @Override
    @CalledByNative
    public void documentLoadedInFrame(long frameId, boolean isMainFrame) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().documentLoadedInFrame(frameId, isMainFrame);
        }
    }

    @Override
    @CalledByNative
    public void navigationEntryCommitted() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().navigationEntryCommitted();
        }
    }

    @Override
    @CalledByNative
    public void didAttachInterstitialPage() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didAttachInterstitialPage();
        }
    }

    @Override
    @CalledByNative
    public void didDetachInterstitialPage() {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didDetachInterstitialPage();
        }
    }

    @Override
    @CalledByNative
    public void didChangeThemeColor(int color) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didChangeThemeColor(color);
        }
    }

    @Override
    @CalledByNative
    public void didStartNavigationToPendingEntry(String url) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().didStartNavigationToPendingEntry(url);
        }
    }

    @Override
    @CalledByNative
    public void mediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().mediaSessionStateChanged(isControllable, isSuspended);
        }
    }

    @Override
    @CalledByNative
    public void destroy() {
        // Super destruction semantics (removing the observer from the
        // Java-based WebContents) are quite different, so we explicitly avoid
        // calling it here.
        ThreadUtils.assertOnUiThread();
        for (mObserversIterator.rewind(); mObserversIterator.hasNext();) {
            mObserversIterator.next().destroy();
        }
        // All observer destroy() implementations should result in their removal
        // from the proxy.
        assert mObservers.isEmpty();
        mObservers.clear();

        if (mNativeWebContentsObserverProxy != 0) {
            nativeDestroy(mNativeWebContentsObserverProxy);
            mNativeWebContentsObserverProxy = 0;
        }
    }

    private native long nativeInit(WebContentsImpl webContents);
    private native void nativeDestroy(long nativeWebContentsObserverProxy);
}
