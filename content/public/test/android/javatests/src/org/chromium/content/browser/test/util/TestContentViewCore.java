// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.ActionMode;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.textclassifier.TextClassifier;

import org.chromium.base.VisibleForTesting;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewCore.InternalAccessDelegate;
import org.chromium.content.browser.PopupZoomer;
import org.chromium.content.browser.SelectionPopupController;
import org.chromium.content.browser.WindowAndroidChangedObserver;
import org.chromium.content.browser.accessibility.WebContentsAccessibility;
import org.chromium.content.browser.input.ImeAdapter;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.input.TextSuggestionHost;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.SelectionClient;
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
    public SelectionPopupController getSelectionPopupControllerForTesting() {
        return null;
    }

    @Override
    public void setSelectionPopupControllerForTesting(SelectionPopupController actionMode) {}

    @Override
    public TextSuggestionHost getTextSuggestionHostForTesting() {
        return null;
    }

    @Override
    public void setTextSuggestionHostForTesting(TextSuggestionHost textSuggestionHost) {}

    @Override
    public void addWindowAndroidChangedObserver(WindowAndroidChangedObserver observer) {}

    @Override
    public void removeWindowAndroidChangedObserver(WindowAndroidChangedObserver observer) {}

    @Override
    public void addImeEventObserver(ImeEventObserver imeEventObserver) {}

    @Override
    public void setImeAdapterForTest(ImeAdapter imeAdapter) {}

    @VisibleForTesting
    @Override
    public ImeAdapter getImeAdapterForTest() {
        return null;
    }

    @Override
    public void initialize(ViewAndroidDelegate viewDelegate,
            InternalAccessDelegate internalDispatcher, WebContents webContents,
            WindowAndroid windowAndroid) {}

    @Override
    public void updateWindowAndroid(WindowAndroid windowAndroid) {}

    @Override
    public void setActionModeCallback(ActionMode.Callback callback) {}

    @Override
    public void setNonSelectionActionModeCallback(ActionMode.Callback callback) {}

    @Override
    public SelectionClient.ResultCallback getPopupControllerResultCallback() {
        return null;
    }

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
    public float getMouseWheelTickMultiplier() {
        return 0.f;
    }

    @Override
    public String getSelectedText() {
        return null;
    }

    @Override
    public boolean isFocusedNodeEditable() {
        return false;
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
    public void addGestureStateListener(GestureStateListener listener) {}

    @Override
    public void removeGestureStateListener(GestureStateListener listener) {}

    @Override
    public void onShow() {}

    @Override
    public int getCurrentRenderProcessId() {
        return 0;
    }

    @Override
    public void onHide() {}

    @Override
    public void destroySelectActionMode() {}

    @Override
    public boolean isSelectActionBarShowing() {
        return false;
    }

    @Override
    public boolean isAttachedToWindow() {
        return false;
    }

    @Override
    public void onAttachedToWindow() {}

    @Override
    public void onDetachedFromWindow() {}

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return null;
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return false;
    }

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
    public ActionModeCallbackHelper getActionModeCallbackHelper() {
        return null;
    }

    @Override
    public void clearSelection() {}

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
    public void onReceivedProcessTextResult(int resultCode, Intent data) {}

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
    public void setSelectionClient(SelectionClient selectionClient) {}

    @Override
    public void setTextClassifier(TextClassifier textClassifier) {}

    @Override
    public TextClassifier getTextClassifier() {
        return null;
    }

    @Override
    public TextClassifier getCustomTextClassifier() {
        return null;
    }

    @Override
    public int getTopControlsShrinkBlinkHeightForTesting() {
        return 0;
    }
}
