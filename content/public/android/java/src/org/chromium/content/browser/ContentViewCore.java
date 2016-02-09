// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.SearchManager;
import android.app.assist.AssistStructure.ViewNode;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import android.os.SystemClock;
import android.provider.Browser;
import android.text.TextUtils;
import android.util.Pair;
import android.util.TypedValue;
import android.view.ActionMode;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.R;
import org.chromium.content.browser.ScreenOrientationListener.ScreenOrientationObserver;
import org.chromium.content.browser.accessibility.BrowserAccessibilityManager;
import org.chromium.content.browser.accessibility.captioning.CaptioningBridgeFactory;
import org.chromium.content.browser.accessibility.captioning.SystemCaptioningBridge;
import org.chromium.content.browser.accessibility.captioning.TextTrackSettings;
import org.chromium.content.browser.input.FloatingPastePopupMenu;
import org.chromium.content.browser.input.GamepadList;
import org.chromium.content.browser.input.ImeAdapter;
import org.chromium.content.browser.input.InputMethodManagerWrapper;
import org.chromium.content.browser.input.JoystickScrollProvider;
import org.chromium.content.browser.input.LegacyPastePopupMenu;
import org.chromium.content.browser.input.PastePopupMenu;
import org.chromium.content.browser.input.PastePopupMenu.PastePopupMenuDelegate;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.input.SelectPopupDialog;
import org.chromium.content.browser.input.SelectPopupDropdown;
import org.chromium.content.browser.input.SelectPopupItem;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.AccessibilitySnapshotNode;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.ime.TextInputType;
import org.chromium.ui.gfx.DeviceDisplayInfo;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.lang.annotation.Annotation;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Provides a Java-side 'wrapper' around a WebContent (native) instance.
 * Contains all the major functionality necessary to manage the lifecycle of a ContentView without
 * being tied to the view system.
 */
