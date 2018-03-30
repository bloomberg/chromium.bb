// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.input.TextSuggestionHost;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsUserData;
import org.chromium.content.browser.webcontents.WebContentsUserData.UserDataFactory;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.ContentViewCore.InternalAccessDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.gamepad.GamepadList;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;

/**
 * Implementation of the interface {@ContentViewCore}.
 */
@JNINamespace("content")
public class ContentViewCoreImpl implements ContentViewCore, DisplayAndroidObserver {
    private static final String TAG = "cr_ContentViewCore";

    private Context mContext;
    private final ObserverList<WindowEventObserver> mWindowEventObservers = new ObserverList<>();

    private ViewGroup mContainerView;
    private InternalAccessDelegate mContainerViewInternals;
    private WebContentsImpl mWebContents;
    private WindowAndroid mWindowAndroid;

    // Native pointer to C++ ContentViewCore object which will be set by nativeInit().
    private long mNativeContentViewCore;

    private boolean mAttachedToWindow;

    // Cached copy of all positions and scales as reported by the renderer.
    private RenderCoordinates mRenderCoordinates;

    /**
     * PID used to indicate an invalid render process.
     */
    // Keep in sync with the value returned from ContentViewCore::GetCurrentRendererProcessId()
    // if there is no render process.
    public static final int INVALID_RENDER_PROCESS_PID = 0;

    // A ViewAndroidDelegate that delegates to the current container view.
    private ViewAndroidDelegate mViewAndroidDelegate;

    // TODO(mthiesse): Clean up the focus code here, this boolean doesn't actually reflect view
    // focus. It reflects a combination of both view focus and resumed state.
    private Boolean mHasViewFocus;

    // This is used in place of window focus, as we can't actually use window focus due to issues
    // where content expects to be focused while a popup steals window focus.
    // See https://crbug.com/686232 for more context.
    private boolean mPaused;

    // The list of observers that are notified when ContentViewCore changes its WindowAndroid.
    private final ObserverList<WindowAndroidChangedObserver> mWindowAndroidChangedObservers =
            new ObserverList<>();

