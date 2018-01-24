// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityNodeProvider;

import org.chromium.base.VisibleForTesting;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewCore.InternalAccessDelegate;
import org.chromium.content.browser.PopupZoomer;
import org.chromium.content.browser.accessibility.WebContentsAccessibility;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.input.TextSuggestionHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * A dummy {@link ContentViewCore} implementation that can be overriden by tests
 * to customize behavior.
 */
public class TestContentViewCore implements ContentViewCore {
    public TestContentViewCore(Context context, String productVersion) {}

    @Override
    public Context getContext() {
        return null;
    }

    @Override
    public ViewGroup getContainerView() {
        return null;
    }

    @Override
    public WebContents getWebContents() {
        return null;
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        return null;
    }

    @Override
    public TextSuggestionHost getTextSuggestionHostForTesting() {
        return null;
    }

    @Override
    public void setTextSuggestionHostForTesting(TextSuggestionHost textSuggestionHost) {}

    @Override
    public void initialize(ViewAndroidDelegate viewDelegate,
            InternalAccessDelegate internalDispatcher, WebContents webContents,
            WindowAndroid windowAndroid) {}

    @Override
    public void updateWindowAndroid(WindowAndroid windowAndroid) {}

    @Override
    public void setContainerView(ViewGroup containerView) {}

    @Override
    public void setContainerViewInternals(InternalAccessDelegate internalDispatcher) {}

    @VisibleForTesting
    @Override
    public void setPopupZoomerForTest(PopupZoomer popupZoomer) {}

    @Override
    public void destroy() {}

    @Override
    public boolean isAlive() {
        return false;
    }

    @Override
    public int getViewportWidthPix() {
        return 0;
    }

    @Override
    public int getViewportHeightPix() {
        return 0;
    }

    @Override
    public boolean isScrollInProgress() {
        return false;
    }

    @Override
    public void sendDoubleTapForTest(long timeMs, int x, int y) {}

    @Override
    public void flingViewport(long timeMs, float velocityX, float velocityY, boolean fromGamepad) {}

    @Override
    public void cancelFling(long timeMs) {}

    @Override
    public void onShow() {}

    @Override
    public int getCurrentRenderProcessId() {
        return 0;
    }

    @Override
    public void onHide() {}

    @Override
    public boolean isAttachedToWindow() {
        return false;
    }

    @Override
    public void onAttachedToWindow() {}

    @Override
    public void onDetachedFromWindow() {}

    @Override
    public void onConfigurationChanged(Configuration newConfig) {}

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return false;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return false;
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {}

    @Override
    public void scrollTo(float xPix, float yPix) {}

    @Override
    public void updateTextSelectionUI(boolean focused) {}

    @Override
    public void onPause() {}

    @Override
    public void onResume() {}

    @Override
    public void onFocusChanged(boolean gainFocus, boolean hideKeyboardOnBlur) {}

    @Override
    public void setCurrentTouchEventOffsets(float dx, float dy) {}

    @Override
    public void scrollBy(float dxPix, float dyPix) {}

    @Override
    public int computeHorizontalScrollOffset() {
        return 0;
    }

    @Override
    public int computeVerticalScrollOffset() {
        return 0;
    }

    @Override
    public int computeHorizontalScrollRange() {
        return 0;
    }

    @Override
    public int computeVerticalScrollRange() {
        return 0;
    }

    @Override
    public int computeHorizontalScrollExtent() {
        return 0;
    }

    @Override
    public int computeVerticalScrollExtent() {
        return 0;
    }

    @Override
    public boolean awakenScrollBars(int startDelay, boolean invalidate) {
        return false;
    }

    @Override
    public void updateMultiTouchZoomSupport(boolean supportsMultiTouchZoom) {}

    @Override
    public void updateDoubleTapSupport(boolean supportsDoubleTap) {}

    @Override
    public void selectPopupMenuItems(int[] indices) {}

    @Override
    public void preserveSelectionOnNextLossOfFocus() {}

    @Override
    public SelectPopup getSelectPopupForTest() {
        return null;
    }

    @Override
    public boolean supportsAccessibilityAction(int action) {
        return false;
    }

    @Override
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        return false;
    }

    @Override
    public WebContentsAccessibility getWebContentsAccessibility() {
        return null;
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        return null;
    }

    @Override
    public void onProvideVirtualStructure(ViewStructure structure, boolean ignoreScrollOffset) {}

    @Override
    public void setObscuredByAnotherView(boolean isObscured) {}

    @Override
    public boolean isTouchExplorationEnabled() {
        return false;
    }

    @Override
    public void setAccessibilityState(boolean state) {}

    @Override
    public void setShouldSetAccessibilityFocusOnPageLoad(boolean on) {}

    @Override
    public boolean getIsMobileOptimizedHint() {
        return false;
    }

    @Override
    public void setBackgroundOpaque(boolean opaque) {}

    @Override
    public void setFullscreenRequiredForOrientationLock(boolean value) {}

    @Override
    public int getTopControlsShrinkBlinkHeightForTesting() {
        return 0;
    }
}
