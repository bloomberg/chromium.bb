// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.view.Surface;
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
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.ContentViewCore.InternalAccessDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.device.gamepad.GamepadList;
import org.chromium.ui.base.ViewAndroidDelegate;
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

    private InternalAccessDelegate mContainerViewInternals;
    private WebContentsImpl mWebContents;
    private WindowAndroid mWindowAndroid;

    // Native pointer to C++ ContentViewCore object which will be set by nativeInit().
    private long mNativeContentViewCore;

    private boolean mAttachedToWindow;

    // Cached copy of all positions and scales as reported by the renderer.
    private RenderCoordinatesImpl mRenderCoordinates;

    /**
     * PID used to indicate an invalid render process.
     */
    // Keep in sync with the value returned from ContentViewCore::GetCurrentRendererProcessId()
    // if there is no render process.
    public static final int INVALID_RENDER_PROCESS_PID = 0;

    // Whether the container view has view-level focus.
    private Boolean mHasViewFocus;

    // This is used in place of window focus on the container view, as we can't actually use window
    // focus due to issues where content expects to be focused while a popup steals window focus.
    // See https://crbug.com/686232 for more context.
    private boolean mPaused;

    // Whether we consider this CVC to have input focus. This is computed through mHasViewFocus and
    // mPaused. See the comments on mPaused for how this doesn't exactly match Android's notion of
    // input focus and why we need to do this.
    private Boolean mHasInputFocus;
    private boolean mHideKeyboardOnBlur;

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
        return webContents.getOrSetUserData(ContentViewCoreImpl.class, null);
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
        ContentViewCoreImpl core = webContents.getOrSetUserData(
                ContentViewCoreImpl.class, UserDataFactoryLazyHolder.INSTANCE);
        assert core != null;
        assert !core.initialized();
        core.initialize(context, productVersion, viewDelegate, internalDispatcher, windowAndroid);
        return core;
    }

    private void initialize(Context context, String productVersion,
            ViewAndroidDelegate viewDelegate, InternalAccessDelegate internalDispatcher,
            WindowAndroid windowAndroid) {
        mContext = context;

        mWindowAndroid = windowAndroid;
        mWebContents.setViewAndroidDelegate(viewDelegate);
        final float dipScale = windowAndroid.getDisplay().getDipScale();

        mNativeContentViewCore = nativeInit(mWebContents, viewDelegate, windowAndroid, dipScale);
        SelectionPopupControllerImpl controller =
                SelectionPopupControllerImpl.create(mContext, windowAndroid, mWebContents);
        controller.setActionModeCallback(ActionModeCallbackHelper.EMPTY_CALLBACK);
        addWindowAndroidChangedObserver(controller);

        mRenderCoordinates = mWebContents.getRenderCoordinates();
        mRenderCoordinates.setDeviceScaleFactor(dipScale);

        ViewGroup containerView = viewDelegate.getContainerView();
        WebContentsAccessibilityImpl wcax = WebContentsAccessibilityImpl.create(
                mContext, containerView, mWebContents, productVersion);
        setContainerViewInternals(internalDispatcher);

        ImeAdapterImpl imeAdapter = ImeAdapterImpl.create(
                mWebContents, ImeAdapterImpl.createDefaultInputMethodManagerWrapper(mContext));
        imeAdapter.addEventObserver(controller);
        imeAdapter.addEventObserver(getJoystick());
        imeAdapter.addEventObserver(TapDisambiguator.create(mContext, mWebContents, containerView));
        TextSuggestionHost textSuggestionHost =
                TextSuggestionHost.create(mContext, mWebContents, windowAndroid);
        addWindowAndroidChangedObserver(textSuggestionHost);

        SelectPopup.create(mContext, mWebContents);

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
        ContentUiEventHandler.fromWebContents(mWebContents).setEventDelegate(internalDispatcher);
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

    private void hidePopupsAndClearSelection() {
        getSelectionPopupController().destroyActionModeAndUnselect();
        mWebContents.dismissTextHandles();
        PopupController.hideAll(mWebContents);
    }

    private void hidePopupsAndPreserveSelection() {
        getSelectionPopupController().hidePopupsAndPreserveSelection();
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
            ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
            if (viewDelegate != null) viewDelegate.getContainerView().requestLayout();
        } finally {
            TraceEvent.end("ContentViewCore.onConfigurationChanged");
        }
    }

    @Override
    public void onPause() {
        if (mPaused) return;
        mPaused = true;
        onFocusChanged();
    }

    @Override
    public void onResume() {
        if (!mPaused) return;
        mPaused = false;
        onFocusChanged();
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (!hasWindowFocus) getGestureListenerManager().resetGestureDetection();
        for (WindowEventObserver observer : mWindowEventObservers) {
            observer.onWindowFocusChanged(hasWindowFocus);
        }
    }

    @Override
    public void onViewFocusChanged(boolean gainFocus) {
        if (mHasViewFocus != null && mHasViewFocus == gainFocus) return;
        mHasViewFocus = gainFocus;
        onFocusChanged();
    }

    private void onFocusChanged() {
        // Wait for view focus to be set before propagating focus changes.
        if (mHasViewFocus == null) return;

        // See the comments on mPaused for why we use it to compute input focus.
        boolean hasInputFocus = mHasViewFocus && !mPaused;
        if (mHasInputFocus != null && mHasInputFocus == hasInputFocus) return;
        mHasInputFocus = hasInputFocus;

        if (mWebContents == null) {
            // CVC is on its way to destruction. The rest needs not running as all the states
            // will be discarded, or WebContentsUserData-based objects are not reachable
            // any more. Simply return here.
            return;
        }

        getImeAdapter().onViewFocusChanged(mHasInputFocus, mHideKeyboardOnBlur);
        getJoystick().setScrollEnabled(
                mHasInputFocus && !getSelectionPopupController().isFocusedNodeEditable());

        SelectionPopupControllerImpl controller = getSelectionPopupController();
        if (mHasInputFocus) {
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
        if (mNativeContentViewCore != 0) nativeSetFocus(mNativeContentViewCore, mHasInputFocus);
    }

    @Override
    public void setHideKeyboardOnBlur(boolean hideKeyboardOnBlur) {
        mHideKeyboardOnBlur = hideKeyboardOnBlur;
    }

    private JoystickHandler getJoystick() {
        return JoystickHandler.fromWebContents(mWebContents);
    }

    // End FrameLayout overrides.

    /**
     * Send the screen orientation value to the renderer.
     */
    @VisibleForTesting
    private void sendOrientationChangeEvent(int orientation) {
        if (mNativeContentViewCore == 0) return;

        nativeSendOrientationChangeEvent(mNativeContentViewCore, orientation);
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
    private native void nativeSendOrientationChangeEvent(
            long nativeContentViewCore, int orientation);
}