    private boolean mInitialized;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<ContentViewCoreImpl> INSTANCE =
                ContentViewCoreImpl::new;
    }

    /**
     * @param webContents The {@link WebContents} to find a {@link ContentViewCore} of.
     * @return            A {@link ContentViewCore} that is connected to {@code webContents} or
     *                    {@code null} if none exists.
     */
    public static ContentViewCoreImpl fromWebContents(WebContents webContents) {
        return WebContentsUserData.fromWebContents(webContents, ContentViewCoreImpl.class, null);
    }

    /**
     * Constructs a new ContentViewCore.
     *
     * @param webContents {@link WebContents} to be associated with this object.
     */
    public ContentViewCoreImpl(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
    }

    public Context getContext() {
        return mContext;
    }

    @Override
    public ViewGroup getContainerView() {
        return mContainerView;
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    private WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    /**
     * Add {@link WindowAndroidChangeObserver} object.
     * @param observer Observer instance to add.
     */
    public void addWindowAndroidChangedObserver(WindowAndroidChangedObserver observer) {
        mWindowAndroidChangedObservers.addObserver(observer);
    }

    /**
     * Remove {@link WindowAndroidChangeObserver} object.
     * @param observer Observer instance to remove.
     */
    public void removeWindowAndroidChangedObserver(WindowAndroidChangedObserver observer) {
        mWindowAndroidChangedObservers.removeObserver(observer);
    }

    public static ContentViewCoreImpl create(Context context, String productVersion,
            WebContents webContents, ViewAndroidDelegate viewDelegate,
            InternalAccessDelegate internalDispatcher, WindowAndroid windowAndroid) {
        ContentViewCoreImpl core = WebContentsUserData.fromWebContents(
                webContents, ContentViewCoreImpl.class, UserDataFactoryLazyHolder.INSTANCE);
        assert core != null;
        assert !core.initialized();
        core.initialize(context, productVersion, viewDelegate, internalDispatcher, windowAndroid);
        return core;
    }

    private void initialize(Context context, String productVersion,
            ViewAndroidDelegate viewDelegate, InternalAccessDelegate internalDispatcher,
            WindowAndroid windowAndroid) {
        mContext = context;

        mViewAndroidDelegate = viewDelegate;
        mWindowAndroid = windowAndroid;
        final float dipScale = windowAndroid.getDisplay().getDipScale();

        mNativeContentViewCore =
                nativeInit(mWebContents, mViewAndroidDelegate, windowAndroid, dipScale);
        ViewGroup containerView = viewDelegate.getContainerView();
        SelectionPopupControllerImpl controller = SelectionPopupControllerImpl.create(
                mContext, windowAndroid, mWebContents, containerView);
        controller.setActionModeCallback(ActionModeCallbackHelper.EMPTY_CALLBACK);
        addWindowAndroidChangedObserver(controller);

        setContainerView(containerView);
        mRenderCoordinates = mWebContents.getRenderCoordinates();
        mRenderCoordinates.setDeviceScaleFactor(dipScale);
        WebContentsAccessibilityImpl wcax = WebContentsAccessibilityImpl.create(
                mContext, containerView, mWebContents, productVersion);
        setContainerViewInternals(internalDispatcher);

        ImeAdapterImpl imeAdapter = ImeAdapterImpl.create(mWebContents, mContainerView,
                ImeAdapterImpl.createDefaultInputMethodManagerWrapper(mContext));
        imeAdapter.addEventObserver(controller);
        imeAdapter.addEventObserver(getJoystick());
        imeAdapter.addEventObserver(TapDisambiguator.create(mContext, mWebContents, containerView));
        TextSuggestionHost textSuggestionHost =
                TextSuggestionHost.create(mContext, mWebContents, windowAndroid, containerView);
        addWindowAndroidChangedObserver(textSuggestionHost);

        SelectPopup.create(mContext, mWebContents, containerView);

        mWindowEventObservers.addObserver(controller);
        mWindowEventObservers.addObserver(getGestureListenerManager());
        mWindowEventObservers.addObserver(textSuggestionHost);
        mWindowEventObservers.addObserver(imeAdapter);
        mWindowEventObservers.addObserver(wcax);
        mInitialized = true;
    }

    public boolean initialized() {
        return mInitialized;
    }

    @Override
    public void updateWindowAndroid(WindowAndroid windowAndroid) {
        removeDisplayAndroidObserver();
        mWindowAndroid = windowAndroid;
        nativeUpdateWindowAndroid(mNativeContentViewCore, windowAndroid);

        // TODO(yusufo): Rename this call to be general for tab reparenting.
        // Clean up cached popups that may have been created with an old activity.
        getSelectPopup().close();
        destroyPastePopup();

        addDisplayAndroidObserverIfNeeded();

        for (WindowAndroidChangedObserver observer : mWindowAndroidChangedObservers) {
            observer.onWindowAndroidChanged(windowAndroid);
        }
    }

    private EventForwarder getEventForwarder() {
        return getWebContents().getEventForwarder();
    }

    private void addDisplayAndroidObserverIfNeeded() {
        if (!mAttachedToWindow) return;
        WindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid != null) {
            DisplayAndroid display = windowAndroid.getDisplay();
            display.addObserver(this);
            onRotationChanged(display.getRotation());
            onDIPScaleChanged(display.getDipScale());
        }
    }

    private void removeDisplayAndroidObserver() {
        WindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid != null) {
            windowAndroid.getDisplay().removeObserver(this);
        }
    }

    @Override
    public void setContainerView(ViewGroup containerView) {
        try {
            TraceEvent.begin("ContentViewCore.setContainerView");
            if (mContainerView != null) {
                getSelectPopup().hide();
                getImeAdapter().setContainerView(containerView);
                getTextSuggestionHost().setContainerView(containerView);
                getSelectPopup().setContainerView(containerView);
            }

            mContainerView = containerView;
            mContainerView.setClickable(true);
            getSelectionPopupController().setContainerView(containerView);
            getGestureListenerManager().setContainerView(containerView);
        } finally {
            TraceEvent.end("ContentViewCore.setContainerView");
        }
    }

    private SelectionPopupControllerImpl getSelectionPopupController() {
        return SelectionPopupControllerImpl.fromWebContents(mWebContents);
    }

    private GestureListenerManagerImpl getGestureListenerManager() {
        return GestureListenerManagerImpl.fromWebContents(mWebContents);
    }

    private ImeAdapterImpl getImeAdapter() {
        return ImeAdapterImpl.fromWebContents(mWebContents);
    }

    private TapDisambiguator getTapDisambiguator() {
        return TapDisambiguator.fromWebContents(mWebContents);
    }

    private WebContentsAccessibilityImpl getWebContentsAccessibility() {
        return WebContentsAccessibilityImpl.fromWebContents(mWebContents);
    }

    private TextSuggestionHost getTextSuggestionHost() {
        return TextSuggestionHost.fromWebContents(mWebContents);
    }

    private SelectPopup getSelectPopup() {
        return SelectPopup.fromWebContents(mWebContents);
    }

    @CalledByNative
    private void onNativeContentViewCoreDestroyed(long nativeContentViewCore) {
        assert nativeContentViewCore == mNativeContentViewCore;
        mNativeContentViewCore = 0;
    }

    @Override
    public void setContainerViewInternals(InternalAccessDelegate internalDispatcher) {
        mContainerViewInternals = internalDispatcher;
        getGestureListenerManager().setScrollDelegate(internalDispatcher);
    }

    @Override
    public void destroy() {
        removeDisplayAndroidObserver();
        if (mNativeContentViewCore != 0) {
            nativeOnJavaContentViewCoreDestroyed(mNativeContentViewCore);
        }
        ImeAdapterImpl imeAdapter = getImeAdapter();
        imeAdapter.resetAndHideKeyboard();
        imeAdapter.removeEventObserver(getSelectionPopupController());
        imeAdapter.removeEventObserver(getJoystick());
        imeAdapter.removeEventObserver(getTapDisambiguator());
        getGestureListenerManager().reset();
        removeWindowAndroidChangedObserver(getTextSuggestionHost());
        mWindowEventObservers.clear();
        hidePopupsAndPreserveSelection();
        mWebContents = null;
        mNativeContentViewCore = 0;

        // See warning in javadoc before adding more clean up code here.
    }

    @Override
    public boolean isAlive() {
        return mNativeContentViewCore != 0;
    }

    @VisibleForTesting
    @Override
    public int getTopControlsShrinkBlinkHeightForTesting() {
        // TODO(jinsukkim): Let callsites provide with its own top controls height to remove
        //                  the test-only method in content layer.
        if (mNativeContentViewCore == 0) return 0;
        return nativeGetTopControlsShrinkBlinkHeightPixForTesting(mNativeContentViewCore);
    }

    private void hidePopupsAndClearSelection() {
        getSelectionPopupController().destroyActionModeAndUnselect();
        mWebContents.dismissTextHandles();
        PopupController.hideAll(mWebContents);
    }

    private void hidePopupsAndPreserveSelection() {
        getSelectionPopupController().hidePopupsAndPreserveSelection();
    }

    private void resetGestureDetection() {
        if (mNativeContentViewCore == 0) return;
        nativeResetGestureDetection(mNativeContentViewCore);
    }

    @SuppressWarnings("javadoc")
    @Override
    public void onAttachedToWindow() {
        mAttachedToWindow = true;
        for (WindowEventObserver observer : mWindowEventObservers) observer.onAttachedToWindow();
        addDisplayAndroidObserverIfNeeded();
        GamepadList.onAttachedToWindow(mContext);
    }

    @SuppressWarnings("javadoc")
    @SuppressLint("MissingSuperCall")
    @Override
    public void onDetachedFromWindow() {
        mAttachedToWindow = false;
        for (WindowEventObserver observer : mWindowEventObservers) observer.onDetachedFromWindow();
        removeDisplayAndroidObserver();
        GamepadList.onDetachedFromWindow();
    }

    @SuppressWarnings("javadoc")
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        try {
            TraceEvent.begin("ContentViewCore.onConfigurationChanged");
            getImeAdapter().onKeyboardConfigurationChanged(newConfig);
            mContainerViewInternals.super_onConfigurationChanged(newConfig);
            // To request layout has side effect, but it seems OK as it only happen in
            // onConfigurationChange and layout has to be changed in most case.
            mContainerView.requestLayout();
        } finally {
            TraceEvent.end("ContentViewCore.onConfigurationChanged");
        }
    }

    @Override
    public void onPause() {
        mPaused = true;
        onFocusChanged(false, true);
    }

    @Override
    public void onResume() {
        mPaused = false;
        onFocusChanged(ViewUtils.hasFocus(getContainerView()), true);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (!hasWindowFocus) resetGestureDetection();
        for (WindowEventObserver observer : mWindowEventObservers) {
            observer.onWindowFocusChanged(hasWindowFocus);
        }
    }

    @Override
    public void onFocusChanged(boolean gainFocus, boolean hideKeyboardOnBlur) {
        if (mHasViewFocus != null && mHasViewFocus == gainFocus) return;
        // TODO(mthiesse): Clean this up. mHasViewFocus isn't view focus really, it's a combination
        // of view focus and resumed state.
        if (gainFocus && mPaused) return;
        mHasViewFocus = gainFocus;

        if (mWebContents == null) {
            // CVC is on its way to destruction. The rest needs not running as all the states
            // will be discarded, or WebContentsUserData-based objects are not reachable
            // any more. Simply return here.
            return;
        }

        getImeAdapter().onViewFocusChanged(gainFocus, hideKeyboardOnBlur);
        getJoystick().setScrollEnabled(
                gainFocus && !getSelectionPopupController().isFocusedNodeEditable());

        SelectionPopupControllerImpl controller = getSelectionPopupController();
        if (gainFocus) {
            controller.restoreSelectionPopupsIfNecessary();
        } else {
            getImeAdapter().cancelRequestToScrollFocusedEditableNodeIntoView();
            if (controller.getPreserveSelectionOnNextLossOfFocus()) {
                controller.setPreserveSelectionOnNextLossOfFocus(false);
                hidePopupsAndPreserveSelection();
            } else {
                hidePopupsAndClearSelection();
                // Clear the selection. The selection is cleared on destroying IME
                // and also here since we may receive destroy first, for example
                // when focus is lost in webview.
                controller.clearSelection();
            }
        }
        if (mNativeContentViewCore != 0) nativeSetFocus(mNativeContentViewCore, gainFocus);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        TapDisambiguator tapDisambiguator = getTapDisambiguator();
        if (tapDisambiguator.isShowing() && keyCode == KeyEvent.KEYCODE_BACK) {
            tapDisambiguator.backButtonPressed();
            return true;
        }
        return mContainerViewInternals.super_onKeyUp(keyCode, event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (GamepadList.dispatchKeyEvent(event)) return true;
        if (!shouldPropagateKeyEvent(event)) {
            return mContainerViewInternals.super_dispatchKeyEvent(event);
        }

        if (getImeAdapter().dispatchKeyEvent(event)) return true;

        return mContainerViewInternals.super_dispatchKeyEvent(event);
    }

    /**
     * Check whether a key should be propagated to the embedder or not.
     * We need to send almost every key to Blink. However:
     * 1. We don't want to block the device on the renderer for
     * some keys like menu, home, call.
     * 2. There are no WebKit equivalents for some of these keys
     * (see app/keyboard_codes_win.h)
     * Note that these are not the same set as KeyEvent.isSystemKey:
     * for instance, AKEYCODE_MEDIA_* will be dispatched to webkit*.
     */
    private static boolean shouldPropagateKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_HOME
                || keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_CALL
                || keyCode == KeyEvent.KEYCODE_ENDCALL || keyCode == KeyEvent.KEYCODE_POWER
                || keyCode == KeyEvent.KEYCODE_HEADSETHOOK || keyCode == KeyEvent.KEYCODE_CAMERA
                || keyCode == KeyEvent.KEYCODE_FOCUS || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
                || keyCode == KeyEvent.KEYCODE_VOLUME_MUTE
                || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            return false;
        }
        return true;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (GamepadList.onGenericMotionEvent(event)) return true;
        if (getJoystick().onGenericMotionEvent(event)) return true;
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_SCROLL:
                    getEventForwarder().onMouseWheelEvent(event.getEventTime(), event.getX(),
                            event.getY(), event.getAxisValue(MotionEvent.AXIS_HSCROLL),
                            event.getAxisValue(MotionEvent.AXIS_VSCROLL));
                    return true;
                case MotionEvent.ACTION_BUTTON_PRESS:
                case MotionEvent.ACTION_BUTTON_RELEASE:
                    // TODO(mustaq): Should we include MotionEvent.TOOL_TYPE_STYLUS here?
                    // crbug.com/592082
                    if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
                        return getEventForwarder().onMouseEvent(event);
                    }
            }
        }
        return mContainerViewInternals.super_onGenericMotionEvent(event);
    }

    private JoystickHandler getJoystick() {
        return JoystickHandler.fromWebContents(mWebContents);
    }

    @Override
    public void scrollBy(float dxPix, float dyPix) {
        if (dxPix == 0 && dyPix == 0) return;
        long time = SystemClock.uptimeMillis();
        // It's a very real (and valid) possibility that a fling may still
        // be active when programatically scrolling. Cancelling the fling in
        // such cases ensures a consistent gesture event stream.
        if (getGestureListenerManager().hasPotentiallyActiveFling()) {
            getEventForwarder().cancelFling(time);
        }
        getEventForwarder().scroll(time, dxPix, dyPix);
    }

    @Override
    public void scrollTo(float xPix, float yPix) {
        final float xCurrentPix = mRenderCoordinates.getScrollXPix();
        final float yCurrentPix = mRenderCoordinates.getScrollYPix();
        final float dxPix = xPix - xCurrentPix;
        final float dyPix = yPix - yCurrentPix;
        scrollBy(dxPix, dyPix);
    }

    @SuppressWarnings("javadoc")
    @Override
    public int computeHorizontalScrollExtent() {
        return mRenderCoordinates.getLastFrameViewportWidthPixInt();
    }

    @SuppressWarnings("javadoc")
    @Override
    public int computeHorizontalScrollOffset() {
        return mRenderCoordinates.getScrollXPixInt();
    }

    @SuppressWarnings("javadoc")
    @Override
    public int computeHorizontalScrollRange() {
        return mRenderCoordinates.getContentWidthPixInt();
    }

    @SuppressWarnings("javadoc")
    @Override
    public int computeVerticalScrollExtent() {
        return mRenderCoordinates.getLastFrameViewportHeightPixInt();
    }

    @SuppressWarnings("javadoc")
    @Override
    public int computeVerticalScrollOffset() {
        return mRenderCoordinates.getScrollYPixInt();
    }

    @SuppressWarnings("javadoc")
    @Override
    public int computeVerticalScrollRange() {
        return mRenderCoordinates.getContentHeightPixInt();
    }

    // End FrameLayout overrides.

    @SuppressWarnings("javadoc")
    @Override
    public boolean awakenScrollBars(int startDelay, boolean invalidate) {
        // For the default implementation of ContentView which draws the scrollBars on the native
        // side, calling this function may get us into a bad state where we keep drawing the
        // scrollBars, so disable it by always returning false.
        if (mContainerView.getScrollBarStyle() == View.SCROLLBARS_INSIDE_OVERLAY) {
            return false;
        } else {
            return mContainerViewInternals.super_awakenScrollBars(startDelay, invalidate);
        }
    }

    @Override
    public void updateMultiTouchZoomSupport(boolean supportsMultiTouchZoom) {
        if (mNativeContentViewCore == 0) return;
        nativeSetMultiTouchZoomSupportEnabled(mNativeContentViewCore, supportsMultiTouchZoom);
    }

    @Override
    public void updateDoubleTapSupport(boolean supportsDoubleTap) {
        if (mNativeContentViewCore == 0) return;
        nativeSetDoubleTapSupportEnabled(mNativeContentViewCore, supportsDoubleTap);
    }

    /**
     * Send the screen orientation value to the renderer.
     */
    @VisibleForTesting
    private void sendOrientationChangeEvent(int orientation) {
        if (mNativeContentViewCore == 0) return;

        nativeSendOrientationChangeEvent(mNativeContentViewCore, orientation);
    }

    @VisibleForTesting
    @Override
    public boolean isSelectPopupVisibleForTest() {
        return getSelectPopup().isVisibleForTesting();
    }

    private void destroyPastePopup() {
        getSelectionPopupController().destroyPastePopup();
    }

    // DisplayAndroidObserver method.
    @Override
    public void onRotationChanged(int rotation) {
        // ActionMode#invalidate() won't be able to re-layout the floating
        // action mode menu items according to the new rotation. So Chrome
        // has to re-create the action mode.
        if (mWebContents != null) {
            SelectionPopupControllerImpl controller = getSelectionPopupController();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && controller != null
                    && controller.isActionModeValid()) {
                hidePopupsAndPreserveSelection();
                controller.showActionModeOrClearOnFailure();
            }
            TextSuggestionHost host = getTextSuggestionHost();
            if (host != null) host.hidePopups();
        }

        int rotationDegrees = 0;
        switch (rotation) {
            case Surface.ROTATION_0:
                rotationDegrees = 0;
                break;
            case Surface.ROTATION_90:
                rotationDegrees = 90;
                break;
            case Surface.ROTATION_180:
                rotationDegrees = 180;
                break;
            case Surface.ROTATION_270:
                rotationDegrees = -90;
                break;
            default:
                throw new IllegalStateException(
                        "Display.getRotation() shouldn't return that value");
        }

        sendOrientationChangeEvent(rotationDegrees);
    }

    // DisplayAndroidObserver method.
    @Override
    public void onDIPScaleChanged(float dipScale) {
        WindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid == null || mNativeContentViewCore == 0) return;

        mRenderCoordinates.setDeviceScaleFactor(dipScale);
        nativeSetDIPScale(mNativeContentViewCore, dipScale);
    }

    private native long nativeInit(WebContents webContents, ViewAndroidDelegate viewAndroidDelegate,
            WindowAndroid window, float dipScale);
    private native void nativeUpdateWindowAndroid(long nativeContentViewCore, WindowAndroid window);
    private native void nativeOnJavaContentViewCoreDestroyed(long nativeContentViewCore);
    private native void nativeSetFocus(long nativeContentViewCore, boolean focused);
    private native void nativeSetDIPScale(long nativeContentViewCore, float dipScale);
    private native int nativeGetTopControlsShrinkBlinkHeightPixForTesting(
            long nativeContentViewCore);
    private native void nativeSendOrientationChangeEvent(
            long nativeContentViewCore, int orientation);
    private native void nativeResetGestureDetection(long nativeContentViewCore);
    private native void nativeSetDoubleTapSupportEnabled(
            long nativeContentViewCore, boolean enabled);
    private native void nativeSetMultiTouchZoomSupportEnabled(
            long nativeContentViewCore, boolean enabled);
}
