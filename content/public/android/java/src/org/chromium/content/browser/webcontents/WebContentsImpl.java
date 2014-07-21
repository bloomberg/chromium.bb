// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;

/**
 * The WebContentsImpl Java wrapper to allow communicating with the native WebContentsImpl
 * object.
 */
@JNINamespace("content")
//TODO(tedchoc): Remove the package restriction once this class moves to a non-public content
//               package whose visibility will be enforced via DEPS.
/* package */ class WebContentsImpl implements WebContents {

    private long mNativeWebContentsAndroid;
    private NavigationController mNavigationController;

    private WebContentsImpl(
            long nativeWebContentsAndroid, NavigationController navigationController) {
        mNativeWebContentsAndroid = nativeWebContentsAndroid;
        mNavigationController = navigationController;
    }

    @CalledByNative
    private static WebContentsImpl create(
            long nativeWebContentsAndroid, NavigationController navigationController) {
        return new WebContentsImpl(nativeWebContentsAndroid, navigationController);
    }

    @CalledByNative
    private void destroy() {
        mNativeWebContentsAndroid = 0;
        mNavigationController = null;
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeWebContentsAndroid;
    }

    @Override
    public NavigationController getNavigationController() {
        return mNavigationController;
    }

    @Override
    public String getTitle() {
        return nativeGetTitle(mNativeWebContentsAndroid);
    }

    @Override
    public String getVisibleUrl() {
        return nativeGetVisibleURL(mNativeWebContentsAndroid);
    }

    @Override
    public void stop() {
        nativeStop(mNativeWebContentsAndroid);
    }

    @Override
    public void insertCSS(String css) {
        if (mNativeWebContentsAndroid == 0) return;
        nativeInsertCSS(mNativeWebContentsAndroid, css);
    }

    @Override
    public void onHide() {
         nativeOnHide(mNativeWebContentsAndroid);
    }

    @Override
    public void onShow() {
        nativeOnShow(mNativeWebContentsAndroid);
    }

    @Override
    public int getBackgroundColor() {
        return nativeGetBackgroundColor(mNativeWebContentsAndroid);
    }

    @Override
    public void addStyleSheetByURL(String url) {
        nativeAddStyleSheetByURL(mNativeWebContentsAndroid, url);
    }

    @Override
    public void showInterstitialPage(
            String url, long interstitialPageDelegateAndroid) {
        nativeShowInterstitialPage(mNativeWebContentsAndroid, url, interstitialPageDelegateAndroid);
    }

    @Override
    public boolean isShowingInterstitialPage() {
        return nativeIsShowingInterstitialPage(mNativeWebContentsAndroid);
    }

    @Override
    public boolean isReady() {
        return nativeIsRenderWidgetHostViewReady(mNativeWebContentsAndroid);
    }

    @Override
    public void exitFullscreen() {
        nativeExitFullscreen(mNativeWebContentsAndroid);
    }

    @Override
    public void updateTopControlsState(boolean enableHiding, boolean enableShowing,
            boolean animate) {
        nativeUpdateTopControlsState(mNativeWebContentsAndroid, enableHiding,
                enableShowing, animate);
    }

    @Override
    public void showImeIfNeeded() {
        nativeShowImeIfNeeded(mNativeWebContentsAndroid);
    }

    @Override
    public void scrollFocusedEditableNodeIntoView() {
        // The native side keeps track of whether the zoom and scroll actually occurred. It is
        // more efficient to do it this way and sometimes fire an unnecessary message rather
        // than synchronize with the renderer and always have an additional message.
        nativeScrollFocusedEditableNodeIntoView(mNativeWebContentsAndroid);
    }

    @Override
    public void selectWordAroundCaret() {
        nativeSelectWordAroundCaret(mNativeWebContentsAndroid);
    }

    private native String nativeGetTitle(long nativeWebContentsAndroid);
    private native String nativeGetVisibleURL(long nativeWebContentsAndroid);
    private native void nativeStop(long nativeWebContentsAndroid);
    private native void nativeInsertCSS(long nativeWebContentsAndroid, String css);
    private native void nativeOnHide(long nativeWebContentsAndroid);
    private native void nativeOnShow(long nativeWebContentsAndroid);
    private native int nativeGetBackgroundColor(long nativeWebContentsAndroid);
    private native void nativeAddStyleSheetByURL(long nativeWebContentsAndroid,
            String url);
    private native void nativeShowInterstitialPage(long nativeWebContentsAndroid,
            String url, long nativeInterstitialPageDelegateAndroid);
    private native boolean nativeIsShowingInterstitialPage(long nativeWebContentsAndroid);
    private native boolean nativeIsRenderWidgetHostViewReady(long nativeWebContentsAndroid);
    private native void nativeExitFullscreen(long nativeWebContentsAndroid);
    private native void nativeUpdateTopControlsState(long nativeWebContentsAndroid,
            boolean enableHiding, boolean enableShowing, boolean animate);
    private native void nativeShowImeIfNeeded(long nativeWebContentsAndroid);
    private native void nativeScrollFocusedEditableNodeIntoView(long nativeWebContentsAndroid);
    private native void nativeSelectWordAroundCaret(long nativeWebContentsAndroid);
}