@JNINamespace("content")
public class ContentViewCore implements AccessibilityStateChangeListener, ScreenOrientationObserver,
                                        SystemCaptioningBridge.SystemCaptioningBridgeListener {
    private static final String TAG = "cr.ContentViewCore";

    // Used to avoid enabling zooming in / out if resulting zooming will
    // produce little visible difference.
    private static final float ZOOM_CONTROLS_EPSILON = 0.007f;

    private static final ZoomControlsDelegate NO_OP_ZOOM_CONTROLS_DELEGATE =
            new ZoomControlsDelegate() {
        @Override
        public void invokeZoomPicker() {}
        @Override
        public void dismissZoomPicker() {}
        @Override
        public void updateZoomControls() {}
    };

    // If the embedder adds a JavaScript interface object that contains an indirect reference to
    // the ContentViewCore, then storing a strong ref to the interface object on the native
    // side would prevent garbage collection of the ContentViewCore (as that strong ref would
    // create a new GC root).
    // For that reason, we store only a weak reference to the interface object on the
    // native side. However we still need a strong reference on the Java side to
    // prevent garbage collection if the embedder doesn't maintain their own ref to the
    // interface object - the Java side ref won't create a new GC root.
    // This map stores those references. We put into the map on addJavaScriptInterface()
    // and remove from it in removeJavaScriptInterface(). The annotation class is stored for
    // the purpose of migrating injected objects from one instance of CVC to another, which
    // is used by Android WebView to support WebChromeClient.onCreateWindow scenario.
    private final Map<String, Pair<Object, Class>> mJavaScriptInterfaces =
            new HashMap<String, Pair<Object, Class>>();

    // Additionally, we keep track of all Java bound JS objects that are in use on the
    // current page to ensure that they are not garbage collected until the page is
    // navigated. This includes interface objects that have been removed
    // via the removeJavaScriptInterface API and transient objects returned from methods
    // on the interface object. Note we use HashSet rather than Set as the native side
    // expects HashSet (no bindings for interfaces).
    private final HashSet<Object> mRetainedJavaScriptObjects = new HashSet<Object>();

    /**
     * A {@link ViewAndroidDelegate} that delegates to the current container view.
     *
     * <p>This delegate handles the replacement of container views transparently so
     * that clients can safely hold to instances of this class.
     */
    private static class ContentViewAndroidDelegate implements ViewAndroidDelegate {
        /**
         * Represents the position of an anchor view.
         */
        @VisibleForTesting
        private static class Position {
            private final float mX;
            private final float mY;
            private final float mWidth;
            private final float mHeight;

            public Position(float x, float y, float width, float height) {
                mX = x;
                mY = y;
                mWidth = width;
                mHeight = height;
            }
        }

        private final RenderCoordinates mRenderCoordinates;

        /**
         * The current container view. This view can be updated with
         * {@link #updateCurrentContainerView()}. This needs to be a WeakReference
         * because ViewAndroidDelegate is held strongly native side, which otherwise
         * indefinitely prevents Android WebView from being garbage collected.
         */
        private WeakReference<ViewGroup> mCurrentContainerView;

        /**
         * List of anchor views stored in the order in which they were acquired mapped
         * to their position.
         */
        private final Map<View, Position> mAnchorViews = new LinkedHashMap<View, Position>();

        ContentViewAndroidDelegate(ViewGroup containerView, RenderCoordinates renderCoordinates) {
            mRenderCoordinates = renderCoordinates;
            mCurrentContainerView = new WeakReference<>(containerView);
        }

        @Override
        public View acquireAnchorView() {
            ViewGroup containerView = mCurrentContainerView.get();
            if (containerView == null) return null;
            View anchorView = new View(containerView.getContext());
            mAnchorViews.put(anchorView, null);
            containerView.addView(anchorView);
            return anchorView;
        }

        @Override
        public void setAnchorViewPosition(
                View view, float x, float y, float width, float height) {
            mAnchorViews.put(view, new Position(x, y, width, height));
            doSetAnchorViewPosition(view, x, y, width, height);
        }

        @SuppressWarnings("deprecation")  // AbsoluteLayout
        private void doSetAnchorViewPosition(
                View view, float x, float y, float width, float height) {
            if (view.getParent() == null) {
                // Ignore. setAnchorViewPosition has been called after the anchor view has
                // already been released.
                return;
            }
            ViewGroup containerView = mCurrentContainerView.get();
            if (containerView == null) {
                return;
            }
            assert view.getParent() == containerView;

            float scale =
                    (float) DeviceDisplayInfo.create(containerView.getContext()).getDIPScale();

            // The anchor view should not go outside the bounds of the ContainerView.
            int leftMargin = Math.round(x * scale);
            int topMargin = Math.round(mRenderCoordinates.getContentOffsetYPix() + y * scale);
            int scaledWidth = Math.round(width * scale);
            // ContentViewCore currently only supports these two container view types.
            if (containerView instanceof FrameLayout) {
                int startMargin;
                if (ApiCompatibilityUtils.isLayoutRtl(containerView)) {
                    startMargin =
                            containerView.getMeasuredWidth() - Math.round((width + x) * scale);
                } else {
                    startMargin = leftMargin;
                }
                if (scaledWidth + startMargin > containerView.getWidth()) {
                    scaledWidth = containerView.getWidth() - startMargin;
                }
                FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                        scaledWidth, Math.round(height * scale));
                ApiCompatibilityUtils.setMarginStart(lp, startMargin);
                lp.topMargin = topMargin;
                view.setLayoutParams(lp);
            } else if (containerView instanceof android.widget.AbsoluteLayout) {
                // This fixes the offset due to a difference in
                // scrolling model of WebView vs. Chrome.
                // TODO(sgurun) fix this to use mContainerViewAtCreation.getScroll[X/Y]()
                // as it naturally accounts for scroll differences between
                // these models.
                leftMargin += mRenderCoordinates.getScrollXPixInt();
                topMargin += mRenderCoordinates.getScrollYPixInt();

                android.widget.AbsoluteLayout.LayoutParams lp =
                        new android.widget.AbsoluteLayout.LayoutParams(
                            scaledWidth, (int) (height * scale), leftMargin, topMargin);
                view.setLayoutParams(lp);
            } else {
                Log.e(TAG, "Unknown layout %s", containerView.getClass().getName());
            }
        }

        @Override
        public void releaseAnchorView(View anchorView) {
            mAnchorViews.remove(anchorView);
            ViewGroup containerView = mCurrentContainerView.get();
            if (containerView != null) {
                containerView.removeView(anchorView);
            }
        }

        /**
         * Updates (or sets for the first time) the current container view to which
         * this class delegates. Existing anchor views are transferred from the old to
         * the new container view.
         */
        void updateCurrentContainerView(ViewGroup containerView) {
            ViewGroup oldContainerView = mCurrentContainerView.get();
            mCurrentContainerView = new WeakReference<>(containerView);
            for (Entry<View, Position> entry : mAnchorViews.entrySet()) {
                View anchorView = entry.getKey();
                Position position = entry.getValue();
                if (oldContainerView != null) {
                    oldContainerView.removeView(anchorView);
                }
                containerView.addView(anchorView);
                if (position != null) {
                    doSetAnchorViewPosition(anchorView,
                            position.mX, position.mY, position.mWidth, position.mHeight);
                }
            }
        }
    }

    /**
     * A {@link WebContentsObserver} that listens to frame navigation events.
     */
    private static class ContentViewWebContentsObserver extends WebContentsObserver {
        // Using a weak reference avoids cycles that might prevent GC of WebView's WebContents.
        private final WeakReference<ContentViewCore> mWeakContentViewCore;

        ContentViewWebContentsObserver(ContentViewCore contentViewCore) {
            super(contentViewCore.getWebContents());
            mWeakContentViewCore = new WeakReference<ContentViewCore>(contentViewCore);
        }

        @Override
        public void didFailLoad(boolean isProvisionalLoad, boolean isMainFrame, int errorCode,
                String description, String failingUrl, boolean wasIgnoredByHandler) {
            // Navigation that fails the provisional load will have the strong binding removed
            // here. One for which the provisional load is commited will have the strong binding
            // removed in navigationEntryCommitted() below.
            if (isProvisionalLoad) determinedProcessVisibility();
        }

        @Override
        public void didNavigateMainFrame(String url, String baseUrl,
                boolean isNavigationToDifferentPage, boolean isFragmentNavigation, int statusCode) {
            if (!isNavigationToDifferentPage) return;
            resetPopupsAndInput();
        }

        @Override
        public void renderProcessGone(boolean wasOomProtected) {
            resetPopupsAndInput();
        }

        @Override
        public void navigationEntryCommitted() {
            determinedProcessVisibility();
        }

        private void resetPopupsAndInput() {
            ContentViewCore contentViewCore = mWeakContentViewCore.get();
            if (contentViewCore == null) return;
            contentViewCore.mIsMobileOptimizedHint = false;
            contentViewCore.hidePopupsAndClearSelection();
            contentViewCore.resetScrollInProgress();
        }

        private void determinedProcessVisibility() {
            ContentViewCore contentViewCore = mWeakContentViewCore.get();
            if (contentViewCore == null) return;
            // Signal to the process management logic that we can now rely on the process
            // visibility signal for binding management. Before the navigation commits, its
            // renderer is considered background even if the pending navigation happens in the
            // foreground renderer.
            ChildProcessLauncher.determinedVisibility(contentViewCore.getCurrentRenderProcessId());
        }
    }

    /**
     * Interface that consumers of {@link ContentViewCore} must implement to allow the proper
     * dispatching of view methods through the containing view.
     *
     * <p>
     * All methods with the "super_" prefix should be routed to the parent of the
     * implementing container view.
     */
    @SuppressWarnings("javadoc")
    public interface InternalAccessDelegate {
        /**
         * @see View#onKeyUp(keyCode, KeyEvent)
         */
        boolean super_onKeyUp(int keyCode, KeyEvent event);

        /**
         * @see View#dispatchKeyEventPreIme(KeyEvent)
         */
        boolean super_dispatchKeyEventPreIme(KeyEvent event);

        /**
         * @see View#dispatchKeyEvent(KeyEvent)
         */
        boolean super_dispatchKeyEvent(KeyEvent event);

        /**
         * @see View#onGenericMotionEvent(MotionEvent)
         */
        boolean super_onGenericMotionEvent(MotionEvent event);

        /**
         * @see View#onConfigurationChanged(Configuration)
         */
        void super_onConfigurationChanged(Configuration newConfig);

        /**
         * @see View#onScrollChanged(int, int, int, int)
         */
        void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix);

        /**
         * @see View#awakenScrollBars()
         */
        boolean awakenScrollBars();

        /**
         * @see View#awakenScrollBars(int, boolean)
         */
        boolean super_awakenScrollBars(int startDelay, boolean invalidate);
    }

    /**
     * An interface for controlling visibility and state of embedder-provided zoom controls.
     */
    public interface ZoomControlsDelegate {
        /**
         * Called when it's reasonable to show zoom controls.
         */
        void invokeZoomPicker();

        /**
         * Called when zoom controls need to be hidden (e.g. when the view hides).
         */
        void dismissZoomPicker();

        /**
         * Called when page scale has been changed, so the controls can update their state.
         */
        void updateZoomControls();
    }

    /**
     * An interface that allows the embedder to be notified when the results of
     * extractSmartClipData are available.
     */
    public interface SmartClipDataListener {
        public void onSmartClipDataExtracted(String text, String html, Rect clipRect);
    }

    private final Context mContext;
    private ViewGroup mContainerView;
    private InternalAccessDelegate mContainerViewInternals;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;

    private ContentViewClient mContentViewClient;

    // Native pointer to C++ ContentViewCoreImpl object which will be set by nativeInit().
    private long mNativeContentViewCore = 0;

    private final ObserverList<GestureStateListener> mGestureStateListeners;
    private final RewindableIterator<GestureStateListener> mGestureStateListenersIterator;
    private ZoomControlsDelegate mZoomControlsDelegate;

    private PopupZoomer mPopupZoomer;
    private SelectPopup mSelectPopup;
    private long mNativeSelectPopupSourceFrame = 0;

    private OverscrollRefreshHandler mOverscrollRefreshHandler;

    private Runnable mFakeMouseMoveRunnable = null;

    // Only valid when focused on a text / password field.
    private ImeAdapter mImeAdapter;

    // Lazily created paste popup menu, triggered either via long press in an
    // editable region or from tapping the insertion handle.
    private PastePopupMenu mPastePopupMenu;
    private boolean mWasPastePopupShowingOnInsertionDragStart;

    // Size of the viewport in physical pixels as set from onSizeChanged.
    private int mViewportWidthPix;
    private int mViewportHeightPix;
    private int mPhysicalBackingWidthPix;
    private int mPhysicalBackingHeightPix;
    private int mTopControlsHeightPix;
    private boolean mTopControlsShrinkBlinkSize;

    // Cached copy of all positions and scales as reported by the renderer.
    private final RenderCoordinates mRenderCoordinates;

    // Provides smooth gamepad joystick-driven scrolling.
    private final JoystickScrollProvider mJoystickScrollProvider;

    private boolean mIsMobileOptimizedHint;

    // Tracks whether a selection is currently active.  When applied to selected text, indicates
    // whether the last selected text is still highlighted.
    private boolean mHasSelection;
    private boolean mHasInsertion;
    private boolean mDraggingSelection;
    private String mLastSelectedText;
    private boolean mFocusedNodeEditable;
    private boolean mFocusedNodeIsPassword;
    private WebActionMode mActionMode;
    private boolean mFloatingActionModeCreationFailed;
    private boolean mUnselectAllOnActionModeDismiss;
    private boolean mPreserveSelectionOnNextLossOfFocus;
    private WebActionModeCallback.ActionHandler mActionHandler;
    private final Rect mSelectionRect = new Rect();

    // Delegate that will handle GET downloads, and be notified of completion of POST downloads.
    private ContentViewDownloadDelegate mDownloadDelegate;

    // Whether native accessibility, i.e. without any script injection, is allowed.
    private boolean mNativeAccessibilityAllowed;

    // Whether native accessibility, i.e. without any script injection, has been enabled.
    private boolean mNativeAccessibilityEnabled;

    // Handles native accessibility, i.e. without any script injection.
    private BrowserAccessibilityManager mBrowserAccessibilityManager;

    // System accessibility service.
    private final AccessibilityManager mAccessibilityManager;

    // Notifies the ContentViewCore when platform closed caption settings have changed
    // if they are supported. Otherwise does nothing.
    private final SystemCaptioningBridge mSystemCaptioningBridge;

    // Accessibility touch exploration state.
    private boolean mTouchExplorationEnabled;

    // Whether accessibility focus should be set to the page when it finishes loading.
    // This only applies if an accessibility service like TalkBack is running.
    // This is desirable behavior for a browser window, but not for an embedded
    // WebView.
    private boolean mShouldSetAccessibilityFocusOnPageLoad;

    // Temporary notification to tell onSizeChanged to focus a form element,
    // because the OSK was just brought up.
    private final Rect mFocusPreOSKViewportRect = new Rect();

    // Store the x, y coordinates of the last touch or mouse event.
    private float mLastFocalEventX;
    private float mLastFocalEventY;

    // Whether a touch scroll sequence is active, used to hide text selection
    // handles. Note that a scroll sequence will *always* bound a pinch
    // sequence, so this will also be true for the duration of a pinch gesture.
    private boolean mTouchScrollInProgress;

    // Multiplier that determines how many (device) pixels to scroll per mouse
    // wheel tick. Defaults to the preferred list item height.
    private float mWheelScrollFactorInPixels;

    // The outstanding fling start events that hasn't got fling end yet. It may be > 1 because
    // onNativeFlingStopped() is called asynchronously.
    private int mPotentiallyActiveFlingCount;

    private SmartClipDataListener mSmartClipDataListener = null;
    private final ObserverList<ContainerViewObserver> mContainerViewObservers;

    /**
     * PID used to indicate an invalid render process.
     */
    // Keep in sync with the value returned from ContentViewCoreImpl::GetCurrentRendererProcessId()
    // if there is no render process.
    public static final int INVALID_RENDER_PROCESS_PID = 0;

    // Offsets for the events that passes through this ContentViewCore.
    private float mCurrentTouchOffsetX;
    private float mCurrentTouchOffsetY;

    // Offsets for smart clip
    private int mSmartClipOffsetX;
    private int mSmartClipOffsetY;

    // Whether the ContentViewCore requires the WebContents to be fullscreen in order to lock the
    // screen orientation.
    private boolean mFullscreenRequiredForOrientationLock = true;

    // A ViewAndroidDelegate that delegates to the current container view.
    private ContentViewAndroidDelegate mViewAndroidDelegate;

    // A flag to determine if we enable hover feature or not.
    private Boolean mEnableTouchHover;

    // The client that implements Contextual Search functionality, or null if none exists.
    private ContextualSearchClient mContextualSearchClient;

    /**
     * @param webContents The {@link WebContents} to find a {@link ContentViewCore} of.
     * @return            A {@link ContentViewCore} that is connected to {@code webContents} or
     *                    {@code null} if none exists.
     */
    public static ContentViewCore fromWebContents(WebContents webContents) {
        return nativeFromWebContentsAndroid(webContents);
    }

    /**
     * Constructs a new ContentViewCore. Embedders must call initialize() after constructing
     * a ContentViewCore and before using it.
     *
     * @param context The context used to create this.
     */
    public ContentViewCore(Context context) {
        mContext = context;
        mRenderCoordinates = new RenderCoordinates();
        mJoystickScrollProvider = new JoystickScrollProvider(this);
        float deviceScaleFactor = getContext().getResources().getDisplayMetrics().density;
        String forceScaleFactor = CommandLine.getInstance().getSwitchValue(
                ContentSwitches.FORCE_DEVICE_SCALE_FACTOR);
        if (forceScaleFactor != null) {
            deviceScaleFactor = Float.valueOf(forceScaleFactor);
        }
        mRenderCoordinates.setDeviceScaleFactor(deviceScaleFactor);
        mAccessibilityManager = (AccessibilityManager)
                getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        mSystemCaptioningBridge = CaptioningBridgeFactory.getSystemCaptioningBridge(mContext);
        mGestureStateListeners = new ObserverList<GestureStateListener>();
        mGestureStateListenersIterator = mGestureStateListeners.rewindableIterator();

        mContainerViewObservers = new ObserverList<ContainerViewObserver>();
    }

    /**
     * @return The context used for creating this ContentViewCore.
     */
    @CalledByNative
    public Context getContext() {
        return mContext;
    }

    /**
     * @return The ViewGroup that all view actions of this ContentViewCore should interact with.
     */
    public ViewGroup getContainerView() {
        return mContainerView;
    }

    /**
     * @return The WebContents currently being rendered.
     */
    public WebContents getWebContents() {
        return mWebContents;
    }

    /**
     * @return The WindowAndroid associated with this ContentViewCore.
     */
    public WindowAndroid getWindowAndroid() {
        if (mNativeContentViewCore == 0) return null;
        return nativeGetJavaWindowAndroid(mNativeContentViewCore);
    }

    /**
     *
     * @param topControlsHeightPix       The height of the top controls in pixels.
     * @param topControlsShrinkBlinkSize The Y amount in pixels to shrink the viewport by.  This
     *                                   specifies how much smaller the Blink layout size should be
     *                                   relative to the size of this View.
     */
    public void setTopControlsHeight(int topControlsHeightPix, boolean topControlsShrinkBlinkSize) {
        if (topControlsHeightPix == mTopControlsHeightPix
                && topControlsShrinkBlinkSize == mTopControlsShrinkBlinkSize) {
            return;
        }

        mTopControlsHeightPix = topControlsHeightPix;
        mTopControlsShrinkBlinkSize = topControlsShrinkBlinkSize;
        if (mNativeContentViewCore != 0) nativeWasResized(mNativeContentViewCore);
    }

    /**
     * Returns a delegate that can be used to add and remove views from the current
     * container view. Clients can safely hold to instances of this class as it handles the
     * replacement of container views transparently.
     *
     * NOTE: Use with care, as not all ContentViewCore users setup their container view in the same
     * way. In particular, the Android WebView has limitations on what implementation details can
     * be provided via a child view, as they are visible in the API and could introduce
     * compatibility breaks with existing applications. If in doubt, contact the
     * android_webview/OWNERS
     *
     * @return A ViewAndroidDelegate that can be used to add and remove views.
     */
    public ViewAndroidDelegate getViewAndroidDelegate() {
        return mViewAndroidDelegate;
    }

    @VisibleForTesting
    public void setImeAdapterForTest(ImeAdapter imeAdapter) {
        mImeAdapter = imeAdapter;
    }

    @VisibleForTesting
    public ImeAdapter getImeAdapterForTest() {
        return mImeAdapter;
    }

    private ImeAdapter createImeAdapter() {
        return new ImeAdapter(
                new InputMethodManagerWrapper(mContext), new ImeAdapter.ImeAdapterDelegate() {
                    @Override
                    public void onImeEvent() {
                        mPopupZoomer.hide(true);
                        getContentViewClient().onImeEvent();
                        if (mFocusedNodeEditable) dismissTextHandles();
                    }

                    @Override
                    public void onKeyboardBoundsUnchanged() {
                        assert mWebContents != null;
                        mWebContents.scrollFocusedEditableNodeIntoView();
                    }

                    @Override
                    public boolean performContextMenuAction(int id) {
                        assert mWebContents != null;
                        switch (id) {
                            case android.R.id.selectAll:
                                mWebContents.selectAll();
                                return true;
                            case android.R.id.cut:
                                mWebContents.cut();
                                return true;
                            case android.R.id.copy:
                                mWebContents.copy();
                                return true;
                            case android.R.id.paste:
                                mWebContents.paste();
                                return true;
                            default:
                                return false;
                        }
                    }

                    @Override
                    public View getAttachedView() {
                        return mContainerView;
                    }

                    @Override
                    public ResultReceiver getNewShowKeyboardReceiver() {
                        return new ResultReceiver(new Handler()) {
                            @Override
                            public void onReceiveResult(int resultCode, Bundle resultData) {
                                if (resultCode == InputMethodManager.RESULT_SHOWN) {
                                    // If OSK is newly shown, delay the form focus until
                                    // the onSizeChanged (in order to adjust relative to the
                                    // new size).
                                    // TODO(jdduke): We should not assume that onSizeChanged will
                                    // always be called, crbug.com/294908.
                                    getContainerView().getWindowVisibleDisplayFrame(
                                            mFocusPreOSKViewportRect);
                                } else if (hasFocus() && resultCode
                                        == InputMethodManager.RESULT_UNCHANGED_SHOWN) {
                                    // If the OSK was already there, focus the form immediately.
                                    if (mWebContents != null) {
                                        mWebContents.scrollFocusedEditableNodeIntoView();
                                    }
                                }
                            }
                        };
                    }
                });
    }

    /**
     *
     * @param containerView The view that will act as a container for all views created by this.
     * @param internalDispatcher Handles dispatching all hidden or super methods to the
     *                           containerView.
     * @param webContents A WebContents instance to connect to.
     * @param windowAndroid An instance of the WindowAndroid.
     */
    // Perform important post-construction set up of the ContentViewCore.
    // We do not require the containing view in the constructor to allow embedders to create a
    // ContentViewCore without having fully created its containing view. The containing view
    // is a vital component of the ContentViewCore, so embedders must exercise caution in what
    // they do with the ContentViewCore before calling initialize().
    // We supply the nativeWebContents pointer here rather than in the constructor to allow us
    // to set the private browsing mode at a later point for the WebView implementation.
    // Note that the caller remains the owner of the nativeWebContents and is responsible for
    // deleting it after destroying the ContentViewCore.
    public void initialize(ViewGroup containerView, InternalAccessDelegate internalDispatcher,
            WebContents webContents, WindowAndroid windowAndroid) {
        createContentViewAndroidDelegate();
        setContainerView(containerView);
        long windowNativePointer = windowAndroid.getNativePointer();
        assert windowNativePointer != 0;

        mZoomControlsDelegate = NO_OP_ZOOM_CONTROLS_DELEGATE;

        mNativeContentViewCore = nativeInit(
                webContents, mViewAndroidDelegate, windowNativePointer, mRetainedJavaScriptObjects);
        mWebContents = nativeGetWebContentsAndroid(mNativeContentViewCore);

        setContainerViewInternals(internalDispatcher);
        mRenderCoordinates.reset();
        initPopupZoomer(mContext);
        mImeAdapter = createImeAdapter();
        attachImeAdapter();

        mWebContentsObserver = new ContentViewWebContentsObserver(this);
    }

    @VisibleForTesting
    public void createContentViewAndroidDelegate() {
        mViewAndroidDelegate = new ContentViewAndroidDelegate(mContainerView, mRenderCoordinates);
    }

    /**
     * Sets a new container view for this {@link ContentViewCore}.
     *
     * <p>WARNING: This method can also be used to replace the existing container view,
     * but you should only do it if you have a very good reason to. Replacing the
     * container view has been designed to support fullscreen in the Webview so it
     * might not be appropriate for other use cases.
     *
     * <p>This method only performs a small part of replacing the container view and
     * embedders are responsible for:
     * <ul>
     *     <li>Disconnecting the old container view from this ContentViewCore</li>
     *     <li>Updating the InternalAccessDelegate</li>
     *     <li>Reconciling the state of this ContentViewCore with the new container view</li>
     *     <li>Tearing down and recreating the native GL rendering where appropriate</li>
     *     <li>etc.</li>
     * </ul>
     */
    public void setContainerView(ViewGroup containerView) {
        try {
            TraceEvent.begin("ContentViewCore.setContainerView");
            if (mContainerView != null) {
                assert mOverscrollRefreshHandler == null;
                mPastePopupMenu = null;
                hidePopupsAndClearSelection();
            }

            mContainerView = containerView;
            mContainerView.setClickable(true);
            mViewAndroidDelegate.updateCurrentContainerView(mContainerView);
            for (ContainerViewObserver observer : mContainerViewObservers) {
                observer.onContainerViewChanged(mContainerView);
            }
        } finally {
            TraceEvent.end("ContentViewCore.setContainerView");
        }
    }

    public void addContainerViewObserver(ContainerViewObserver observer) {
        mContainerViewObservers.addObserver(observer);
    }

    public void removeContainerViewObserver(ContainerViewObserver observer) {
        mContainerViewObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onNativeContentViewCoreDestroyed(long nativeContentViewCore) {
        assert nativeContentViewCore == mNativeContentViewCore;
        mNativeContentViewCore = 0;
    }

    /**
     * Set the Container view Internals.
     * @param internalDispatcher Handles dispatching all hidden or super methods to the
     *                           containerView.
     */
    public void setContainerViewInternals(InternalAccessDelegate internalDispatcher) {
        mContainerViewInternals = internalDispatcher;
    }

    @VisibleForTesting
    void initPopupZoomer(Context context) {
        mPopupZoomer = new PopupZoomer(context);
        mPopupZoomer.setOnVisibilityChangedListener(new PopupZoomer.OnVisibilityChangedListener() {
            // mContainerView can change, but this OnVisibilityChangedListener can only be used
            // to add and remove views from the mContainerViewAtCreation.
            private final ViewGroup mContainerViewAtCreation = mContainerView;

            @Override
            public void onPopupZoomerShown(final PopupZoomer zoomer) {
                mContainerViewAtCreation.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mContainerViewAtCreation.indexOfChild(zoomer) == -1) {
                            mContainerViewAtCreation.addView(zoomer);
                        }
                    }
                });
            }

            @Override
            public void onPopupZoomerHidden(final PopupZoomer zoomer) {
                mContainerViewAtCreation.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mContainerViewAtCreation.indexOfChild(zoomer) != -1) {
                            mContainerViewAtCreation.removeView(zoomer);
                            mContainerViewAtCreation.invalidate();
                        }
                    }
                });
            }
        });
        // TODO(yongsheng): LONG_TAP is not enabled in PopupZoomer. So need to dispatch a LONG_TAP
        // gesture if a user completes a tap on PopupZoomer UI after a LONG_PRESS gesture.
        PopupZoomer.OnTapListener listener = new PopupZoomer.OnTapListener() {
            // mContainerView can change, but this OnTapListener can only be used
            // with the mContainerViewAtCreation.
            private final ViewGroup mContainerViewAtCreation = mContainerView;

            @Override
            public boolean onSingleTap(View v, MotionEvent e) {
                mContainerViewAtCreation.requestFocus();
                if (mNativeContentViewCore != 0) {
                    nativeSingleTap(mNativeContentViewCore, e.getEventTime(), e.getX(), e.getY());
                }
                return true;
            }

            @Override
            public boolean onLongPress(View v, MotionEvent e) {
                if (mNativeContentViewCore != 0) {
                    nativeLongPress(mNativeContentViewCore, e.getEventTime(), e.getX(), e.getY());
                }
                return true;
            }
        };
        mPopupZoomer.setOnTapListener(listener);
    }

    @VisibleForTesting
    public void setPopupZoomerForTest(PopupZoomer popupZoomer) {
        mPopupZoomer = popupZoomer;
    }

    /**
     * Destroy the internal state of the ContentView. This method may only be
     * called after the ContentView has been removed from the view system. No
     * other methods may be called on this ContentView after this method has
     * been called.
     * Warning: destroy() is not guranteed to be called in Android WebView.
     * Any object that relies solely on destroy() being called to be cleaned up
     * will leak in Android WebView. If appropriate, consider clean up in
     * onDetachedFromWindow() which is guaranteed to be called in Android WebView.
     */
    public void destroy() {
        if (mNativeContentViewCore != 0) {
            nativeOnJavaContentViewCoreDestroyed(mNativeContentViewCore);
        }
        mWebContentsObserver.destroy();
        mWebContentsObserver = null;
        setSmartClipDataListener(null);
        setZoomControlsDelegate(null);
        // TODO(igsolla): address TODO in ContentViewClient because ContentViewClient is not
        // currently a real Null Object.
        //
        // Instead of deleting the client we use the Null Object pattern to avoid null checks
        // in this class.
        mContentViewClient = new ContentViewClient();
        mWebContents = null;
        mOverscrollRefreshHandler = null;
        mNativeContentViewCore = 0;
        mJavaScriptInterfaces.clear();
        mRetainedJavaScriptObjects.clear();
        for (mGestureStateListenersIterator.rewind(); mGestureStateListenersIterator.hasNext();) {
            mGestureStateListenersIterator.next().onDestroyed();
        }
        mGestureStateListeners.clear();
        ScreenOrientationListener.getInstance().removeObserver(this);
        mContainerViewObservers.clear();
        hidePopupsAndPreserveSelection();
        mPastePopupMenu = null;

        // See warning in javadoc before adding more clean up code here.
    }

    /**
     * Returns true initially, false after destroy() has been called.
     * It is illegal to call any other public method after destroy().
     */
    public boolean isAlive() {
        return mNativeContentViewCore != 0;
    }

    /**
     * This is only useful for passing over JNI to native code that requires ContentViewCore*.
     * @return native ContentViewCore pointer.
     */
    @CalledByNative
    long getNativeContentViewCore() {
        return mNativeContentViewCore;
    }

    public void setContentViewClient(ContentViewClient client) {
        if (client == null) {
            throw new IllegalArgumentException("The client can't be null.");
        }
        mContentViewClient = client;
    }

    @VisibleForTesting
    public ContentViewClient getContentViewClient() {
        if (mContentViewClient == null) {
            // We use the Null Object pattern to avoid having to perform a null check in this class.
            // We create it lazily because most of the time a client will be set almost immediately
            // after ContentView is created.
            mContentViewClient = new ContentViewClient();
            // We don't set the native ContentViewClient pointer here on purpose. The native
            // implementation doesn't mind a null delegate and using one is better than passing a
            // Null Object, since we cut down on the number of JNI calls.
        }
        return mContentViewClient;
    }

    @CalledByNative
    private void onBackgroundColorChanged(int color) {
        getContentViewClient().onBackgroundColorChanged(color);
    }

    /**
     * @return Viewport width in physical pixels as set from onSizeChanged.
     */
    @CalledByNative
    public int getViewportWidthPix() {
        return mViewportWidthPix;
    }

    /**
     * @return Viewport height in physical pixels as set from onSizeChanged.
     */
    @CalledByNative
    public int getViewportHeightPix() {
        return mViewportHeightPix;
    }

    /**
     * @return Width of underlying physical surface.
     */
    @CalledByNative
    private int getPhysicalBackingWidthPix() {
        return mPhysicalBackingWidthPix;
    }

    /**
     * @return Height of underlying physical surface.
     */
    @CalledByNative
    private int getPhysicalBackingHeightPix() {
        return mPhysicalBackingHeightPix;
    }

    /**
     * @return The amount that the viewport size given to Blink is shrunk by the URL-bar..
     */
    @CalledByNative
    public boolean doTopControlsShrinkBlinkSize() {
        return mTopControlsShrinkBlinkSize;
    }

    @CalledByNative
    public int getTopControlsHeightPix() {
        return mTopControlsHeightPix;
    }

    /**
     * @see android.webkit.WebView#getContentHeight()
     */
    public float getContentHeightCss() {
        return mRenderCoordinates.getContentHeightCss();
    }

    /**
     * @see android.webkit.WebView#getContentWidth()
     */
    public float getContentWidthCss() {
        return mRenderCoordinates.getContentWidthCss();
    }

    /**
     * @return The selected text (empty if no text selected).
     */
    @VisibleForTesting
    public String getSelectedText() {
        return mHasSelection ? mLastSelectedText : "";
    }

    /**
     * @return Whether the current selection is editable (false if no text selected).
     */
    public boolean isSelectionEditable() {
        return mHasSelection ? mFocusedNodeEditable : false;
    }

    /**
     * @return Whether the current focused node is editable.
     */
    public boolean isFocusedNodeEditable() {
        return mFocusedNodeEditable;
    }

    /**
     * @return Whether the HTML5 gamepad API is active.
     */
    public boolean isGamepadAPIActive() {
        return GamepadList.isGamepadAPIActive();
    }

    // End FrameLayout overrides.

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    public boolean onTouchEvent(MotionEvent event) {
        final boolean isTouchHandleEvent = false;
        return onTouchEventImpl(event, isTouchHandleEvent);
    }

    /**
     * Called by PopupWindow-based touch handles.
     * @param event the MotionEvent targeting the handle.
     */
    public boolean onTouchHandleEvent(MotionEvent event) {
        final boolean isTouchHandleEvent = true;
        return onTouchEventImpl(event, isTouchHandleEvent);
    }

    private boolean onTouchEventImpl(MotionEvent event, boolean isTouchHandleEvent) {
        TraceEvent.begin("onTouchEvent");
        try {
            int eventAction = event.getActionMasked();

            if (eventAction == MotionEvent.ACTION_DOWN) {
                cancelRequestToScrollFocusedEditableNodeIntoView();
            }

            if (SPenSupport.isSPenSupported(mContext)) {
                eventAction = SPenSupport.convertSPenEventAction(eventAction);
            }
            if (!isValidTouchEventActionForNative(eventAction)) return false;

            if (mNativeContentViewCore == 0) return false;

            // A zero offset is quite common, in which case the unnecessary copy should be avoided.
            MotionEvent offset = null;
            if (mCurrentTouchOffsetX != 0 || mCurrentTouchOffsetY != 0) {
                offset = createOffsetMotionEvent(event);
                event = offset;
            }

            final int pointerCount = event.getPointerCount();

            float[] touchMajor = {event.getTouchMajor(),
                                  pointerCount > 1 ? event.getTouchMajor(1) : 0};
            float[] touchMinor = {event.getTouchMinor(),
                                  pointerCount > 1 ? event.getTouchMinor(1) : 0};

            for (int i = 0; i < 2; i++) {
                if (touchMajor[i] < touchMinor[i]) {
                    float tmp = touchMajor[i];
                    touchMajor[i] = touchMinor[i];
                    touchMinor[i] = tmp;
                }
            }

            final boolean consumed = nativeOnTouchEvent(mNativeContentViewCore, event,
                    event.getEventTime(), eventAction,
                    pointerCount, event.getHistorySize(), event.getActionIndex(),
                    event.getX(), event.getY(),
                    pointerCount > 1 ? event.getX(1) : 0,
                    pointerCount > 1 ? event.getY(1) : 0,
                    event.getPointerId(0), pointerCount > 1 ? event.getPointerId(1) : -1,
                    touchMajor[0], touchMajor[1],
                    touchMinor[0], touchMinor[1],
                    event.getOrientation(), pointerCount > 1 ? event.getOrientation(1) : 0,
                    event.getAxisValue(MotionEvent.AXIS_TILT),
                    pointerCount > 1 ? event.getAxisValue(MotionEvent.AXIS_TILT, 1) : 0,
                    event.getRawX(), event.getRawY(),
                    event.getToolType(0),
                    pointerCount > 1 ? event.getToolType(1) : MotionEvent.TOOL_TYPE_UNKNOWN,
                    event.getButtonState(),
                    event.getMetaState(),
                    isTouchHandleEvent);

            if (offset != null) offset.recycle();
            return consumed;
        } finally {
            TraceEvent.end("onTouchEvent");
        }
    }

    @CalledByNative
    private void requestDisallowInterceptTouchEvent() {
        mContainerView.requestDisallowInterceptTouchEvent(true);
    }

    private static boolean isValidTouchEventActionForNative(int eventAction) {
        // Only these actions have any effect on gesture detection.  Other
        // actions have no corresponding WebTouchEvent type and may confuse the
        // touch pipline, so we ignore them entirely.
        return eventAction == MotionEvent.ACTION_DOWN
                || eventAction == MotionEvent.ACTION_UP
                || eventAction == MotionEvent.ACTION_CANCEL
                || eventAction == MotionEvent.ACTION_MOVE
                || eventAction == MotionEvent.ACTION_POINTER_DOWN
                || eventAction == MotionEvent.ACTION_POINTER_UP;
    }

    /**
     * @return Whether a scroll targeting web content is in progress.
     */
    public boolean isScrollInProgress() {
        return mTouchScrollInProgress || mPotentiallyActiveFlingCount > 0;
    }

    private void setTouchScrollInProgress(boolean inProgress) {
        if (mTouchScrollInProgress == inProgress) return;
        mTouchScrollInProgress = inProgress;
        updateActionModeVisibility();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onFlingStartEventConsumed(int vx, int vy) {
        mPotentiallyActiveFlingCount++;
        setTouchScrollInProgress(false);
        for (mGestureStateListenersIterator.rewind();
                    mGestureStateListenersIterator.hasNext();) {
            mGestureStateListenersIterator.next().onFlingStartGesture(
                    vx, vy, computeVerticalScrollOffset(), computeVerticalScrollExtent());
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onFlingCancelEventAck() {
        updateGestureStateListener(GestureEventType.FLING_CANCEL);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onScrollBeginEventAck() {
        setTouchScrollInProgress(true);
        hidePastePopup();
        mZoomControlsDelegate.invokeZoomPicker();
        updateGestureStateListener(GestureEventType.SCROLL_START);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onScrollUpdateGestureConsumed() {
        mZoomControlsDelegate.invokeZoomPicker();
        for (mGestureStateListenersIterator.rewind();
                mGestureStateListenersIterator.hasNext();) {
            mGestureStateListenersIterator.next().onScrollUpdateGestureConsumed();
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onScrollEndEventAck() {
        setTouchScrollInProgress(false);
        updateGestureStateListener(GestureEventType.SCROLL_END);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onPinchBeginEventAck() {
        updateGestureStateListener(GestureEventType.PINCH_BEGIN);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onPinchEndEventAck() {
        updateGestureStateListener(GestureEventType.PINCH_END);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onSingleTapEventAck(boolean consumed, int x, int y) {
        for (mGestureStateListenersIterator.rewind();
                mGestureStateListenersIterator.hasNext();) {
            mGestureStateListenersIterator.next().onSingleTap(consumed, x, y);
        }
        hidePastePopup();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onShowUnhandledTapUIIfNeeded(int x, int y) {
        if (mContextualSearchClient != null) {
            mContextualSearchClient.showUnhandledTapUIIfNeeded(x, y);
        }
    }

    /**
     * Called just prior to a tap or press gesture being forwarded to the renderer.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private boolean filterTapOrPressEvent(int type, int x, int y) {
        if (type == GestureEventType.LONG_PRESS && offerLongPressToEmbedder()) {
            return true;
        }
        updateForTapOrPress(type, x, y);
        return false;
    }

    @VisibleForTesting
    public void sendDoubleTapForTest(long timeMs, int x, int y) {
        if (mNativeContentViewCore == 0) return;
        nativeDoubleTap(mNativeContentViewCore, timeMs, x, y);
    }

    /**
     * Flings the viewport with velocity vector (velocityX, velocityY).
     * @param timeMs the current time.
     * @param velocityX fling speed in x-axis.
     * @param velocityY fling speed in y-axis.
     */
    public void flingViewport(long timeMs, int velocityX, int velocityY) {
        if (mNativeContentViewCore == 0) return;
        nativeFlingCancel(mNativeContentViewCore, timeMs);
        nativeScrollBegin(mNativeContentViewCore, timeMs, 0, 0, velocityX, velocityY, true);
        nativeFlingStart(mNativeContentViewCore, timeMs, 0, 0, velocityX, velocityY, true);
    }

    /**
     * Cancel any fling gestures active.
     * @param timeMs Current time (in milliseconds).
     */
    public void cancelFling(long timeMs) {
        if (mNativeContentViewCore == 0) return;
        nativeFlingCancel(mNativeContentViewCore, timeMs);
    }

    /**
     * Add a listener that gets alerted on gesture state changes.
     * @param listener Listener to add.
     */
    public void addGestureStateListener(GestureStateListener listener) {
        mGestureStateListeners.addObserver(listener);
    }

    /**
     * Removes a listener that was added to watch for gesture state changes.
     * @param listener Listener to remove.
     */
    public void removeGestureStateListener(GestureStateListener listener) {
        mGestureStateListeners.removeObserver(listener);
    }

    void updateGestureStateListener(int gestureType) {
        for (mGestureStateListenersIterator.rewind();
                mGestureStateListenersIterator.hasNext();) {
            GestureStateListener listener = mGestureStateListenersIterator.next();
            switch (gestureType) {
                case GestureEventType.PINCH_BEGIN:
                    listener.onPinchStarted();
                    break;
                case GestureEventType.PINCH_END:
                    listener.onPinchEnded();
                    break;
                case GestureEventType.FLING_END:
                    listener.onFlingEndGesture(
                            computeVerticalScrollOffset(),
                            computeVerticalScrollExtent());
                    break;
                case GestureEventType.SCROLL_START:
                    listener.onScrollStarted(
                            computeVerticalScrollOffset(),
                            computeVerticalScrollExtent());
                    break;
                case GestureEventType.SCROLL_END:
                    listener.onScrollEnded(
                            computeVerticalScrollOffset(),
                            computeVerticalScrollExtent());
                    break;
                default:
                    break;
            }
        }
    }

    /**
     * To be called when the ContentView is shown.
     */
    public void onShow() {
        assert mWebContents != null;
        mWebContents.onShow();
        setAccessibilityState(mAccessibilityManager.isEnabled());
        restoreSelectionPopupsIfNecessary();
    }

    /**
     * @return The ID of the renderer process that backs this tab or
     *         {@link #INVALID_RENDER_PROCESS_PID} if there is none.
     */
    public int getCurrentRenderProcessId() {
        return nativeGetCurrentRenderProcessId(mNativeContentViewCore);
    }

    /**
     * To be called when the ContentView is hidden.
     */
    public void onHide() {
        assert mWebContents != null;
        hidePopupsAndPreserveSelection();
        mWebContents.onHide();
    }

    private void hidePopupsAndClearSelection() {
        mUnselectAllOnActionModeDismiss = true;
        hidePopups();
    }

    private void hidePopupsAndPreserveSelection() {
        mUnselectAllOnActionModeDismiss = false;
        hidePopups();
    }

    private void hidePopups() {
        hideSelectActionMode();
        hidePastePopup();
        hideSelectPopup();
        mPopupZoomer.hide(false);
        if (mUnselectAllOnActionModeDismiss) dismissTextHandles();
    }

    private void restoreSelectionPopupsIfNecessary() {
        if (mHasSelection && mActionMode == null) showSelectActionMode(true);
    }

    public void hideSelectActionMode() {
        if (mActionMode != null) {
            mActionMode.finish();
            mActionMode = null;
        }
    }

    public boolean isSelectActionBarShowing() {
        return mActionMode != null;
    }

    private void resetGestureDetection() {
        if (mNativeContentViewCore == 0) return;
        nativeResetGestureDetection(mNativeContentViewCore);
    }

    /**
     * @see View#onAttachedToWindow()
     */
    @SuppressWarnings("javadoc")
    public void onAttachedToWindow() {
        setAccessibilityState(mAccessibilityManager.isEnabled());
        setTextHandlesTemporarilyHidden(false);
        restoreSelectionPopupsIfNecessary();
        ScreenOrientationListener.getInstance().addObserver(this, mContext);
        GamepadList.onAttachedToWindow(mContext);
        mAccessibilityManager.addAccessibilityStateChangeListener(this);
        mSystemCaptioningBridge.addListener(this);
    }

    /**
     * @see View#onDetachedFromWindow()
     */
    @SuppressWarnings("javadoc")
    @SuppressLint("MissingSuperCall")
    public void onDetachedFromWindow() {
        mZoomControlsDelegate.dismissZoomPicker();

        ScreenOrientationListener.getInstance().removeObserver(this);
        GamepadList.onDetachedFromWindow();
        mAccessibilityManager.removeAccessibilityStateChangeListener(this);

        // WebView uses PopupWindows for handle rendering, which may remain
        // unintentionally visible even after the WebView has been detached.
        // Override the handle visibility explicitly to address this, but
        // preserve the underlying selection for detachment cases like screen
        // locking and app switching.
        setTextHandlesTemporarilyHidden(true);
        hidePopupsAndPreserveSelection();
        mSystemCaptioningBridge.removeListener(this);
    }

    /**
     * @see View#onVisibilityChanged(android.view.View, int)
     */
    public void onVisibilityChanged(View changedView, int visibility) {
        if (visibility != View.VISIBLE) {
            mZoomControlsDelegate.dismissZoomPicker();
        }
    }

    /**
     * @see View#onCreateInputConnection(EditorInfo)
     */
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return mImeAdapter.onCreateInputConnection(outAttrs);
    }

    /**
     * @see View#onCheckIsTextEditor()
     */
    public boolean onCheckIsTextEditor() {
        return mImeAdapter.hasTextInputType();
    }

    /**
     * @see View#onConfigurationChanged(Configuration)
     */
    @SuppressWarnings("javadoc")
    public void onConfigurationChanged(Configuration newConfig) {
        try {
            TraceEvent.begin("ContentViewCore.onConfigurationChanged");
            mImeAdapter.onKeyboardConfigurationChanged(newConfig);
            mContainerViewInternals.super_onConfigurationChanged(newConfig);
            // To request layout has side effect, but it seems OK as it only happen in
            // onConfigurationChange and layout has to be changed in most case.
            mContainerView.requestLayout();
        } finally {
            TraceEvent.end("ContentViewCore.onConfigurationChanged");
        }
    }

    /**
     * @see View#onSizeChanged(int, int, int, int)
     */
    @SuppressWarnings("javadoc")
    public void onSizeChanged(int wPix, int hPix, int owPix, int ohPix) {
        if (getViewportWidthPix() == wPix && getViewportHeightPix() == hPix) return;

        mViewportWidthPix = wPix;
        mViewportHeightPix = hPix;
        if (mNativeContentViewCore != 0) {
            nativeWasResized(mNativeContentViewCore);
        }

        updateAfterSizeChanged();
    }

    /**
     * Called when the underlying surface the compositor draws to changes size.
     * This may be larger than the viewport size.
     */
    public void onPhysicalBackingSizeChanged(int wPix, int hPix) {
        if (mPhysicalBackingWidthPix == wPix && mPhysicalBackingHeightPix == hPix) return;

        mPhysicalBackingWidthPix = wPix;
        mPhysicalBackingHeightPix = hPix;

        if (mNativeContentViewCore != 0) {
            nativeWasResized(mNativeContentViewCore);
        }
    }

    private void updateAfterSizeChanged() {
        mPopupZoomer.hide(false);

        // Execute a delayed form focus operation because the OSK was brought
        // up earlier.
        if (!mFocusPreOSKViewportRect.isEmpty()) {
            Rect rect = new Rect();
            getContainerView().getWindowVisibleDisplayFrame(rect);
            if (!rect.equals(mFocusPreOSKViewportRect)) {
                // Only assume the OSK triggered the onSizeChanged if width was preserved.
                if (rect.width() == mFocusPreOSKViewportRect.width()) {
                    assert mWebContents != null;
                    mWebContents.scrollFocusedEditableNodeIntoView();
                }
                cancelRequestToScrollFocusedEditableNodeIntoView();
            }
        }
    }

    private void cancelRequestToScrollFocusedEditableNodeIntoView() {
        // Zero-ing the rect will prevent |updateAfterSizeChanged()| from
        // issuing the delayed form focus event.
        mFocusPreOSKViewportRect.setEmpty();
    }

    /**
     * @see View#onWindowFocusChanged(boolean)
     */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (!hasWindowFocus) resetGestureDetection();
        if (mActionMode != null) mActionMode.onWindowFocusChanged(hasWindowFocus);
        for (mGestureStateListenersIterator.rewind(); mGestureStateListenersIterator.hasNext();) {
            mGestureStateListenersIterator.next().onWindowFocusChanged(hasWindowFocus);
        }
    }

    public void onFocusChanged(boolean gainFocus) {
        mImeAdapter.onViewFocusChanged(gainFocus);
        mJoystickScrollProvider.setEnabled(gainFocus && !mFocusedNodeEditable);

        if (gainFocus) {
            restoreSelectionPopupsIfNecessary();
        } else {
            cancelRequestToScrollFocusedEditableNodeIntoView();
            if (mPreserveSelectionOnNextLossOfFocus) {
                mPreserveSelectionOnNextLossOfFocus = false;
                hidePopupsAndPreserveSelection();
            } else {
                hidePopupsAndClearSelection();
                // Clear the selection. The selection is cleared on destroying IME
                // and also here since we may receive destroy first, for example
                // when focus is lost in webview.
                clearSelection();
            }
        }
        if (mNativeContentViewCore != 0) nativeSetFocus(mNativeContentViewCore, gainFocus);
    }

    /**
     * @see View#onKeyUp(int, KeyEvent)
     */
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (mPopupZoomer.isShowing() && keyCode == KeyEvent.KEYCODE_BACK) {
            mPopupZoomer.hide(true);
            return true;
        }
        return mContainerViewInternals.super_onKeyUp(keyCode, event);
    }

    /**
     * @see View#dispatchKeyEventPreIme(KeyEvent)
     */
    public boolean dispatchKeyEventPreIme(KeyEvent event) {
        try {
            TraceEvent.begin("ContentViewCore.dispatchKeyEventPreIme");
            return mContainerViewInternals.super_dispatchKeyEventPreIme(event);
        } finally {
            TraceEvent.end("ContentViewCore.dispatchKeyEventPreIme");
        }
    }

    /**
     * @see View#dispatchKeyEvent(KeyEvent)
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (GamepadList.dispatchKeyEvent(event)) return true;
        if (getContentViewClient().shouldOverrideKeyEvent(event)) {
            return mContainerViewInternals.super_dispatchKeyEvent(event);
        }

        if (mImeAdapter.dispatchKeyEvent(event)) return true;

        return mContainerViewInternals.super_dispatchKeyEvent(event);
    }

    /**
     * @see View#onHoverEvent(MotionEvent)
     * Mouse move events are sent on hover enter, hover move and hover exit.
     * They are sent on hover exit because sometimes it acts as both a hover
     * move and hover exit.
     */
    public boolean onHoverEvent(MotionEvent event) {
        TraceEvent.begin("onHoverEvent");

        MotionEvent offset = createOffsetMotionEvent(event);
        try {
            if (mBrowserAccessibilityManager != null) {
                return mBrowserAccessibilityManager.onHoverEvent(offset);
            }

            // Work around Android bug where the x, y coordinates of a hover exit
            // event are incorrect when touch exploration is on.
            if (mTouchExplorationEnabled && offset.getAction() == MotionEvent.ACTION_HOVER_EXIT) {
                return true;
            }

            // TODO(lanwei): Remove this switch once experimentation is complete -
            // crbug.com/418188
            if (event.getToolType(0) == MotionEvent.TOOL_TYPE_FINGER) {
                if (mEnableTouchHover == null) {
                    mEnableTouchHover =
                            CommandLine.getInstance().hasSwitch(ContentSwitches.ENABLE_TOUCH_HOVER);
                }
                if (!mEnableTouchHover.booleanValue()) return false;
            }

            mContainerView.removeCallbacks(mFakeMouseMoveRunnable);
            if (mNativeContentViewCore != 0) {
                nativeSendMouseMoveEvent(mNativeContentViewCore, offset.getEventTime(),
                        offset.getX(), offset.getY());
            }
            return true;
        } finally {
            offset.recycle();
            TraceEvent.end("onHoverEvent");
        }
    }

    /**
     * @see View#onGenericMotionEvent(MotionEvent)
     */
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (GamepadList.onGenericMotionEvent(event)) return true;
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
            mLastFocalEventX = event.getX();
            mLastFocalEventY = event.getY();
            switch (event.getAction()) {
                case MotionEvent.ACTION_SCROLL:
                    if (mNativeContentViewCore == 0) return false;

                    nativeSendMouseWheelEvent(mNativeContentViewCore, event.getEventTime(),
                            event.getX(), event.getY(),
                            event.getAxisValue(MotionEvent.AXIS_HSCROLL),
                            event.getAxisValue(MotionEvent.AXIS_VSCROLL),
                            getWheelScrollFactorInPixels());

                    mContainerView.removeCallbacks(mFakeMouseMoveRunnable);
                    // Send a delayed onMouseMove event so that we end
                    // up hovering over the right position after the scroll.
                    final MotionEvent eventFakeMouseMove = MotionEvent.obtain(event);
                    mFakeMouseMoveRunnable = new Runnable() {
                        @Override
                        public void run() {
                            onHoverEvent(eventFakeMouseMove);
                            eventFakeMouseMove.recycle();
                        }
                    };
                    mContainerView.postDelayed(mFakeMouseMoveRunnable, 250);
                    return true;
            }
        } else if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
            if (mJoystickScrollProvider.onMotion(event)) return true;
        }
        return mContainerViewInternals.super_onGenericMotionEvent(event);
    }

    /**
     * Sets the current amount to offset incoming touch events by.  This is used to handle content
     * moving and not lining up properly with the android input system.
     * @param dx The X offset in pixels to shift touch events.
     * @param dy The Y offset in pixels to shift touch events.
     */
    public void setCurrentMotionEventOffsets(float dx, float dy) {
        mCurrentTouchOffsetX = dx;
        mCurrentTouchOffsetY = dy;
    }

    private MotionEvent createOffsetMotionEvent(MotionEvent src) {
        MotionEvent dst = MotionEvent.obtain(src);
        dst.offsetLocation(mCurrentTouchOffsetX, mCurrentTouchOffsetY);
        return dst;
    }

    /**
     * @see View#scrollBy(int, int)
     * Currently the ContentView scrolling happens in the native side. In
     * the Java view system, it is always pinned at (0, 0). scrollBy() and scrollTo()
     * are overridden, so that View's mScrollX and mScrollY will be unchanged at
     * (0, 0). This is critical for drawing ContentView correctly.
     */
    public void scrollBy(float dxPix, float dyPix, boolean useLastFocalEventLocation) {
        if (mNativeContentViewCore == 0) return;
        if (dxPix == 0 && dyPix == 0) return;
        long time = SystemClock.uptimeMillis();
        // It's a very real (and valid) possibility that a fling may still
        // be active when programatically scrolling. Cancelling the fling in
        // such cases ensures a consistent gesture event stream.
        if (mPotentiallyActiveFlingCount > 0) nativeFlingCancel(mNativeContentViewCore, time);
        // x/y represents starting location of scroll.
        final float x = useLastFocalEventLocation ? mLastFocalEventX : 0f;
        final float y = useLastFocalEventLocation ? mLastFocalEventY : 0f;
        nativeScrollBegin(
                mNativeContentViewCore, time, x, y, -dxPix, -dyPix, !useLastFocalEventLocation);
        nativeScrollBy(mNativeContentViewCore, time, x, y, dxPix, dyPix);
        nativeScrollEnd(mNativeContentViewCore, time);
    }

    /**
     * @see View#scrollTo(int, int)
     */
    public void scrollTo(float xPix, float yPix) {
        if (mNativeContentViewCore == 0) return;
        final float xCurrentPix = mRenderCoordinates.getScrollXPix();
        final float yCurrentPix = mRenderCoordinates.getScrollYPix();
        final float dxPix = xPix - xCurrentPix;
        final float dyPix = yPix - yCurrentPix;
        scrollBy(dxPix, dyPix, false);
    }

    // NOTE: this can go away once ContentView.getScrollX() reports correct values.
    //       see: b/6029133
    public int getNativeScrollXForTest() {
        return mRenderCoordinates.getScrollXPixInt();
    }

    // NOTE: this can go away once ContentView.getScrollY() reports correct values.
    //       see: b/6029133
    public int getNativeScrollYForTest() {
        return mRenderCoordinates.getScrollYPixInt();
    }

    /**
     * @see View#computeHorizontalScrollExtent()
     */
    @SuppressWarnings("javadoc")
    public int computeHorizontalScrollExtent() {
        return mRenderCoordinates.getLastFrameViewportWidthPixInt();
    }

    /**
     * @see View#computeHorizontalScrollOffset()
     */
    @SuppressWarnings("javadoc")
    public int computeHorizontalScrollOffset() {
        return mRenderCoordinates.getScrollXPixInt();
    }

    /**
     * @see View#computeHorizontalScrollRange()
     */
    @SuppressWarnings("javadoc")
    public int computeHorizontalScrollRange() {
        return mRenderCoordinates.getContentWidthPixInt();
    }

    /**
     * @see View#computeVerticalScrollExtent()
     */
    @SuppressWarnings("javadoc")
    public int computeVerticalScrollExtent() {
        return mRenderCoordinates.getLastFrameViewportHeightPixInt();
    }

    /**
     * @see View#computeVerticalScrollOffset()
     */
    @SuppressWarnings("javadoc")
    public int computeVerticalScrollOffset() {
        return mRenderCoordinates.getScrollYPixInt();
    }

    /**
     * @see View#computeVerticalScrollRange()
     */
    @SuppressWarnings("javadoc")
    public int computeVerticalScrollRange() {
        return mRenderCoordinates.getContentHeightPixInt();
    }

    // End FrameLayout overrides.

    /**
     * @see View#awakenScrollBars(int, boolean)
     */
    @SuppressWarnings("javadoc")
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

    private void updateForTapOrPress(int type, float xPix, float yPix) {
        if (type != GestureEventType.SINGLE_TAP_CONFIRMED
                && type != GestureEventType.SINGLE_TAP_UP
                && type != GestureEventType.LONG_PRESS
                && type != GestureEventType.LONG_TAP) {
            return;
        }

        if (mContainerView.isFocusable() && mContainerView.isFocusableInTouchMode()
                && !mContainerView.isFocused())  {
            mContainerView.requestFocus();
        }

        if (!mPopupZoomer.isShowing()) mPopupZoomer.setLastTouch(xPix, yPix);
        mLastFocalEventX = xPix;
        mLastFocalEventY = yPix;
    }

    public void setZoomControlsDelegate(ZoomControlsDelegate zoomControlsDelegate) {
        if (zoomControlsDelegate == null) {
            mZoomControlsDelegate = NO_OP_ZOOM_CONTROLS_DELEGATE;
            return;
        }
        mZoomControlsDelegate = zoomControlsDelegate;
    }

    public void updateMultiTouchZoomSupport(boolean supportsMultiTouchZoom) {
        if (mNativeContentViewCore == 0) return;
        nativeSetMultiTouchZoomSupportEnabled(mNativeContentViewCore, supportsMultiTouchZoom);
    }

    public void updateDoubleTapSupport(boolean supportsDoubleTap) {
        if (mNativeContentViewCore == 0) return;
        nativeSetDoubleTapSupportEnabled(mNativeContentViewCore, supportsDoubleTap);
    }

    public void selectPopupMenuItems(int[] indices) {
        if (mNativeContentViewCore != 0) {
            nativeSelectPopupMenuItems(mNativeContentViewCore, mNativeSelectPopupSourceFrame,
                                       indices);
        }
        mNativeSelectPopupSourceFrame = 0;
        mSelectPopup = null;
    }

    /**
     * Send the screen orientation value to the renderer.
     */
    @VisibleForTesting
    void sendOrientationChangeEvent(int orientation) {
        if (mNativeContentViewCore == 0) return;

        nativeSendOrientationChangeEvent(mNativeContentViewCore, orientation);
    }

    /**
     * Register the delegate to be used when content can not be handled by
     * the rendering engine, and should be downloaded instead. This will replace
     * the current delegate, if any.
     * @param delegate An implementation of ContentViewDownloadDelegate.
     */
    public void setDownloadDelegate(ContentViewDownloadDelegate delegate) {
        mDownloadDelegate = delegate;
    }

    // Called by DownloadController.
    ContentViewDownloadDelegate getDownloadDelegate() {
        return mDownloadDelegate;
    }

    @VisibleForTesting
    public WebActionModeCallback.ActionHandler getSelectActionHandler() {
        return mActionHandler;
    }

    private void showSelectActionMode(boolean allowFallbackIfFloatingActionModeCreationFails) {
        if (mActionMode != null) {
            mActionMode.invalidate();
            return;
        }

        // Start a new action mode with a WebActionModeCallback.
        if (mActionHandler == null) {
            mActionHandler = new WebActionModeCallback.ActionHandler() {
                /**
                 * Android Intent size limitations prevent sending over a megabyte of data. Limit
                 * query lengths to 100kB because other things may be added to the Intent.
                 */
                private static final int MAX_SHARE_QUERY_LENGTH = 100000;

                /** Google search doesn't support requests slightly larger than this. */
                private static final int MAX_SEARCH_QUERY_LENGTH = 1000;

                @Override
                public void selectAll() {
                    mWebContents.selectAll();
                }

                @Override
                public void cut() {
                    mWebContents.cut();
                }

                @Override
                public void copy() {
                    mWebContents.copy();
                }

                @Override
                public void paste() {
                    mWebContents.paste();
                }

                @Override
                public void share() {
                    final String query = sanitizeQuery(getSelectedText(), MAX_SHARE_QUERY_LENGTH);
                    if (TextUtils.isEmpty(query)) return;

                    Intent send = new Intent(Intent.ACTION_SEND);
                    send.setType("text/plain");
                    send.putExtra(Intent.EXTRA_TEXT, query);
                    try {
                        Intent i = Intent.createChooser(send, getContext().getString(
                                R.string.actionbar_share));
                        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        getContext().startActivity(i);
                    } catch (android.content.ActivityNotFoundException ex) {
                        // If no app handles it, do nothing.
                    }
                }

                @Override
                public void processText(Intent intent) {
                    assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;

                    final String query = sanitizeQuery(getSelectedText(), MAX_SEARCH_QUERY_LENGTH);
                    if (TextUtils.isEmpty(query)) return;

                    intent.putExtra(Intent.EXTRA_PROCESS_TEXT, query);
                    try {
                        if (getContentViewClient().doesPerformProcessText()) {
                            getContentViewClient().startProcessTextIntent(intent);
                        } else {
                            getWindowAndroid().showIntent(
                                    intent, new WindowAndroid.IntentCallback() {
                                        @Override
                                        public void onIntentCompleted(WindowAndroid window,
                                                int resultCode, ContentResolver contentResolver,
                                                Intent data) {
                                            onReceivedProcessTextResult(resultCode, data);
                                        }
                                    }, null);
                        }
                    } catch (android.content.ActivityNotFoundException ex) {
                        // If no app handles it, do nothing.
                    }
                }

                @Override
                public void search() {
                    final String query = sanitizeQuery(getSelectedText(), MAX_SEARCH_QUERY_LENGTH);
                    if (TextUtils.isEmpty(query)) return;

                    // See if ContentViewClient wants to override.
                    if (getContentViewClient().doesPerformWebSearch()) {
                        getContentViewClient().performWebSearch(query);
                        return;
                    }

                    Intent i = new Intent(Intent.ACTION_WEB_SEARCH);
                    i.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
                    i.putExtra(SearchManager.QUERY, query);
                    i.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
                    i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    try {
                        getContext().startActivity(i);
                    } catch (android.content.ActivityNotFoundException ex) {
                        // If no app handles it, do nothing.
                    }
                }

                @Override
                public boolean isSelectionPassword() {
                    return mFocusedNodeIsPassword;
                }

                @Override
                public boolean isSelectionEditable() {
                    return mFocusedNodeEditable;
                }

                @Override
                public boolean isInsertion() {
                    return mHasInsertion;
                }

                @Override
                public void onDestroyActionMode() {
                    mActionMode = null;
                    if (mUnselectAllOnActionModeDismiss) {
                        dismissTextHandles();
                        clearSelection();
                    }
                    if (!supportsFloatingActionMode()) {
                        getContentViewClient().onContextualActionBarHidden();
                    }
                }

                @Override
                public void onGetContentRect(Rect outRect) {
                    // The selection coordinates are relative to the content viewport, but we need
                    // coordinates relative to the containing View.
                    outRect.set(mSelectionRect);
                    outRect.offset(0, (int) mRenderCoordinates.getContentOffsetYPix());
                }

                @Override
                public boolean isIncognito() {
                    return mWebContents.isIncognito();
                }

                @Override
                public boolean isSelectActionModeAllowed(int actionModeItem) {
                    boolean isAllowedByClient =
                            getContentViewClient().isSelectActionModeAllowed(actionModeItem);
                    if (actionModeItem == WebActionModeCallback.MENU_ITEM_SHARE) {
                        return isAllowedByClient && isShareAvailable();
                    }

                    if (actionModeItem == WebActionModeCallback.MENU_ITEM_WEB_SEARCH) {
                        return isAllowedByClient && isWebSearchAvailable();
                    }

                    return isAllowedByClient;
                }

                private boolean isShareAvailable() {
                    Intent intent = new Intent(Intent.ACTION_SEND);
                    intent.setType("text/plain");
                    return getContext().getPackageManager().queryIntentActivities(intent,
                            PackageManager.MATCH_DEFAULT_ONLY).size() > 0;
                }

                private boolean isWebSearchAvailable() {
                    if (getContentViewClient().doesPerformWebSearch()) return true;
                    Intent intent = new Intent(Intent.ACTION_WEB_SEARCH);
                    intent.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
                    return getContext().getPackageManager().queryIntentActivities(intent,
                            PackageManager.MATCH_DEFAULT_ONLY).size() > 0;
                }

                private String sanitizeQuery(String query, int maxLength) {
                    if (TextUtils.isEmpty(query) || query.length() < maxLength) return query;
                    Log.w(TAG, "Truncating oversized query (" + query.length() + ").");
                    return query.substring(0, maxLength) + "…";
                }
            };
        }
        mActionMode = null;
        // On ICS, startActionMode throws an NPE when getParent() is null.
        if (mContainerView.getParent() != null) {
            assert mWebContents != null;
            ActionMode actionMode = startActionMode(allowFallbackIfFloatingActionModeCreationFails);
            if (actionMode != null) mActionMode = new WebActionMode(actionMode, mContainerView);
        }
        mUnselectAllOnActionModeDismiss = true;
        if (mActionMode == null) {
            // There is no ActionMode, so remove the selection.
            clearSelection();
        } else {
            // TODO(jdduke): Refactor ContentViewClient.onContextualActionBarShown to be aware of
            // non-action-bar (i.e., floating) ActionMode instances, crbug.com/524666.
            if (!supportsFloatingActionMode()) {
                getContentViewClient().onContextualActionBarShown();
            }
        }
    }

    private boolean supportsFloatingActionMode() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        return !mFloatingActionModeCreationFailed;
    }

    private ActionMode startActionMode(boolean allowFallbackIfFloatingActionModeCreationFails) {
        WebActionModeCallback callback =
                new WebActionModeCallback(mContainerView.getContext(), mActionHandler);
        if (supportsFloatingActionMode()) {
            ActionMode actionMode = startFloatingActionMode(callback);
            if (actionMode != null) return actionMode;
            mFloatingActionModeCreationFailed = true;
            if (!allowFallbackIfFloatingActionModeCreationFails) return null;
        }
        return startDefaultActionMode(callback);
    }

    private ActionMode startDefaultActionMode(WebActionModeCallback callback) {
        return mContainerView.startActionMode(callback);
    }

    @TargetApi(Build.VERSION_CODES.M)
    private ActionMode startFloatingActionMode(WebActionModeCallback callback) {
        ActionMode.Callback2 callback2 = new FloatingWebActionModeCallback(callback);
        return mContainerView.startActionMode(callback2, ActionMode.TYPE_FLOATING);
    }

    private void invalidateActionModeContentRect() {
        if (mActionMode != null) mActionMode.invalidateContentRect();
    }

    private void updateActionModeVisibility() {
        if (mActionMode == null) return;
        // The active fling count isn't reliable with WebView, so only use the
        // active touch scroll signal for hiding. The fling animation movement
        // will naturally hide the ActionMode by invalidating its content rect.
        mActionMode.hide(mDraggingSelection || mTouchScrollInProgress);
    }

    /**
     * Clears the current text selection. Note that we will try to move cursor to selection
     * end if applicable.
     */
    public void clearSelection() {
        if (mFocusedNodeEditable) {
            mImeAdapter.moveCursorToSelectionEnd();
        } else {
            // This method can be called during shutdown, guard against null accordingly.
            if (mWebContents != null) mWebContents.unselect();
        }
    }

    /**
     * Ensure the selection is preserved the next time the view loses focus.
     */
    public void preserveSelectionOnNextLossOfFocus() {
        mPreserveSelectionOnNextLossOfFocus = true;
    }

    /**
     * @return Whether the page has an active, touch-controlled selection region.
     */
    @VisibleForTesting
    public boolean hasSelection() {
        return mHasSelection;
    }

    /**
     * @return Whether the page has an active, touch-controlled insertion handle.
     */
    @VisibleForTesting
    public boolean hasInsertion() {
        return mHasInsertion;
    }

    @CalledByNative
    private void onSelectionEvent(
            int eventType, int xAnchor, int yAnchor, int left, int top, int right, int bottom) {
        // Ensure the provided selection coordinates form a non-empty rect, as required by
        // the selection action mode.
        if (left == right) ++right;
        if (top == bottom) ++bottom;
        switch (eventType) {
            case SelectionEventType.SELECTION_HANDLES_SHOWN:
                mSelectionRect.set(left, top, right, bottom);
                mHasSelection = true;
                mUnselectAllOnActionModeDismiss = true;
                // TODO(cjhopman): Remove this when there is a better signal that long press caused
                // a selection. See http://crbug.com/150151.
                mContainerView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
                showSelectActionMode(true);
                break;

            case SelectionEventType.SELECTION_HANDLES_MOVED:
                mSelectionRect.set(left, top, right, bottom);
                invalidateActionModeContentRect();
                break;

            case SelectionEventType.SELECTION_HANDLES_CLEARED:
                mHasSelection = false;
                mDraggingSelection = false;
                mUnselectAllOnActionModeDismiss = false;
                hideSelectActionMode();
                mSelectionRect.setEmpty();
                break;

            case SelectionEventType.SELECTION_HANDLE_DRAG_STARTED:
                mDraggingSelection = true;
                updateActionModeVisibility();
                break;

            case SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED:
                mDraggingSelection = false;
                updateActionModeVisibility();
                break;

            case SelectionEventType.INSERTION_HANDLE_SHOWN:
                mSelectionRect.set(left, top, right, bottom);
                mHasInsertion = true;
                break;

            case SelectionEventType.INSERTION_HANDLE_MOVED:
                mSelectionRect.set(left, top, right, bottom);
                if (!isScrollInProgress() && isPastePopupShowing()) {
                    showPastePopup(xAnchor, yAnchor);
                } else {
                    hidePastePopup();
                }
                break;

            case SelectionEventType.INSERTION_HANDLE_TAPPED:
                if (mWasPastePopupShowingOnInsertionDragStart) {
                    hidePastePopup();
                } else {
                    showPastePopup(xAnchor, yAnchor);
                }
                mWasPastePopupShowingOnInsertionDragStart = false;
                break;

            case SelectionEventType.INSERTION_HANDLE_CLEARED:
                hidePastePopup();
                mHasInsertion = false;
                mSelectionRect.setEmpty();
                break;

            case SelectionEventType.INSERTION_HANDLE_DRAG_STARTED:
                mWasPastePopupShowingOnInsertionDragStart = isPastePopupShowing();
                hidePastePopup();
                break;

            case SelectionEventType.INSERTION_HANDLE_DRAG_STOPPED:
                if (mWasPastePopupShowingOnInsertionDragStart) {
                    showPastePopup(xAnchor, yAnchor);
                }
                mWasPastePopupShowingOnInsertionDragStart = false;
                break;
            case SelectionEventType.SELECTION_ESTABLISHED:
            case SelectionEventType.SELECTION_DISSOLVED:
                break;

            default:
                assert false : "Invalid selection event type.";
        }
        if (mContextualSearchClient != null) {
            mContextualSearchClient.onSelectionEvent(eventType, xAnchor, yAnchor);
        }
    }

    private void dismissTextHandles() {
        if (mNativeContentViewCore != 0) nativeDismissTextHandles(mNativeContentViewCore);
    }

    private void setTextHandlesTemporarilyHidden(boolean hide) {
        if (mNativeContentViewCore == 0) return;
        nativeSetTextHandlesTemporarilyHidden(mNativeContentViewCore, hide);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void updateFrameInfo(
            float scrollOffsetX, float scrollOffsetY,
            float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor,
            float contentWidth, float contentHeight,
            float viewportWidth, float viewportHeight,
            float controlsOffsetYCss, float contentOffsetYCss,
            boolean isMobileOptimizedHint) {
        TraceEvent.begin("ContentViewCore:updateFrameInfo");
        mIsMobileOptimizedHint = isMobileOptimizedHint;
        // Adjust contentWidth/Height to be always at least as big as
        // the actual viewport (as set by onSizeChanged).
        final float deviceScale = mRenderCoordinates.getDeviceScaleFactor();
        contentWidth = Math.max(contentWidth,
                mViewportWidthPix / (deviceScale * pageScaleFactor));
        contentHeight = Math.max(contentHeight,
                mViewportHeightPix / (deviceScale * pageScaleFactor));
        final float contentOffsetYPix = mRenderCoordinates.fromDipToPix(contentOffsetYCss);

        final boolean contentSizeChanged =
                contentWidth != mRenderCoordinates.getContentWidthCss()
                || contentHeight != mRenderCoordinates.getContentHeightCss();
        final boolean scaleLimitsChanged =
                minPageScaleFactor != mRenderCoordinates.getMinPageScaleFactor()
                || maxPageScaleFactor != mRenderCoordinates.getMaxPageScaleFactor();
        final boolean pageScaleChanged =
                pageScaleFactor != mRenderCoordinates.getPageScaleFactor();
        final boolean scrollChanged =
                pageScaleChanged
                || scrollOffsetX != mRenderCoordinates.getScrollX()
                || scrollOffsetY != mRenderCoordinates.getScrollY();
        final boolean contentOffsetChanged =
                contentOffsetYPix != mRenderCoordinates.getContentOffsetYPix();

        final boolean needHidePopupZoomer = contentSizeChanged || scrollChanged;
        final boolean needUpdateZoomControls = scaleLimitsChanged || scrollChanged;

        if (needHidePopupZoomer) mPopupZoomer.hide(true);

        if (scrollChanged) {
            mContainerViewInternals.onScrollChanged(
                    (int) mRenderCoordinates.fromLocalCssToPix(scrollOffsetX),
                    (int) mRenderCoordinates.fromLocalCssToPix(scrollOffsetY),
                    (int) mRenderCoordinates.getScrollXPix(),
                    (int) mRenderCoordinates.getScrollYPix());
        }

        mRenderCoordinates.updateFrameInfo(
                scrollOffsetX, scrollOffsetY,
                contentWidth, contentHeight,
                viewportWidth, viewportHeight,
                pageScaleFactor, minPageScaleFactor, maxPageScaleFactor,
                contentOffsetYPix);

        if (scrollChanged || contentOffsetChanged) {
            for (mGestureStateListenersIterator.rewind();
                    mGestureStateListenersIterator.hasNext();) {
                mGestureStateListenersIterator.next().onScrollOffsetOrExtentChanged(
                        computeVerticalScrollOffset(),
                        computeVerticalScrollExtent());
            }
        }

        if (needUpdateZoomControls) mZoomControlsDelegate.updateZoomControls();

        // Update offsets for fullscreen.
        final float controlsOffsetPix = controlsOffsetYCss * deviceScale;
        // TODO(aelias): Remove last argument after downstream removes it.
        getContentViewClient().onOffsetsForFullscreenChanged(
                controlsOffsetPix, contentOffsetYPix);

        if (mBrowserAccessibilityManager != null) {
            mBrowserAccessibilityManager.notifyFrameInfoInitialized();
        }
        TraceEvent.end("ContentViewCore:updateFrameInfo");
    }

    @CalledByNative
    private void updateImeAdapter(long nativeImeAdapterAndroid, int textInputType,
            int textInputFlags, String text, int selectionStart, int selectionEnd,
            int compositionStart, int compositionEnd, boolean showImeIfNeeded,
            boolean isNonImeChange) {
        try {
            TraceEvent.begin("ContentViewCore.updateImeAdapter");
            boolean focusedNodeEditable = (textInputType != TextInputType.NONE);
            boolean focusedNodeIsPassword = (textInputType == TextInputType.PASSWORD);
            if (!focusedNodeEditable) hidePastePopup();

            mImeAdapter.attach(nativeImeAdapterAndroid);
            mImeAdapter.updateKeyboardVisibility(
                    textInputType, textInputFlags, showImeIfNeeded);
            mImeAdapter.updateState(text, selectionStart, selectionEnd, compositionStart,
                    compositionEnd, isNonImeChange);

            if (mActionMode != null) {
                final boolean actionModeConfigurationChanged =
                        focusedNodeEditable != mFocusedNodeEditable
                        || focusedNodeIsPassword != mFocusedNodeIsPassword;
                if (actionModeConfigurationChanged) mActionMode.invalidate();
            }

            mFocusedNodeIsPassword = focusedNodeIsPassword;
            if (focusedNodeEditable != mFocusedNodeEditable) {
                mFocusedNodeEditable = focusedNodeEditable;
                mJoystickScrollProvider.setEnabled(!mFocusedNodeEditable);
                getContentViewClient().onFocusedNodeEditabilityChanged(mFocusedNodeEditable);
            }
        } finally {
            TraceEvent.end("ContentViewCore.updateImeAdapter");
        }
    }

    @CalledByNative
    private void forceUpdateImeAdapter(long nativeImeAdapterAndroid) {
        mImeAdapter.attach(nativeImeAdapterAndroid);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void setTitle(String title) {
        getContentViewClient().onUpdateTitle(title);
    }

    /**
     * Called (from native) when the <select> popup needs to be shown.
     * @param nativeSelectPopupSourceFrame The native RenderFrameHost that owns the popup.
     * @param items           Items to show.
     * @param enabled         POPUP_ITEM_TYPEs for items.
     * @param multiple        Whether the popup menu should support multi-select.
     * @param selectedIndices Indices of selected items.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void showSelectPopup(long nativeSelectPopupSourceFrame, Rect bounds, String[] items,
            int[] enabled, boolean multiple, int[] selectedIndices, boolean rightAligned) {
        if (mContainerView.getParent() == null || mContainerView.getVisibility() != View.VISIBLE) {
            mNativeSelectPopupSourceFrame = nativeSelectPopupSourceFrame;
            selectPopupMenuItems(null);
            return;
        }

        hidePopupsAndClearSelection();
        assert mNativeSelectPopupSourceFrame == 0 : "Zombie popup did not clear the frame source";

        assert items.length == enabled.length;
        List<SelectPopupItem> popupItems = new ArrayList<SelectPopupItem>();
        for (int i = 0; i < items.length; i++) {
            popupItems.add(new SelectPopupItem(items[i], enabled[i]));
        }
        if (DeviceFormFactor.isTablet(mContext) && !multiple && !isTouchExplorationEnabled()) {
            mSelectPopup = new SelectPopupDropdown(
                    this, popupItems, bounds, selectedIndices, rightAligned);
        } else {
            if (getWindowAndroid() == null) return;
            Context windowContext = getWindowAndroid().getContext().get();
            if (windowContext == null) return;
            mSelectPopup = new SelectPopupDialog(
                    this, windowContext, popupItems, multiple, selectedIndices);
        }
        mNativeSelectPopupSourceFrame = nativeSelectPopupSourceFrame;
        mSelectPopup.show();
    }

    /**
     * Called when the <select> popup needs to be hidden.
     */
    @CalledByNative
    private void hideSelectPopup() {
        if (mSelectPopup != null) mSelectPopup.hide();
    }

    /**
     * @return The visible select popup being shown.
     */
    @VisibleForTesting
    public SelectPopup getSelectPopupForTest() {
        return mSelectPopup;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void showDisambiguationPopup(Rect targetRect, Bitmap zoomedBitmap) {
        mPopupZoomer.setBitmap(zoomedBitmap);
        mPopupZoomer.show(targetRect);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private MotionEventSynthesizer createMotionEventSynthesizer() {
        return new MotionEventSynthesizer(this);
    }

    /**
     * Initialize the view with an overscroll refresh handler.
     * @param handler The refresh handler.
     */
    public void setOverscrollRefreshHandler(OverscrollRefreshHandler handler) {
        assert mOverscrollRefreshHandler == null || handler == null;
        mOverscrollRefreshHandler = handler;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private boolean onOverscrollRefreshStart() {
        if (mOverscrollRefreshHandler == null) return false;
        return mOverscrollRefreshHandler.start();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onOverscrollRefreshUpdate(float delta) {
        if (mOverscrollRefreshHandler != null) mOverscrollRefreshHandler.pull(delta);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onOverscrollRefreshRelease(boolean allowRefresh) {
        if (mOverscrollRefreshHandler != null) mOverscrollRefreshHandler.release(allowRefresh);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onOverscrollRefreshReset() {
        if (mOverscrollRefreshHandler != null) mOverscrollRefreshHandler.reset();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onSelectionChanged(String text) {
        mLastSelectedText = text;
        if (mContextualSearchClient != null) {
            mContextualSearchClient.onSelectionChanged(text);
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private boolean showPastePopupWithFeedback(int x, int y) {
        // TODO(jdduke): Remove this when there is a better signal that long press caused
        // showing of the paste popup. See http://crbug.com/150151.
        if (showPastePopup(x, y)) {
            mContainerView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
            if (mWebContents != null) mWebContents.onContextMenuOpened();
            return true;
        } else {
            return false;
        }
    }

    @VisibleForTesting
    public boolean isPastePopupShowing() {
        if (mPastePopupMenu != null) return mPastePopupMenu.isShowing();
        return false;
    }

    private boolean showPastePopup(int x, int y) {
        if (mContainerView.getParent() == null || mContainerView.getVisibility() != View.VISIBLE) {
            return false;
        }

        if (!mHasInsertion || !canPaste()) return false;
        final float contentOffsetYPix = mRenderCoordinates.getContentOffsetYPix();
        getPastePopup().show(x, (int) (y + contentOffsetYPix));
        return true;
    }

    private void hidePastePopup() {
        if (mPastePopupMenu != null) mPastePopupMenu.hide();
    }

    private PastePopupMenu getPastePopup() {
        if (mPastePopupMenu == null) {
            PastePopupMenuDelegate delegate = new PastePopupMenuDelegate() {
                @Override
                public void paste() {
                    mWebContents.paste();
                    dismissTextHandles();
                }

                @Override
                public void onDismiss() {
                    if (mWebContents != null) mWebContents.onContextMenuClosed();
                }
            };
            if (supportsFloatingActionMode()) {
                mPastePopupMenu = new FloatingPastePopupMenu(getContainerView(), delegate);
            } else {
                mPastePopupMenu = new LegacyPastePopupMenu(getContainerView(), delegate);
            }
        }
        return mPastePopupMenu;
    }

    private boolean canPaste() {
        return ((ClipboardManager) mContext.getSystemService(
                Context.CLIPBOARD_SERVICE)).hasPrimaryClip();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRenderProcessChange() {
        attachImeAdapter();
        // Immediately sync closed caption settings to the new render process.
        mSystemCaptioningBridge.syncToListener(this);
    }

    /**
     * Attaches the native ImeAdapter object to the java ImeAdapter to allow communication via JNI.
     */
    public void attachImeAdapter() {
        if (mImeAdapter != null && mNativeContentViewCore != 0) {
            mImeAdapter.attach(nativeGetNativeImeAdapter(mNativeContentViewCore));
        }
    }

    /**
     * @see View#hasFocus()
     */
    @CalledByNative
    private boolean hasFocus() {
        // If the container view is not focusable, we consider it always focused from
        // Chromium's point of view.
        if (!mContainerView.isFocusable()) return true;
        return mContainerView.hasFocus();
    }

    /**
     * Checks whether the ContentViewCore can be zoomed in.
     *
     * @return True if the ContentViewCore can be zoomed in.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomIn() {
        final float zoomInExtent = mRenderCoordinates.getMaxPageScaleFactor()
                - mRenderCoordinates.getPageScaleFactor();
        return zoomInExtent > ZOOM_CONTROLS_EPSILON;
    }

    /**
     * Checks whether the ContentViewCore can be zoomed out.
     *
     * @return True if the ContentViewCore can be zoomed out.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomOut() {
        final float zoomOutExtent = mRenderCoordinates.getPageScaleFactor()
                - mRenderCoordinates.getMinPageScaleFactor();
        return zoomOutExtent > ZOOM_CONTROLS_EPSILON;
    }

    /**
     * Zooms in the ContentViewCore by 25% (or less if that would result in
     * zooming in more than possible).
     *
     * @return True if there was a zoom change, false otherwise.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomIn() {
        if (!canZoomIn()) {
            return false;
        }
        return pinchByDelta(1.25f);
    }

    /**
     * Zooms out the ContentViewCore by 20% (or less if that would result in
     * zooming out more than possible).
     *
     * @return True if there was a zoom change, false otherwise.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomOut() {
        if (!canZoomOut()) {
            return false;
        }
        return pinchByDelta(0.8f);
    }

    /**
     * Resets the zoom factor of the ContentViewCore.
     *
     * @return True if there was a zoom change, false otherwise.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean zoomReset() {
        // The page scale factor is initialized to mNativeMinimumScale when
        // the page finishes loading. Thus sets it back to mNativeMinimumScale.
        if (!canZoomOut()) return false;
        return pinchByDelta(
                mRenderCoordinates.getMinPageScaleFactor()
                        / mRenderCoordinates.getPageScaleFactor());
    }

    /**
     * Simulate a pinch zoom gesture.
     *
     * @param delta the factor by which the current page scale should be multiplied by.
     * @return whether the gesture was sent.
     */
    public boolean pinchByDelta(float delta) {
        if (mNativeContentViewCore == 0) return false;

        long timeMs = SystemClock.uptimeMillis();
        int xPix = getViewportWidthPix() / 2;
        int yPix = getViewportHeightPix() / 2;

        nativePinchBegin(mNativeContentViewCore, timeMs, xPix, yPix);
        nativePinchBy(mNativeContentViewCore, timeMs, xPix, yPix, delta);
        nativePinchEnd(mNativeContentViewCore, timeMs);

        return true;
    }

    /**
     * Invokes the graphical zoom picker widget for this ContentView.
     */
    public void invokeZoomPicker() {
        mZoomControlsDelegate.invokeZoomPicker();
    }

    /**
     * Enables or disables inspection of JavaScript objects added via
     * {@link #addJavascriptInterface(Object, String)} by means of Object.keys() method and
     * &quot;for .. in&quot; loop. Being able to inspect JavaScript objects is useful
     * when debugging hybrid Android apps, but can't be enabled for legacy applications due
     * to compatibility risks.
     *
     * @param allow Whether to allow JavaScript objects inspection.
     */
    public void setAllowJavascriptInterfacesInspection(boolean allow) {
        nativeSetAllowJavascriptInterfacesInspection(mNativeContentViewCore, allow);
    }

    /**
     * Returns JavaScript interface objects previously injected via
     * {@link #addJavascriptInterface(Object, String)}.
     *
     * @return the mapping of names to interface objects and corresponding annotation classes
     */
    public Map<String, Pair<Object, Class>> getJavascriptInterfaces() {
        return mJavaScriptInterfaces;
    }

    /**
     * This will mimic {@link #addPossiblyUnsafeJavascriptInterface(Object, String, Class)}
     * and automatically pass in {@link JavascriptInterface} as the required annotation.
     *
     * @param object The Java object to inject into the ContentViewCore's JavaScript context.  Null
     *               values are ignored.
     * @param name   The name used to expose the instance in JavaScript.
     */
    public void addJavascriptInterface(Object object, String name) {
        addPossiblyUnsafeJavascriptInterface(object, name, JavascriptInterface.class);
    }

    /**
     * This method injects the supplied Java object into the ContentViewCore.
     * The object is injected into the JavaScript context of the main frame,
     * using the supplied name. This allows the Java object to be accessed from
     * JavaScript. Note that that injected objects will not appear in
     * JavaScript until the page is next (re)loaded. For example:
     * <pre> view.addJavascriptInterface(new Object(), "injectedObject");
     * view.loadData("<!DOCTYPE html><title></title>", "text/html", null);
     * view.loadUrl("javascript:alert(injectedObject.toString())");</pre>
     * <p><strong>IMPORTANT:</strong>
     * <ul>
     * <li> addJavascriptInterface() can be used to allow JavaScript to control
     * the host application. This is a powerful feature, but also presents a
     * security risk. Use of this method in a ContentViewCore containing
     * untrusted content could allow an attacker to manipulate the host
     * application in unintended ways, executing Java code with the permissions
     * of the host application. Use extreme care when using this method in a
     * ContentViewCore which could contain untrusted content. Particular care
     * should be taken to avoid unintentional access to inherited methods, such
     * as {@link Object#getClass()}. To prevent access to inherited methods,
     * pass an annotation for {@code requiredAnnotation}.  This will ensure
     * that only methods with {@code requiredAnnotation} are exposed to the
     * Javascript layer.  {@code requiredAnnotation} will be passed to all
     * subsequently injected Java objects if any methods return an object.  This
     * means the same restrictions (or lack thereof) will apply.  Alternatively,
     * {@link #addJavascriptInterface(Object, String)} can be called, which
     * automatically uses the {@link JavascriptInterface} annotation.
     * <li> JavaScript interacts with Java objects on a private, background
     * thread of the ContentViewCore. Care is therefore required to maintain
     * thread safety.</li>
     * </ul></p>
     *
     * @param object             The Java object to inject into the
     *                           ContentViewCore's JavaScript context. Null
     *                           values are ignored.
     * @param name               The name used to expose the instance in
     *                           JavaScript.
     * @param requiredAnnotation Restrict exposed methods to ones with this
     *                           annotation.  If {@code null} all methods are
     *                           exposed.
     *
     */
    public void addPossiblyUnsafeJavascriptInterface(Object object, String name,
            Class<? extends Annotation> requiredAnnotation) {
        if (mNativeContentViewCore != 0 && object != null) {
            mJavaScriptInterfaces.put(name, new Pair<Object, Class>(object, requiredAnnotation));
            nativeAddJavascriptInterface(mNativeContentViewCore, object, name, requiredAnnotation);
        }
    }

    /**
     * Removes a previously added JavaScript interface with the given name.
     *
     * @param name The name of the interface to remove.
     */
    public void removeJavascriptInterface(String name) {
        mJavaScriptInterfaces.remove(name);
        if (mNativeContentViewCore != 0) {
            nativeRemoveJavascriptInterface(mNativeContentViewCore, name);
        }
    }

    /**
     * Return the current scale of the ContentView.
     * @return The current page scale factor.
     */
    @VisibleForTesting
    public float getScale() {
        return mRenderCoordinates.getPageScaleFactor();
    }

    @CalledByNative
    private void startContentIntent(String contentUrl, boolean isMainFrame) {
        getContentViewClient().onStartContentIntent(getContext(), contentUrl, isMainFrame);
    }

    @Override
    public void onAccessibilityStateChanged(boolean enabled) {
        setAccessibilityState(enabled);
    }

    /**
     * Determines whether or not this ContentViewCore can handle this accessibility action.
     * @param action The action to perform.
     * @return Whether or not this action is supported.
     */
    public boolean supportsAccessibilityAction(int action) {
        // TODO(dmazzoni): implement this in BrowserAccessibilityManager.
        return false;
    }

    /**
     * Attempts to perform an accessibility action on the web content.  If the accessibility action
     * cannot be processed, it returns {@code null}, allowing the caller to know to call the
     * super {@link View#performAccessibilityAction(int, Bundle)} method and use that return value.
     * Otherwise the return value from this method should be used.
     * @param action The action to perform.
     * @param arguments Optional action arguments.
     * @return Whether the action was performed or {@code null} if the call should be delegated to
     *         the super {@link View} class.
     */
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        // TODO(dmazzoni): implement this in BrowserAccessibilityManager.
        return false;
    }

    /**
     * Set the BrowserAccessibilityManager, used for native accessibility
     * (not script injection). This is only set when system accessibility
     * has been enabled.
     * @param manager The new BrowserAccessibilityManager.
     */
    public void setBrowserAccessibilityManager(BrowserAccessibilityManager manager) {
        mBrowserAccessibilityManager = manager;

        if (mBrowserAccessibilityManager != null && mRenderCoordinates.hasFrameInfo()) {
            mBrowserAccessibilityManager.notifyFrameInfoInitialized();
        }
    }

    /**
     * Get the BrowserAccessibilityManager, used for native accessibility
     * (not script injection). This will return null when system accessibility
     * is not enabled.
     * @return This view's BrowserAccessibilityManager.
     */
    public BrowserAccessibilityManager getBrowserAccessibilityManager() {
        return mBrowserAccessibilityManager;
    }

    /**
     * If native accessibility (not script injection) is enabled, and if this is
     * running on JellyBean or later, returns an AccessibilityNodeProvider that
     * implements native accessibility for this view. Returns null otherwise.
     * Lazily initializes native accessibility here if it's allowed.
     * @return The AccessibilityNodeProvider, if available, or null otherwise.
     */
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        if (mBrowserAccessibilityManager != null) {
            return mBrowserAccessibilityManager.getAccessibilityNodeProvider();
        }

        if (mNativeAccessibilityAllowed && !mNativeAccessibilityEnabled
                && mNativeContentViewCore != 0) {
            mNativeAccessibilityEnabled = true;
            nativeSetAccessibilityEnabled(mNativeContentViewCore, true);
        }

        return null;
    }

    @TargetApi(Build.VERSION_CODES.M)
    public void onProvideVirtualStructure(final ViewStructure structure) {
        // Do not collect accessibility tree in incognito mode
        if (getWebContents().isIncognito()) {
            structure.setChildCount(0);
            return;
        }

        structure.setChildCount(1);
        final ViewStructure viewRoot = structure.asyncNewChild(0);
        getWebContents().requestAccessibilitySnapshot(
                new AccessibilitySnapshotCallback() {
                    @Override
                    public void onAccessibilitySnapshot(AccessibilitySnapshotNode root) {
                        viewRoot.setClassName("");
                        if (root == null) {
                            viewRoot.asyncCommit();
                            return;
                        }
                        createVirtualStructure(viewRoot, root, 0, 0);
                    }
                },
                mRenderCoordinates.getContentOffsetYPix(),
                mRenderCoordinates.getScrollXPix());
    }

    // When creating the View structure, the left and top are relative to the parent node.
    // The X scroll is not used, rather compensated through X-position, while the Y scroll
    // is provided.
    @TargetApi(Build.VERSION_CODES.M)
    private void createVirtualStructure(ViewStructure viewNode, AccessibilitySnapshotNode node,
            int parentX, int parentY) {
        viewNode.setClassName(node.className);
        if (node.hasSelection) {
            viewNode.setText(node.text, node.startSelection, node.endSelection);
        } else {
            viewNode.setText(node.text);
        }
        viewNode.setDimens(node.x - parentX - node.scrollX, node.y - parentY, 0, node.scrollY,
                node.width, node.height);
        viewNode.setChildCount(node.children.size());
        if (node.hasStyle) {
            int style = (node.bold ? ViewNode.TEXT_STYLE_BOLD : 0)
                    | (node.italic ? ViewNode.TEXT_STYLE_ITALIC : 0)
                    | (node.underline ? ViewNode.TEXT_STYLE_UNDERLINE : 0)
                    | (node.lineThrough ? ViewNode.TEXT_STYLE_STRIKE_THRU : 0);
            viewNode.setTextStyle(node.textSize, node.color, node.bgcolor, style);
        }
        for (int i = 0; i < node.children.size(); i++) {
            createVirtualStructure(viewNode.asyncNewChild(i), node.children.get(i), node.x,
                    node.y);
        }
        viewNode.asyncCommit();
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    @Override
    public void onSystemCaptioningChanged(TextTrackSettings settings) {
        if (mNativeContentViewCore == 0) return;
        nativeSetTextTrackSettings(mNativeContentViewCore,
                settings.getTextTracksEnabled(), settings.getTextTrackBackgroundColor(),
                settings.getTextTrackFontFamily(), settings.getTextTrackFontStyle(),
                settings.getTextTrackFontVariant(), settings.getTextTrackTextColor(),
                settings.getTextTrackTextShadow(), settings.getTextTrackTextSize());
    }

    /**
     * Called when the processed text is replied from an activity that supports
     * Intent.ACTION_PROCESS_TEXT.
     * @param resultCode the code that indicates if the activity successfully processed the text
     * @param data the reply that contains the processed text.
     */
    public void onReceivedProcessTextResult(int resultCode, Intent data) {
        if (mWebContents == null || !isSelectionEditable() || resultCode != Activity.RESULT_OK
                || data == null) {
            return;
        }

        CharSequence result = data.getCharSequenceExtra(Intent.EXTRA_PROCESS_TEXT);
        if (result != null) {
            // TODO(hush): Use a variant of replace that re-selects the replaced text.
            // crbug.com/546710
            mWebContents.replace(result.toString());
        }
    }

    /**
     * Returns true if accessibility is on and touch exploration is enabled.
     */
    public boolean isTouchExplorationEnabled() {
        return mTouchExplorationEnabled;
    }

    /**
     * Turns browser accessibility on or off.
     * If |state| is |false|, this turns off both native and injected accessibility.
     * Otherwise, if accessibility script injection is enabled, this will enable the injected
     * accessibility scripts. Native accessibility is enabled on demand.
     */
    public void setAccessibilityState(boolean state) {
        if (!state) {
            mNativeAccessibilityAllowed = false;
            mTouchExplorationEnabled = false;
        } else {
            mNativeAccessibilityAllowed = true;
            mTouchExplorationEnabled = mAccessibilityManager.isTouchExplorationEnabled();
        }
    }

    /**
     * Return whether or not we should set accessibility focus on page load.
     */
    public boolean shouldSetAccessibilityFocusOnPageLoad() {
        return mShouldSetAccessibilityFocusOnPageLoad;
    }

    /**
     * Sets whether or not we should set accessibility focus on page load.
     * This only applies if an accessibility service like TalkBack is running.
     * This is desirable behavior for a browser window, but not for an embedded
     * WebView.
     */
    public void setShouldSetAccessibilityFocusOnPageLoad(boolean on) {
        mShouldSetAccessibilityFocusOnPageLoad = on;
    }

    /**
     *
     * @return The cached copy of render positions and scales.
     */
    public RenderCoordinates getRenderCoordinates() {
        return mRenderCoordinates;
    }

    /**
     * @return Whether the current page seems to be mobile-optimized. This hint is based upon
     *         rendered frames and may return different values when called multiple times for the
     *         same page (particularly during page load).
     */
    public boolean getIsMobileOptimizedHint() {
        return mIsMobileOptimizedHint;
    }

    @CalledByNative
    private static Rect createRect(int x, int y, int right, int bottom) {
        return new Rect(x, y, right, bottom);
    }

    public void extractSmartClipData(int x, int y, int width, int height) {
        if (mNativeContentViewCore != 0) {
            x += mSmartClipOffsetX;
            y += mSmartClipOffsetY;
            nativeExtractSmartClipData(mNativeContentViewCore, x, y, width, height);
        }
    }

    /**
     * Set offsets for smart clip.
     *
     * <p>This should be called if there is a viewport change introduced by,
     * e.g., show and hide of a location bar.
     *
     * @param offsetX Offset for X position.
     * @param offsetY Offset for Y position.
     */
    public void setSmartClipOffsets(int offsetX, int offsetY) {
        mSmartClipOffsetX = offsetX;
        mSmartClipOffsetY = offsetY;
    }

    @CalledByNative
    private void onSmartClipDataExtracted(String text, String html, Rect clipRect) {
        // Translate the positions by the offsets introduced by location bar. Note that the
        // coordinates are in dp scale, and that this definitely has the potential to be
        // different from the offsets when extractSmartClipData() was called. However,
        // as long as OEM has a UI that consumes all the inputs and waits until the
        // callback is called, then there shouldn't be any difference.
        // TODO(changwan): once crbug.com/416432 is resolved, try to pass offsets as
        // separate params for extractSmartClipData(), and apply them not the new offset
        // values in the callback.
        final float deviceScale = mRenderCoordinates.getDeviceScaleFactor();
        final int offsetXInDp = (int) (mSmartClipOffsetX / deviceScale);
        final int offsetYInDp = (int) (mSmartClipOffsetY / deviceScale);
        clipRect.offset(-offsetXInDp, -offsetYInDp);

        if (mSmartClipDataListener != null) {
            mSmartClipDataListener.onSmartClipDataExtracted(text, html, clipRect);
        }
    }

    public void setSmartClipDataListener(SmartClipDataListener listener) {
        mSmartClipDataListener = listener;
    }

    public void setBackgroundOpaque(boolean opaque) {
        if (mNativeContentViewCore != 0) {
            nativeSetBackgroundOpaque(mNativeContentViewCore, opaque);
        }
    }

    /**
     * Offer a long press gesture to the embedding View, primarily for WebView compatibility.
     *
     * @return true if the embedder handled the event.
     */
    private boolean offerLongPressToEmbedder() {
        return mContainerView.performLongClick();
    }

    /**
     * Reset scroll and fling accounting, notifying listeners as appropriate.
     * This is useful as a failsafe when the input stream may have been interruped.
     */
    private void resetScrollInProgress() {
        if (!isScrollInProgress()) return;

        final boolean touchScrollInProgress = mTouchScrollInProgress;
        final int potentiallyActiveFlingCount = mPotentiallyActiveFlingCount;

        setTouchScrollInProgress(false);
        mPotentiallyActiveFlingCount = 0;
        if (touchScrollInProgress) updateGestureStateListener(GestureEventType.SCROLL_END);
        if (potentiallyActiveFlingCount > 0) updateGestureStateListener(GestureEventType.FLING_END);
    }

    private float getWheelScrollFactorInPixels() {
        if (mWheelScrollFactorInPixels == 0) {
            TypedValue outValue = new TypedValue();
            // This is the same attribute used by Android Views to scale wheel
            // event motion into scroll deltas.
            if (mContext.getTheme().resolveAttribute(
                        android.R.attr.listPreferredItemHeight, outValue, true)) {
                mWheelScrollFactorInPixels =
                        outValue.getDimension(mContext.getResources().getDisplayMetrics());
            } else {
                // If attribute retrieval fails, just use a sensible default.
                mWheelScrollFactorInPixels = 64 * mRenderCoordinates.getDeviceScaleFactor();
            }
        }
        return mWheelScrollFactorInPixels;
    }

    ContentVideoViewEmbedder getContentVideoViewEmbedder() {
        return getContentViewClient().getContentVideoViewEmbedder();
    }

    @CalledByNative
    private boolean shouldBlockMediaRequest(String url) {
        return getContentViewClient().shouldBlockMediaRequest(url);
    }

    @CalledByNative
    private void onNativeFlingStopped() {
        // Note that mTouchScrollInProgress should normally be false at this
        // point, but we reset it anyway as another failsafe.
        setTouchScrollInProgress(false);
        if (mPotentiallyActiveFlingCount <= 0) return;
        mPotentiallyActiveFlingCount--;
        updateGestureStateListener(GestureEventType.FLING_END);
    }

    @Override
    public void onScreenOrientationChanged(int orientation) {
        // ActionMode#invalidate() won't be able to re-layout the floating
        // action mode menu items according to the new orientation. So Chrome
        // has to re-create the action mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && mActionMode != null) {
            hidePopupsAndPreserveSelection();
            showSelectActionMode(true);
        }

        sendOrientationChangeEvent(orientation);
    }

    /**
     * Set whether the ContentViewCore requires the WebContents to be fullscreen in order to lock
     * the screen orientation.
     */
    public void setFullscreenRequiredForOrientationLock(boolean value) {
        mFullscreenRequiredForOrientationLock = value;
    }

    @CalledByNative
    private boolean isFullscreenRequiredForOrientationLock() {
        return mFullscreenRequiredForOrientationLock;
    }

    /**
     * Sets the client to use for Contextual Search functionality.
     * @param contextualSearchClient The client to notify for Contextual Search operations.
     */
    public void setContextualSearchClient(ContextualSearchClient contextualSearchClient) {
        mContextualSearchClient = contextualSearchClient;
    }

    private native long nativeInit(WebContents webContents, ViewAndroidDelegate viewAndroidDelegate,
            long windowAndroidPtr, HashSet<Object> retainedObjectSet);
    private static native ContentViewCore nativeFromWebContentsAndroid(WebContents webContents);

    private native WebContents nativeGetWebContentsAndroid(long nativeContentViewCoreImpl);
    private native WindowAndroid nativeGetJavaWindowAndroid(long nativeContentViewCoreImpl);

    private native void nativeOnJavaContentViewCoreDestroyed(long nativeContentViewCoreImpl);

    private native void nativeSetFocus(long nativeContentViewCoreImpl, boolean focused);

    private native void nativeSendOrientationChangeEvent(
            long nativeContentViewCoreImpl, int orientation);

    // All touch events (including flings, scrolls etc) accept coordinates in physical pixels.
    private native boolean nativeOnTouchEvent(
            long nativeContentViewCoreImpl, MotionEvent event,
            long timeMs, int action, int pointerCount, int historySize, int actionIndex,
            float x0, float y0, float x1, float y1,
            int pointerId0, int pointerId1,
            float touchMajor0, float touchMajor1,
            float touchMinor0, float touchMinor1,
            float orientation0, float orientation1,
            float tilt0, float tilt1,
            float rawX, float rawY,
            int androidToolType0, int androidToolType1,
            int androidButtonState, int androidMetaState,
            boolean isTouchHandleEvent);

    private native int nativeSendMouseMoveEvent(
            long nativeContentViewCoreImpl, long timeMs, float x, float y);

    private native int nativeSendMouseWheelEvent(long nativeContentViewCoreImpl, long timeMs,
            float x, float y, float ticksX, float ticksY, float pixelsPerTick);

    private native void nativeScrollBegin(long nativeContentViewCoreImpl, long timeMs, float x,
            float y, float hintX, float hintY, boolean targetViewport);

    private native void nativeScrollEnd(long nativeContentViewCoreImpl, long timeMs);

    private native void nativeScrollBy(
            long nativeContentViewCoreImpl, long timeMs, float x, float y,
            float deltaX, float deltaY);

    private native void nativeFlingStart(long nativeContentViewCoreImpl, long timeMs, float x,
            float y, float vx, float vy, boolean targetViewport);

    private native void nativeFlingCancel(long nativeContentViewCoreImpl, long timeMs);

    private native void nativeSingleTap(
            long nativeContentViewCoreImpl, long timeMs, float x, float y);

    private native void nativeDoubleTap(
            long nativeContentViewCoreImpl, long timeMs, float x, float y);

    private native void nativeLongPress(
            long nativeContentViewCoreImpl, long timeMs, float x, float y);

    private native void nativePinchBegin(
            long nativeContentViewCoreImpl, long timeMs, float x, float y);

    private native void nativePinchEnd(long nativeContentViewCoreImpl, long timeMs);

    private native void nativePinchBy(long nativeContentViewCoreImpl, long timeMs,
            float anchorX, float anchorY, float deltaScale);

    private native void nativeSelectBetweenCoordinates(
            long nativeContentViewCoreImpl, float x1, float y1, float x2, float y2);

    private native void nativeDismissTextHandles(long nativeContentViewCoreImpl);
    private native void nativeSetTextHandlesTemporarilyHidden(
            long nativeContentViewCoreImpl, boolean hidden);

    private native void nativeResetGestureDetection(long nativeContentViewCoreImpl);

    private native void nativeSetDoubleTapSupportEnabled(
            long nativeContentViewCoreImpl, boolean enabled);

    private native void nativeSetMultiTouchZoomSupportEnabled(
            long nativeContentViewCoreImpl, boolean enabled);

    private native void nativeSelectPopupMenuItems(long nativeContentViewCoreImpl,
            long nativeSelectPopupSourceFrame, int[] indices);


    private native long nativeGetNativeImeAdapter(long nativeContentViewCoreImpl);

    private native int nativeGetCurrentRenderProcessId(long nativeContentViewCoreImpl);

    private native void nativeSetAllowJavascriptInterfacesInspection(
            long nativeContentViewCoreImpl, boolean allow);

    private native void nativeAddJavascriptInterface(long nativeContentViewCoreImpl, Object object,
            String name, Class requiredAnnotation);

    private native void nativeRemoveJavascriptInterface(long nativeContentViewCoreImpl,
            String name);

    private native void nativeWasResized(long nativeContentViewCoreImpl);

    private native void nativeSetAccessibilityEnabled(
            long nativeContentViewCoreImpl, boolean enabled);

    private native void nativeSetTextTrackSettings(long nativeContentViewCoreImpl,
            boolean textTracksEnabled, String textTrackBackgroundColor, String textTrackFontFamily,
            String textTrackFontStyle, String textTrackFontVariant, String textTrackTextColor,
            String textTrackTextShadow, String textTrackTextSize);

    private native void nativeExtractSmartClipData(long nativeContentViewCoreImpl,
            int x, int y, int w, int h);

    private native void nativeSetBackgroundOpaque(long nativeContentViewCoreImpl, boolean opaque);
}
