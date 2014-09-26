// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.SearchManager;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.database.ContentObserver;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import android.os.SystemClock;
import android.provider.Browser;
import android.provider.Settings;
import android.text.Editable;
import android.text.Selection;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;
import android.view.ActionMode;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CalledByNative;
import org.chromium.base.CommandLine;
import org.chromium.base.JNINamespace;
import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.content.R;
import org.chromium.content.browser.ScreenOrientationListener.ScreenOrientationObserver;
import org.chromium.content.browser.accessibility.AccessibilityInjector;
import org.chromium.content.browser.accessibility.BrowserAccessibilityManager;
import org.chromium.content.browser.input.AdapterInputConnection;
import org.chromium.content.browser.input.GamepadList;
import org.chromium.content.browser.input.ImeAdapter;
import org.chromium.content.browser.input.ImeAdapter.AdapterInputConnectionFactory;
import org.chromium.content.browser.input.InputMethodManagerWrapper;
import org.chromium.content.browser.input.PastePopupMenu;
import org.chromium.content.browser.input.PastePopupMenu.PastePopupMenuDelegate;
import org.chromium.content.browser.input.PopupTouchHandleDrawable;
import org.chromium.content.browser.input.PopupTouchHandleDrawable.PopupTouchHandleDrawableDelegate;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.input.SelectPopupDialog;
import org.chromium.content.browser.input.SelectPopupDropdown;
import org.chromium.content.browser.input.SelectPopupItem;
import org.chromium.content.browser.input.SelectionEventType;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.gfx.DeviceDisplayInfo;

import java.lang.annotation.Annotation;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * Provides a Java-side 'wrapper' around a WebContent (native) instance.
 * Contains all the major functionality necessary to manage the lifecycle of a ContentView without
 * being tied to the view system.
 */
@JNINamespace("content")
public class ContentViewCore
        implements AccessibilityStateChangeListener, ScreenOrientationObserver {

    private static final String TAG = "ContentViewCore";

    // Used to avoid enabling zooming in / out if resulting zooming will
    // produce little visible difference.
    private static final float ZOOM_CONTROLS_EPSILON = 0.007f;

    // Used to represent gestures for long press and long tap.
    private static final int IS_LONG_PRESS = 1;
    private static final int IS_LONG_TAP = 2;

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
         * @see View#drawChild(Canvas, View, long)
         */
        boolean drawChild(Canvas canvas, View child, long drawingTime);

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
    private WebContentsObserverAndroid mWebContentsObserver;

    private ContentViewClient mContentViewClient;

    private ContentSettings mContentSettings;

    // Native pointer to C++ ContentViewCoreImpl object which will be set by nativeInit().
    private long mNativeContentViewCore = 0;

    private final ObserverList<GestureStateListener> mGestureStateListeners;
    private final RewindableIterator<GestureStateListener> mGestureStateListenersIterator;
    private ZoomControlsDelegate mZoomControlsDelegate;

    private PopupZoomer mPopupZoomer;
    private SelectPopup mSelectPopup;
    private long mNativeSelectPopupSourceFrame = 0;

    private Runnable mFakeMouseMoveRunnable = null;

    // Only valid when focused on a text / password field.
    private ImeAdapter mImeAdapter;
    private ImeAdapter.AdapterInputConnectionFactory mAdapterInputConnectionFactory;
    private AdapterInputConnection mInputConnection;
    private InputMethodManagerWrapper mInputMethodManagerWrapper;

    // Lazily created paste popup menu, triggered either via long press in an
    // editable region or from tapping the insertion handle.
    private PastePopupMenu mPastePopupMenu;
    private boolean mWasPastePopupShowingOnInsertionDragStart;

    private PopupTouchHandleDrawableDelegate mTouchHandleDelegate;

    private PositionObserver mPositionObserver;

    // Size of the viewport in physical pixels as set from onSizeChanged.
    private int mViewportWidthPix;
    private int mViewportHeightPix;
    private int mPhysicalBackingWidthPix;
    private int mPhysicalBackingHeightPix;
    private int mTopControlsLayoutHeightPix;

    // Cached copy of all positions and scales as reported by the renderer.
    private final RenderCoordinates mRenderCoordinates;

    // Tracks whether a selection is currently active.  When applied to selected text, indicates
    // whether the last selected text is still highlighted.
    private boolean mHasSelection;
    private boolean mHasInsertion;
    private String mLastSelectedText;
    private boolean mFocusedNodeEditable;
    private ActionMode mActionMode;
    private boolean mUnselectAllOnActionModeDismiss;
    private boolean mPreserveSelectionOnNextLossOfFocus;

    // Delegate that will handle GET downloads, and be notified of completion of POST downloads.
    private ContentViewDownloadDelegate mDownloadDelegate;

    // The AccessibilityInjector that handles loading Accessibility scripts into the web page.
    private AccessibilityInjector mAccessibilityInjector;

    // Whether native accessibility, i.e. without any script injection, is allowed.
    private boolean mNativeAccessibilityAllowed;

    // Whether native accessibility, i.e. without any script injection, has been enabled.
    private boolean mNativeAccessibilityEnabled;

    // Handles native accessibility, i.e. without any script injection.
    private BrowserAccessibilityManager mBrowserAccessibilityManager;

    // System accessibility service.
    private final AccessibilityManager mAccessibilityManager;

    // Accessibility touch exploration state.
    private boolean mTouchExplorationEnabled;

    // Whether accessibility focus should be set to the page when it finishes loading.
    // This only applies if an accessibility service like TalkBack is running.
    // This is desirable behavior for a browser window, but not for an embedded
    // WebView.
    private boolean mShouldSetAccessibilityFocusOnPageLoad;

    // Allows us to dynamically respond when the accessibility script injection flag changes.
    private ContentObserver mAccessibilityScriptInjectionObserver;

    // Temporary notification to tell onSizeChanged to focus a form element,
    // because the OSK was just brought up.
    private final Rect mFocusPreOSKViewportRect = new Rect();

    // On tap this will store the x, y coordinates of the touch.
    private int mLastTapX;
    private int mLastTapY;

    // Whether a touch scroll sequence is active, used to hide text selection
    // handles. Note that a scroll sequence will *always* bound a pinch
    // sequence, so this will also be true for the duration of a pinch gesture.
    private boolean mTouchScrollInProgress;

    // The outstanding fling start events that hasn't got fling end yet. It may be > 1 because
    // onNativeFlingStopped() is called asynchronously.
    private int mPotentiallyActiveFlingCount;

    private ViewAndroid mViewAndroid;

    private SmartClipDataListener mSmartClipDataListener = null;

    // This holds the state of editable text (e.g. contents of <input>, contenteditable) of
    // a focused element.
    // Every time the user, IME, javascript (Blink), autofill etc. modifies the content, the new
    //  state must be reflected to this to keep consistency.
    private final Editable mEditable;

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

    /**
     * Constructs a new ContentViewCore. Embedders must call initialize() after constructing
     * a ContentViewCore and before using it.
     *
     * @param context The context used to create this.
     */
    public ContentViewCore(Context context) {
        mContext = context;

        mAdapterInputConnectionFactory = new AdapterInputConnectionFactory();
        mInputMethodManagerWrapper = new InputMethodManagerWrapper(mContext);

        mRenderCoordinates = new RenderCoordinates();
        float deviceScaleFactor = getContext().getResources().getDisplayMetrics().density;
        String forceScaleFactor = CommandLine.getInstance().getSwitchValue(
                ContentSwitches.FORCE_DEVICE_SCALE_FACTOR);
        if (forceScaleFactor != null) {
            deviceScaleFactor = Float.valueOf(forceScaleFactor);
        }
        mRenderCoordinates.setDeviceScaleFactor(deviceScaleFactor);
        mAccessibilityManager = (AccessibilityManager)
                getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        mGestureStateListeners = new ObserverList<GestureStateListener>();
        mGestureStateListenersIterator = mGestureStateListeners.rewindableIterator();

        mEditable = Editable.Factory.getInstance().newEditable("");
        Selection.setSelection(mEditable, 0);
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

    /* TODO(aelias): Remove this after downstream callers switch to setTopControlsLayoutHeight. */
    public void setViewportSizeOffset(int offsetXPix, int offsetYPix) {
        setTopControlsLayoutHeight(offsetYPix);
    }

    /**
     * Specifies how much smaller the Blink layout size should be relative to the size of this
     * view.
     * @param topControlsLayoutHeightPix The Y amount in pixels to shrink the viewport by.
     */
    public void setTopControlsLayoutHeight(int topControlsLayoutHeightPix) {
        if (topControlsLayoutHeightPix != mTopControlsLayoutHeightPix) {
            mTopControlsLayoutHeightPix = topControlsLayoutHeightPix;
            if (mNativeContentViewCore != 0) nativeWasResized(mNativeContentViewCore);
        }
    }

    /**
     * Returns a delegate that can be used to add and remove views from the ContainerView.
     *
     * NOTE: Use with care, as not all ContentViewCore users setup their ContainerView in the same
     * way. In particular, the Android WebView has limitations on what implementation details can
     * be provided via a child view, as they are visible in the API and could introduce
     * compatibility breaks with existing applications. If in doubt, contact the
     * android_webview/OWNERS
     *
     * @return A ViewAndroidDelegate that can be used to add and remove views.
     */
    @VisibleForTesting
    public ViewAndroidDelegate getViewAndroidDelegate() {
        return new ViewAndroidDelegate() {
            // mContainerView can change, but this ViewAndroidDelegate can only be used to
            // add and remove views from the mContainerViewAtCreation.
            private final ViewGroup mContainerViewAtCreation = mContainerView;

            @Override
            public View acquireAnchorView() {
                View anchorView = new View(mContext);
                mContainerViewAtCreation.addView(anchorView);
                return anchorView;
            }

            @Override
            @SuppressWarnings("deprecation")  // AbsoluteLayout
            public void setAnchorViewPosition(
                    View view, float x, float y, float width, float height) {
                if (view.getParent() == null) {
                    // Ignore. setAnchorViewPosition has been called after the anchor view has
                    // already been released.
                    return;
                }
                assert view.getParent() == mContainerViewAtCreation;

                float scale = (float) DeviceDisplayInfo.create(mContext).getDIPScale();

                // The anchor view should not go outside the bounds of the ContainerView.
                int leftMargin = Math.round(x * scale);
                int topMargin = Math.round(mRenderCoordinates.getContentOffsetYPix() + y * scale);
                int scaledWidth = Math.round(width * scale);
                // ContentViewCore currently only supports these two container view types.
                if (mContainerViewAtCreation instanceof FrameLayout) {
                    int startMargin;
                    if (ApiCompatibilityUtils.isLayoutRtl(mContainerViewAtCreation)) {
                        startMargin = mContainerViewAtCreation.getMeasuredWidth()
                                - Math.round((width + x) * scale);
                    } else {
                        startMargin = leftMargin;
                    }
                    if (scaledWidth + startMargin > mContainerViewAtCreation.getWidth()) {
                        scaledWidth = mContainerViewAtCreation.getWidth() - startMargin;
                    }
                    FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                        scaledWidth, Math.round(height * scale));
                    ApiCompatibilityUtils.setMarginStart(lp, startMargin);
                    lp.topMargin = topMargin;
                    view.setLayoutParams(lp);
                } else if (mContainerViewAtCreation instanceof android.widget.AbsoluteLayout) {
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
                    Log.e(TAG, "Unknown layout " + mContainerViewAtCreation.getClass().getName());
                }
            }

            @Override
            public void releaseAnchorView(View anchorView) {
                mContainerViewAtCreation.removeView(anchorView);
            }
        };
    }

    @VisibleForTesting
    public void setImeAdapterForTest(ImeAdapter imeAdapter) {
        mImeAdapter = imeAdapter;
    }

    @VisibleForTesting
    public ImeAdapter getImeAdapterForTest() {
        return mImeAdapter;
    }

    @VisibleForTesting
    public void setAdapterInputConnectionFactory(AdapterInputConnectionFactory factory) {
        mAdapterInputConnectionFactory = factory;
    }

    @VisibleForTesting
    public void setInputMethodManagerWrapperForTest(InputMethodManagerWrapper immw) {
        mInputMethodManagerWrapper = immw;
    }

    @VisibleForTesting
    public AdapterInputConnection getInputConnectionForTest() {
        return mInputConnection;
    }

    private ImeAdapter createImeAdapter(Context context) {
        return new ImeAdapter(mInputMethodManagerWrapper,
                new ImeAdapter.ImeAdapterDelegate() {
                    @Override
                    public void onImeEvent() {
                        mPopupZoomer.hide(true);
                        getContentViewClient().onImeEvent();
                        if (mFocusedNodeEditable) hideTextHandles();
                    }

                    @Override
                    public void onDismissInput() {
                        getContentViewClient().onImeStateChangeRequested(false);
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
                                getContentViewClient().onImeStateChangeRequested(
                                        resultCode == InputMethodManager.RESULT_SHOWN ||
                                        resultCode == InputMethodManager.RESULT_UNCHANGED_SHOWN);
                                if (resultCode == InputMethodManager.RESULT_SHOWN) {
                                    // If OSK is newly shown, delay the form focus until
                                    // the onSizeChanged (in order to adjust relative to the
                                    // new size).
                                    // TODO(jdduke): We should not assume that onSizeChanged will
                                    // always be called, crbug.com/294908.
                                    getContainerView().getWindowVisibleDisplayFrame(
                                            mFocusPreOSKViewportRect);
                                } else if (hasFocus() && resultCode ==
                                        InputMethodManager.RESULT_UNCHANGED_SHOWN) {
                                    // If the OSK was already there, focus the form immediately.
                                    scrollFocusedEditableNodeIntoView();
                                }
                            }
                        };
                    }
                }
        );
    }

    /**
     *
     * @param containerView The view that will act as a container for all views created by this.
     * @param internalDispatcher Handles dispatching all hidden or super methods to the
     *                           containerView.
     * @param nativeWebContents A pointer to the native web contents.
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
            long nativeWebContents, WindowAndroid windowAndroid) {
        setContainerView(containerView);

        long windowNativePointer = windowAndroid.getNativePointer();
        assert windowNativePointer != 0;
        mViewAndroid = new ViewAndroid(windowAndroid, getViewAndroidDelegate());
        long viewAndroidNativePointer = mViewAndroid.getNativePointer();
        assert viewAndroidNativePointer != 0;

        mZoomControlsDelegate = NO_OP_ZOOM_CONTROLS_DELEGATE;

        mNativeContentViewCore = nativeInit(
                nativeWebContents, viewAndroidNativePointer, windowNativePointer,
                mRetainedJavaScriptObjects);
        mWebContents = nativeGetWebContentsAndroid(mNativeContentViewCore);
        mContentSettings = new ContentSettings(this, mNativeContentViewCore);

        setContainerViewInternals(internalDispatcher);
        mRenderCoordinates.reset();
        initPopupZoomer(mContext);
        mImeAdapter = createImeAdapter(mContext);

        mAccessibilityInjector = AccessibilityInjector.newInstance(this);

        mWebContentsObserver = new WebContentsObserverAndroid(mWebContents) {
            @Override
            public void didNavigateMainFrame(String url, String baseUrl,
                    boolean isNavigationToDifferentPage, boolean isFragmentNavigation) {
                if (!isNavigationToDifferentPage) return;
                hidePopupsAndClearSelection();
                resetScrollInProgress();
                resetGestureDetection();
            }

            @Override
            public void renderProcessGone(boolean wasOomProtected) {
                hidePopupsAndClearSelection();
                resetScrollInProgress();
                // No need to reset gesture detection as the detector will have
                // been destroyed in the RenderWidgetHostView.
            }
        };
    }

    /**
     * Sets a new container view for this {@link ContentViewCore}.
     *
     * <p>WARNING: This is not a general purpose method and has been designed with WebView
     * fullscreen in mind. Please be aware that it might not be appropriate for other use cases
     * and that it has a number of limitations. For example the PopupZoomer only works with the
     * container view with which this ContentViewCore has been initialized.
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
        TraceEvent.begin();
        if (mContainerView != null) {
            mPastePopupMenu = null;
            mInputConnection = null;
            hidePopupsAndClearSelection();
        }

        mContainerView = containerView;
        mPositionObserver = new ViewPositionObserver(mContainerView);
        String contentDescription = "Web View";
        if (R.string.accessibility_content_view == 0) {
            Log.w(TAG, "Setting contentDescription to 'Web View' as no value was specified.");
        } else {
            contentDescription = mContext.getResources().getString(
                    R.string.accessibility_content_view);
        }
        mContainerView.setContentDescription(contentDescription);
        mContainerView.setWillNotDraw(false);
        mContainerView.setClickable(true);
        TraceEvent.end();
    }

    @CalledByNative
    void onNativeContentViewCoreDestroyed(long nativeContentViewCore) {
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

    private void initPopupZoomer(Context context) {
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
                        } else {
                            assert false : "PopupZoomer should never be shown without being hidden";
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
                        } else {
                            assert false : "PopupZoomer should never be hidden without being shown";
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
     */
    public void destroy() {
        if (mNativeContentViewCore != 0) {
            nativeOnJavaContentViewCoreDestroyed(mNativeContentViewCore);
        }
        mWebContentsObserver.detachFromWebContents();
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
        if (mViewAndroid != null) mViewAndroid.destroy();
        mNativeContentViewCore = 0;
        mContentSettings = null;
        mJavaScriptInterfaces.clear();
        mRetainedJavaScriptObjects.clear();
        unregisterAccessibilityContentObserver();
        mGestureStateListeners.clear();
        ScreenOrientationListener.getInstance().removeObserver(this);
        mPositionObserver.clearListener();
    }

    private void unregisterAccessibilityContentObserver() {
        if (mAccessibilityScriptInjectionObserver == null) {
            return;
        }
        getContext().getContentResolver().unregisterContentObserver(
                mAccessibilityScriptInjectionObserver);
        mAccessibilityScriptInjectionObserver = null;
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
    public long getNativeContentViewCore() {
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
     * Shows an interstitial page driven by the passed in delegate.
     *
     * @param url The URL being blocked by the interstitial.
     * @param delegate The delegate handling the interstitial.
     */
    @VisibleForTesting
    public void showInterstitialPage(
            String url, InterstitialPageDelegateAndroid delegate) {
        assert mWebContents != null;
        mWebContents.showInterstitialPage(url, delegate.getNative());
    }

    /**
     * @return Whether the page is currently showing an interstitial, such as a bad HTTPS page.
     */
    public boolean isShowingInterstitialPage() {
        assert mWebContents != null;
        return mWebContents.isShowingInterstitialPage();
    }

    /**
     * @return Viewport width in physical pixels as set from onSizeChanged.
     */
    @CalledByNative
    public int getViewportWidthPix() { return mViewportWidthPix; }

    /**
     * @return Viewport height in physical pixels as set from onSizeChanged.
     */
    @CalledByNative
    public int getViewportHeightPix() { return mViewportHeightPix; }

    /**
     * @return Width of underlying physical surface.
     */
    @CalledByNative
    public int getPhysicalBackingWidthPix() { return mPhysicalBackingWidthPix; }

    /**
     * @return Height of underlying physical surface.
     */
    @CalledByNative
    public int getPhysicalBackingHeightPix() { return mPhysicalBackingHeightPix; }

    /* TODO(aelias): Remove these when downstream callers disappear. */
    @VisibleForTesting
    public int getViewportSizeOffsetWidthPix() { return 0; }
    @VisibleForTesting
    public int getViewportSizeOffsetHeightPix() { return getTopControlsLayoutHeightPix(); }

    /**
     * @return The amount that the viewport size given to Blink is shrunk by the URL-bar..
     */
    @CalledByNative
    public int getTopControlsLayoutHeightPix() { return mTopControlsLayoutHeightPix; }

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

    // End FrameLayout overrides.

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    public boolean onTouchEvent(MotionEvent event) {
        final boolean isTouchHandleEvent = false;
        return onTouchEventImpl(event, isTouchHandleEvent);
    }

    private boolean onTouchEventImpl(MotionEvent event, boolean isTouchHandleEvent) {
        TraceEvent.begin("onTouchEvent");
        try {
            int eventAction = event.getActionMasked();

            if (eventAction == MotionEvent.ACTION_DOWN) {
                cancelRequestToScrollFocusedEditableNodeIntoView();
            }

            if (SPenSupport.isSPenSupported(mContext))
                eventAction = SPenSupport.convertSPenEventAction(eventAction);
            if (!isValidTouchEventActionForNative(eventAction)) return false;

            if (mNativeContentViewCore == 0) return false;

            // A zero offset is quite common, in which case the unnecessary copy should be avoided.
            MotionEvent offset = null;
            if (mCurrentTouchOffsetX != 0 || mCurrentTouchOffsetY != 0) {
                offset = createOffsetMotionEvent(event);
                event = offset;
            }

            final int pointerCount = event.getPointerCount();
            final boolean consumed = nativeOnTouchEvent(mNativeContentViewCore, event,
                    event.getEventTime(), eventAction,
                    pointerCount, event.getHistorySize(), event.getActionIndex(),
                    event.getX(), event.getY(),
                    pointerCount > 1 ? event.getX(1) : 0,
                    pointerCount > 1 ? event.getY(1) : 0,
                    event.getPointerId(0), pointerCount > 1 ? event.getPointerId(1) : -1,
                    event.getTouchMajor(), pointerCount > 1 ? event.getTouchMajor(1) : 0,
                    event.getTouchMinor(), pointerCount > 1 ? event.getTouchMinor(1) : 0,
                    event.getOrientation(), pointerCount > 1 ? event.getOrientation(1) : 0,
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

    public void setIgnoreRemainingTouchEvents() {
        resetGestureDetection();
    }

    public boolean isScrollInProgress() {
        return mTouchScrollInProgress || mPotentiallyActiveFlingCount > 0;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onFlingStartEventConsumed(int vx, int vy) {
        mTouchScrollInProgress = false;
        mPotentiallyActiveFlingCount++;
        for (mGestureStateListenersIterator.rewind();
                    mGestureStateListenersIterator.hasNext();) {
            mGestureStateListenersIterator.next().onFlingStartGesture(
                    vx, vy, computeVerticalScrollOffset(), computeVerticalScrollExtent());
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onFlingStartEventHadNoConsumer(int vx, int vy) {
        mTouchScrollInProgress = false;
        for (mGestureStateListenersIterator.rewind();
                    mGestureStateListenersIterator.hasNext();) {
            mGestureStateListenersIterator.next().onUnhandledFlingStartEvent(vx, vy);
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
        mTouchScrollInProgress = true;
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
        if (!mTouchScrollInProgress) return;
        mTouchScrollInProgress = false;
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

    @VisibleForTesting
    public void flingForTest(long timeMs, int x, int y, int velocityX, int velocityY) {
        if (mNativeContentViewCore == 0) return;
        nativeFlingCancel(mNativeContentViewCore, timeMs);
        nativeScrollBegin(mNativeContentViewCore, timeMs, x, y, velocityX, velocityY);
        nativeFlingStart(mNativeContentViewCore, timeMs, x, y, velocityX, velocityY);
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
                case GestureEventType.FLING_CANCEL:
                    listener.onFlingCancelGesture();
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
     * Inserts the provided markup sandboxed into the frame.
     */
    public void setupTransitionView(String markup) {
        assert mWebContents != null;
        mWebContents.setupTransitionView(markup);
    }

    /**
     * Hides transition elements specified by the selector, and activates any
     * exiting-transition stylesheets.
     */
    public void beginExitTransition(String cssSelector) {
        assert mWebContents != null;
        mWebContents.beginExitTransition(cssSelector);
    }

    /**
     * Requests the renderer insert a link to the specified stylesheet in the
     * main frame's document.
     */
    public void addStyleSheetByURL(String url) {
        assert mWebContents != null;
        mWebContents.addStyleSheetByURL(url);
    }

    /**
     * Injects the passed Javascript code in the current page and evaluates it.
     * If a result is required, pass in a callback.
     * Used in automation tests.
     *
     * @param script The Javascript to execute.
     * @param callback The callback to be fired off when a result is ready. The script's
     *                 result will be json encoded and passed as the parameter, and the call
     *                 will be made on the main thread.
     *                 If no result is required, pass null.
     */
    public void evaluateJavaScript(String script, JavaScriptCallback callback) {
        assert mWebContents != null;
        mWebContents.evaluateJavaScript(script, callback);
    }

    /**
     * Post a message to a frame.
     * TODO(sgurun) also add support for transferring a message channel port.
     *
     * @param frameName The name of the frame. If the name is null the message is posted
     *                  to the main frame.
     * @param message   The message
     * @param sourceOrigin  The source origin
     * @param targetOrigin  The target origin
     */
    public void postMessageToFrame(String frameName, String message,
            String sourceOrigin, String targetOrigin) {
        if (mNativeContentViewCore == 0) return;
        nativePostMessageToFrame(mNativeContentViewCore, frameName, message, sourceOrigin,
            targetOrigin);
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
    @VisibleForTesting
    public int getCurrentRenderProcessId() {
        return nativeGetCurrentRenderProcessId(mNativeContentViewCore);
    }

    /**
     * To be called when the ContentView is hidden.
     */
    public void onHide() {
        assert mWebContents != null;
        hidePopupsAndPreserveSelection();
        setInjectedAccessibility(false);
        mWebContents.onHide();
    }

    /**
     * Return the ContentSettings object used to retrieve the settings for this
     * ContentViewCore. For modifications, ChromeNativePreferences is to be used.
     * @return A ContentSettings object that can be used to retrieve this
     *         ContentViewCore's settings.
     */
    public ContentSettings getContentSettings() {
        return mContentSettings;
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
        hideSelectActionBar();
        hidePastePopup();
        hideSelectPopup();
        mPopupZoomer.hide(false);
        if (mUnselectAllOnActionModeDismiss) hideTextHandles();
    }

    private void restoreSelectionPopupsIfNecessary() {
        if (mHasSelection && mActionMode == null) showSelectActionBar();
    }

    public void hideSelectActionBar() {
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
        restoreSelectionPopupsIfNecessary();
        ScreenOrientationListener.getInstance().addObserver(this, mContext);
        GamepadList.onAttachedToWindow(mContext);
    }

    /**
     * @see View#onDetachedFromWindow()
     */
    @SuppressWarnings("javadoc")
    @SuppressLint("MissingSuperCall")
    public void onDetachedFromWindow() {
        setInjectedAccessibility(false);
        hidePopupsAndPreserveSelection();
        mZoomControlsDelegate.dismissZoomPicker();
        unregisterAccessibilityContentObserver();

        ScreenOrientationListener.getInstance().removeObserver(this);
        GamepadList.onDetachedFromWindow();
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
        if (!mImeAdapter.hasTextInputType()) {
            // Although onCheckIsTextEditor will return false in this case, the EditorInfo
            // is still used by the InputMethodService. Need to make sure the IME doesn't
            // enter fullscreen mode.
            outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
        }
        mInputConnection = mAdapterInputConnectionFactory.get(mContainerView, mImeAdapter,
                mEditable, outAttrs);
        return mInputConnection;
    }

    @VisibleForTesting
    public AdapterInputConnection getAdapterInputConnectionForTest() {
        return mInputConnection;
    }

    @VisibleForTesting
    public Editable getEditableForTest() {
        return mEditable;
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
        TraceEvent.begin();

        if (newConfig.keyboard != Configuration.KEYBOARD_NOKEYS) {
            if (mNativeContentViewCore != 0) {
                mImeAdapter.attach(nativeGetNativeImeAdapter(mNativeContentViewCore),
                        ImeAdapter.getTextInputTypeNone(), 0 /* no flags */);
            }
            mInputMethodManagerWrapper.restartInput(mContainerView);
        }
        mContainerViewInternals.super_onConfigurationChanged(newConfig);

        // To request layout has side effect, but it seems OK as it only happen in
        // onConfigurationChange and layout has to be changed in most case.
        mContainerView.requestLayout();
        TraceEvent.end();
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

    /* TODO(aelias): Remove this after downstream callers disappear. */
    public void onOverdrawBottomHeightChanged(int overdrawHeightPix) {
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
                    scrollFocusedEditableNodeIntoView();
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

    private void scrollFocusedEditableNodeIntoView() {
        assert mWebContents != null;
        mWebContents.scrollFocusedEditableNodeIntoView();
    }

    /**
     * Selects the word around the caret, if any.
     * The caller can check if selection actually occurred by listening to OnSelectionChanged.
     */
    public void selectWordAroundCaret() {
        assert mWebContents != null;
        mWebContents.selectWordAroundCaret();
    }

    /**
     * @see View#onWindowFocusChanged(boolean)
     */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (!hasWindowFocus) resetGestureDetection();
    }

    public void onFocusChanged(boolean gainFocus) {
        if (gainFocus) {
            restoreSelectionPopupsIfNecessary();
        } else {
            hideImeIfNeeded();
            cancelRequestToScrollFocusedEditableNodeIntoView();
            if (mPreserveSelectionOnNextLossOfFocus) {
                mPreserveSelectionOnNextLossOfFocus = false;
                hidePopupsAndPreserveSelection();
            } else {
                hidePopupsAndClearSelection();
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
            TraceEvent.begin();
            return mContainerViewInternals.super_dispatchKeyEventPreIme(event);
        } finally {
            TraceEvent.end();
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
            switch (event.getAction()) {
                case MotionEvent.ACTION_SCROLL:
                    if (mNativeContentViewCore == 0) return false;

                    nativeSendMouseWheelEvent(mNativeContentViewCore, event.getEventTime(),
                            event.getX(), event.getY(),
                            event.getAxisValue(MotionEvent.AXIS_VSCROLL));

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
    public void scrollBy(int xPix, int yPix) {
        if (mNativeContentViewCore != 0) {
            nativeScrollBy(mNativeContentViewCore,
                    SystemClock.uptimeMillis(), 0, 0, xPix, yPix);
        }
    }

    /**
     * @see View#scrollTo(int, int)
     */
    public void scrollTo(int xPix, int yPix) {
        if (mNativeContentViewCore == 0) return;
        final float xCurrentPix = mRenderCoordinates.getScrollXPix();
        final float yCurrentPix = mRenderCoordinates.getScrollYPix();
        final float dxPix = xPix - xCurrentPix;
        final float dyPix = yPix - yCurrentPix;
        if (dxPix != 0 || dyPix != 0) {
            long time = SystemClock.uptimeMillis();
            nativeScrollBegin(mNativeContentViewCore, time,
                    xCurrentPix, yCurrentPix, -dxPix, -dyPix);
            nativeScrollBy(mNativeContentViewCore,
                    time, xCurrentPix, yCurrentPix, dxPix, dyPix);
            nativeScrollEnd(mNativeContentViewCore, time);
        }
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

        mLastTapX = (int) xPix;
        mLastTapY = (int) yPix;
    }

    /**
     * @return The x coordinate for the last point that a tap or press gesture was initiated from.
     */
    public int getLastTapX()  {
        return mLastTapX;
    }

    /**
     * @return The y coordinate for the last point that a tap or press gesture was initiated from.
     */
    public int getLastTapY()  {
        return mLastTapY;
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

    private void showSelectActionBar() {
        if (mActionMode != null) {
            mActionMode.invalidate();
            return;
        }

        // Start a new action mode with a SelectActionModeCallback.
        SelectActionModeCallback.ActionHandler actionHandler =
                new SelectActionModeCallback.ActionHandler() {
            @Override
            public void selectAll() {
                mImeAdapter.selectAll();
            }

            @Override
            public void cut() {
                mImeAdapter.cut();
            }

            @Override
            public void copy() {
                mImeAdapter.copy();
            }

            @Override
            public void paste() {
                mImeAdapter.paste();
            }

            @Override
            public void share() {
                final String query = getSelectedText();
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
            public void search() {
                final String query = getSelectedText();
                if (TextUtils.isEmpty(query)) return;

                // See if ContentViewClient wants to override
                if (getContentViewClient().doesPerformWebSearch()) {
                    getContentViewClient().performWebSearch(query);
                    return;
                }

                Intent i = new Intent(Intent.ACTION_WEB_SEARCH);
                i.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
                i.putExtra(SearchManager.QUERY, query);
                i.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
                if (!(getContext() instanceof Activity)) {
                    i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                }
                try {
                    getContext().startActivity(i);
                } catch (android.content.ActivityNotFoundException ex) {
                    // If no app handles it, do nothing.
                }
            }

            @Override
            public boolean isSelectionPassword() {
                return mImeAdapter.isSelectionPassword();
            }

            @Override
            public boolean isSelectionEditable() {
                return mFocusedNodeEditable;
            }

            @Override
            public void onDestroyActionMode() {
                mActionMode = null;
                if (mUnselectAllOnActionModeDismiss) {
                    hideTextHandles();
                    if (isSelectionEditable()) {
                        int selectionEnd = Selection.getSelectionEnd(mEditable);
                        mInputConnection.setSelection(selectionEnd, selectionEnd);
                    } else {
                        mImeAdapter.unselect();
                    }
                }
                getContentViewClient().onContextualActionBarHidden();
            }

            @Override
            public boolean isShareAvailable() {
                Intent intent = new Intent(Intent.ACTION_SEND);
                intent.setType("text/plain");
                return getContext().getPackageManager().queryIntentActivities(intent,
                        PackageManager.MATCH_DEFAULT_ONLY).size() > 0;
            }

            @Override
            public boolean isWebSearchAvailable() {
                if (getContentViewClient().doesPerformWebSearch()) return true;
                Intent intent = new Intent(Intent.ACTION_WEB_SEARCH);
                intent.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
                return getContext().getPackageManager().queryIntentActivities(intent,
                        PackageManager.MATCH_DEFAULT_ONLY).size() > 0;
            }
        };
        mActionMode = null;
        // On ICS, startActionMode throws an NPE when getParent() is null.
        if (mContainerView.getParent() != null) {
            assert mWebContents != null;
            mActionMode = mContainerView.startActionMode(
                    getContentViewClient().getSelectActionModeCallback(getContext(), actionHandler,
                            mWebContents.isIncognito()));
        }
        mUnselectAllOnActionModeDismiss = true;
        if (mActionMode == null) {
            // There is no ActionMode, so remove the selection.
            mImeAdapter.unselect();
        } else {
            getContentViewClient().onContextualActionBarShown();
        }
    }

    /**
     * Clears the current text selection.
     */
    public void clearSelection() {
        mImeAdapter.unselect();
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

    private void hidePastePopup() {
        if (mPastePopupMenu == null) return;
        mPastePopupMenu.hide();
    }

    @CalledByNative
    private void onSelectionEvent(int eventType, float posXDip, float posYDip) {
        switch (eventType) {
            case SelectionEventType.SELECTION_SHOWN:
                mHasSelection = true;
                mUnselectAllOnActionModeDismiss = true;
                // TODO(cjhopman): Remove this when there is a better signal that long press caused
                // a selection. See http://crbug.com/150151.
                mContainerView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
                showSelectActionBar();
                break;

            case SelectionEventType.SELECTION_CLEARED:
                mHasSelection = false;
                mUnselectAllOnActionModeDismiss = false;
                hideSelectActionBar();
                break;

            case SelectionEventType.SELECTION_DRAG_STARTED:
                break;

            case SelectionEventType.SELECTION_DRAG_STOPPED:
                break;

            case SelectionEventType.INSERTION_SHOWN:
                mHasInsertion = true;
                break;

            case SelectionEventType.INSERTION_MOVED:
                if (mPastePopupMenu == null) break;
                if (!isScrollInProgress() && mPastePopupMenu.isShowing()) {
                    showPastePopup((int) posXDip, (int) posYDip);
                } else {
                    hidePastePopup();
                }
                break;

            case SelectionEventType.INSERTION_TAPPED:
                if (mWasPastePopupShowingOnInsertionDragStart)
                    hidePastePopup();
                else
                    showPastePopup((int) posXDip, (int) posYDip);
                break;

            case SelectionEventType.INSERTION_CLEARED:
                mHasInsertion = false;
                hidePastePopup();
                break;

            case SelectionEventType.INSERTION_DRAG_STARTED:
                mWasPastePopupShowingOnInsertionDragStart =
                        mPastePopupMenu != null && mPastePopupMenu.isShowing();
                hidePastePopup();
                break;

            default:
                assert false : "Invalid selection event type.";
        }

        final float scale = mRenderCoordinates.getDeviceScaleFactor();
        getContentViewClient().onSelectionEvent(eventType, posXDip * scale, posYDip * scale);
    }

    private void hideTextHandles() {
        mHasSelection = false;
        mHasInsertion = false;
        if (mNativeContentViewCore != 0) nativeHideTextHandles(mNativeContentViewCore);
    }

    /**
     * Hides the IME if the containerView is the active view for IME.
     */
    public void hideImeIfNeeded() {
        // Hide input method window from the current view synchronously
        // because ImeAdapter does so asynchronouly with a delay, and
        // by the time when ImeAdapter dismisses the input, the
        // containerView may have lost focus.
        // We cannot trust ContentViewClient#onImeStateChangeRequested to
        // hide the input window because it has an empty default implementation.
        // So we need to explicitly hide the input method window here.
        if (mInputMethodManagerWrapper.isActive(mContainerView)) {
            mInputMethodManagerWrapper.hideSoftInputFromWindow(
                    mContainerView.getWindowToken(), 0, null);
        }
        getContentViewClient().onImeStateChangeRequested(false);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void updateFrameInfo(
            float scrollOffsetX, float scrollOffsetY,
            float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor,
            float contentWidth, float contentHeight,
            float viewportWidth, float viewportHeight,
            float controlsOffsetYCss, float contentOffsetYCss) {
        TraceEvent.begin("ContentViewCore:updateFrameInfo");
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
                controlsOffsetPix, contentOffsetYPix, 0);

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
        TraceEvent.begin();
        mFocusedNodeEditable = (textInputType != ImeAdapter.getTextInputTypeNone());
        if (!mFocusedNodeEditable) hidePastePopup();

        mImeAdapter.updateKeyboardVisibility(
                nativeImeAdapterAndroid, textInputType, textInputFlags, showImeIfNeeded);

        if (mInputConnection != null) {
            mInputConnection.updateState(text, selectionStart, selectionEnd, compositionStart,
                    compositionEnd, isNonImeChange);
        }

        if (mActionMode != null) mActionMode.invalidate();
        TraceEvent.end();
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
            int[] enabled, boolean multiple, int[] selectedIndices) {
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
        if (DeviceFormFactor.isTablet(mContext) && !multiple) {
            mSelectPopup = new SelectPopupDropdown(this, popupItems, bounds, selectedIndices);
        } else {
            mSelectPopup = new SelectPopupDialog(this, popupItems, multiple, selectedIndices);
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
    private TouchEventSynthesizer createTouchEventSynthesizer() {
        return new TouchEventSynthesizer(this);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private PopupTouchHandleDrawable createPopupTouchHandleDrawable() {
        if (mTouchHandleDelegate == null) {
            mTouchHandleDelegate = new PopupTouchHandleDrawableDelegate() {
                @Override
                public View getParent() {
                    return getContainerView();
                }

                @Override
                public PositionObserver getParentPositionObserver() {
                    return mPositionObserver;
                }

                @Override
                public boolean onTouchHandleEvent(MotionEvent event) {
                    final boolean isTouchHandleEvent = true;
                    return onTouchEventImpl(event, isTouchHandleEvent);
                }
            };
        }
        return new PopupTouchHandleDrawable(mTouchHandleDelegate);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onSelectionChanged(String text) {
        mLastSelectedText = text;
        getContentViewClient().onSelectionChanged(text);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void showPastePopupWithFeedback(int xDip, int yDip) {
        // TODO(jdduke): Remove this when there is a better signal that long press caused
        // showing of the paste popup. See http://crbug.com/150151.
        if (showPastePopup(xDip, yDip)) {
            mContainerView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
        }
    }

    private boolean showPastePopup(int xDip, int yDip) {
        if (!mHasInsertion || !canPaste()) return false;
        final float contentOffsetYPix = mRenderCoordinates.getContentOffsetYPix();
        getPastePopup().showAt(
            (int) mRenderCoordinates.fromDipToPix(xDip),
            (int) (mRenderCoordinates.fromDipToPix(yDip) + contentOffsetYPix));
        return true;
    }

    private PastePopupMenu getPastePopup() {
        if (mPastePopupMenu == null) {
            mPastePopupMenu = new PastePopupMenu(getContainerView(),
                new PastePopupMenuDelegate() {
                    @Override
                    public void paste() {
                        mImeAdapter.paste();
                        hideTextHandles();
                    }
                });
        }
        return mPastePopupMenu;
    }

    @VisibleForTesting
    public PastePopupMenu getPastePopupForTest() {
        return getPastePopup();
    }

    private boolean canPaste() {
        if (!mFocusedNodeEditable) return false;
        return ((ClipboardManager) mContext.getSystemService(
                Context.CLIPBOARD_SERVICE)).hasPrimaryClip();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRenderProcessChange() {
        attachImeAdapter();
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
    public boolean hasFocus() {
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

    /**
     * If the view is ready to draw contents to the screen. In hardware mode,
     * the initialization of the surface texture may not occur until after the
     * view has been added to the layout. This method will return {@code true}
     * once the texture is actually ready.
     */
    public boolean isReady() {
        assert mWebContents != null;
        return mWebContents.isReady();
    }

    @CalledByNative
    private void startContentIntent(String contentUrl) {
        getContentViewClient().onStartContentIntent(getContext(), contentUrl);
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
        return mAccessibilityInjector.supportsAccessibilityAction(action);
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
        if (mAccessibilityInjector.supportsAccessibilityAction(action)) {
            return mAccessibilityInjector.performAccessibilityAction(action, arguments);
        }

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

        if (mNativeAccessibilityAllowed &&
                !mNativeAccessibilityEnabled &&
                mNativeContentViewCore != 0 &&
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            mNativeAccessibilityEnabled = true;
            nativeSetAccessibilityEnabled(mNativeContentViewCore, true);
        }

        return null;
    }

    /**
     * @see View#onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo)
     */
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        // Note: this is only used by the script-injecting accessibility code.
        mAccessibilityInjector.onInitializeAccessibilityNodeInfo(info);
    }

    /**
     * @see View#onInitializeAccessibilityEvent(AccessibilityEvent)
     */
    public void onInitializeAccessibilityEvent(AccessibilityEvent event) {
        // Note: this is only used by the script-injecting accessibility code.
        event.setClassName(this.getClass().getName());

        // Identify where the top-left of the screen currently points to.
        event.setScrollX(mRenderCoordinates.getScrollXPixInt());
        event.setScrollY(mRenderCoordinates.getScrollYPixInt());

        // The maximum scroll values are determined by taking the content dimensions and
        // subtracting off the actual dimensions of the ChromeView.
        int maxScrollXPix = Math.max(0, mRenderCoordinates.getMaxHorizontalScrollPixInt());
        int maxScrollYPix = Math.max(0, mRenderCoordinates.getMaxVerticalScrollPixInt());
        event.setScrollable(maxScrollXPix > 0 || maxScrollYPix > 0);

        // Setting the maximum scroll values requires API level 15 or higher.
        final int sdkVersionRequiredToSetScroll = 15;
        if (Build.VERSION.SDK_INT >= sdkVersionRequiredToSetScroll) {
            event.setMaxScrollX(maxScrollXPix);
            event.setMaxScrollY(maxScrollYPix);
        }
    }

    /**
     * Returns whether accessibility script injection is enabled on the device
     */
    public boolean isDeviceAccessibilityScriptInjectionEnabled() {
        try {
            // On JellyBean and higher, native accessibility is the default so script
            // injection is only allowed if enabled via a flag.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN &&
                    !CommandLine.getInstance().hasSwitch(
                            ContentSwitches.ENABLE_ACCESSIBILITY_SCRIPT_INJECTION)) {
                return false;
            }

            if (!mContentSettings.getJavaScriptEnabled()) {
                return false;
            }

            int result = getContext().checkCallingOrSelfPermission(
                    android.Manifest.permission.INTERNET);
            if (result != PackageManager.PERMISSION_GRANTED) {
                return false;
            }

            Field field = Settings.Secure.class.getField("ACCESSIBILITY_SCRIPT_INJECTION");
            field.setAccessible(true);
            String accessibilityScriptInjection = (String) field.get(null);
            ContentResolver contentResolver = getContext().getContentResolver();

            if (mAccessibilityScriptInjectionObserver == null) {
                ContentObserver contentObserver = new ContentObserver(new Handler()) {
                    @Override
                    public void onChange(boolean selfChange, Uri uri) {
                        setAccessibilityState(mAccessibilityManager.isEnabled());
                    }
                };
                contentResolver.registerContentObserver(
                    Settings.Secure.getUriFor(accessibilityScriptInjection),
                    false,
                    contentObserver);
                mAccessibilityScriptInjectionObserver = contentObserver;
            }

            return Settings.Secure.getInt(contentResolver, accessibilityScriptInjection, 0) == 1;
        } catch (NoSuchFieldException e) {
            // Do nothing, default to false.
        } catch (IllegalAccessException e) {
            // Do nothing, default to false.
        }
        return false;
    }

    /**
     * Returns whether or not accessibility injection is being used.
     */
    public boolean isInjectingAccessibilityScript() {
        return mAccessibilityInjector.accessibilityIsAvailable();
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
            setInjectedAccessibility(false);
            mNativeAccessibilityAllowed = false;
            mTouchExplorationEnabled = false;
        } else {
            boolean useScriptInjection = isDeviceAccessibilityScriptInjectionEnabled();
            setInjectedAccessibility(useScriptInjection);
            mNativeAccessibilityAllowed = !useScriptInjection;
            mTouchExplorationEnabled = mAccessibilityManager.isTouchExplorationEnabled();
        }
    }

    /**
     * Enable or disable injected accessibility features
     */
    public void setInjectedAccessibility(boolean enabled) {
        mAccessibilityInjector.addOrRemoveAccessibilityApisIfNecessary();
        mAccessibilityInjector.setScriptEnabled(enabled);
    }

    /**
     * Stop any TTS notifications that are currently going on.
     */
    public void stopCurrentAccessibilityNotifications() {
        mAccessibilityInjector.onPageLostFocus();
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
     * Inform WebKit that Fullscreen mode has been exited by the user.
     */
    public void exitFullscreen() {
        assert mWebContents != null;
        mWebContents.exitFullscreen();
    }

    /**
     * Changes whether hiding the top controls is enabled.
     *
     * @param enableHiding Whether hiding the top controls should be enabled or not.
     * @param enableShowing Whether showing the top controls should be enabled or not.
     * @param animate Whether the transition should be animated or not.
     */
    public void updateTopControlsState(boolean enableHiding, boolean enableShowing,
            boolean animate) {
        assert mWebContents != null;
        mWebContents.updateTopControlsState(
                enableHiding, enableShowing, animate);
    }

    /**
     * @return The cached copy of render positions and scales.
     */
    public RenderCoordinates getRenderCoordinates() {
        return mRenderCoordinates;
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

        mTouchScrollInProgress = false;
        mPotentiallyActiveFlingCount = 0;

        if (touchScrollInProgress) updateGestureStateListener(GestureEventType.SCROLL_END);
        if (potentiallyActiveFlingCount > 0) updateGestureStateListener(GestureEventType.FLING_END);
    }

    private native long nativeInit(long webContentsPtr,
            long viewAndroidPtr, long windowAndroidPtr, HashSet<Object> retainedObjectSet);

    @CalledByNative
    private ContentVideoViewClient getContentVideoViewClient() {
        return getContentViewClient().getContentVideoViewClient();
    }

    @CalledByNative
    private boolean shouldBlockMediaRequest(String url) {
        return getContentViewClient().shouldBlockMediaRequest(url);
    }

    @CalledByNative
    private void onNativeFlingStopped() {
        // Note that mTouchScrollInProgress should normally be false at this
        // point, but we reset it anyway as another failsafe.
        mTouchScrollInProgress = false;
        if (mPotentiallyActiveFlingCount <= 0) return;
        mPotentiallyActiveFlingCount--;
        updateGestureStateListener(GestureEventType.FLING_END);
    }

    @Override
    public void onScreenOrientationChanged(int orientation) {
        sendOrientationChangeEvent(orientation);
    }

    public void resumeResponseDeferredAtStart() {
        assert mWebContents != null;
        mWebContents.resumeResponseDeferredAtStart();
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

    private native WebContents nativeGetWebContentsAndroid(long nativeContentViewCoreImpl);

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
            float rawX, float rawY,
            int androidToolType0, int androidToolType1,
            int androidButtonState, int androidMetaState,
            boolean isTouchHandleEvent);

    private native int nativeSendMouseMoveEvent(
            long nativeContentViewCoreImpl, long timeMs, float x, float y);

    private native int nativeSendMouseWheelEvent(
            long nativeContentViewCoreImpl, long timeMs, float x, float y, float verticalAxis);

    private native void nativeScrollBegin(
            long nativeContentViewCoreImpl, long timeMs, float x, float y, float hintX,
            float hintY);

    private native void nativeScrollEnd(long nativeContentViewCoreImpl, long timeMs);

    private native void nativeScrollBy(
            long nativeContentViewCoreImpl, long timeMs, float x, float y,
            float deltaX, float deltaY);

    private native void nativeFlingStart(
            long nativeContentViewCoreImpl, long timeMs, float x, float y, float vx, float vy);

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

    private native void nativeMoveCaret(long nativeContentViewCoreImpl, float x, float y);

    private native void nativeHideTextHandles(long nativeContentViewCoreImpl);

    private native void nativeResetGestureDetection(long nativeContentViewCoreImpl);

    private native void nativeSetDoubleTapSupportEnabled(
            long nativeContentViewCoreImpl, boolean enabled);

    private native void nativeSetMultiTouchZoomSupportEnabled(
            long nativeContentViewCoreImpl, boolean enabled);

    private native void nativeSelectPopupMenuItems(long nativeContentViewCoreImpl,
            long nativeSelectPopupSourceFrame, int[] indices);


    private native void nativePostMessageToFrame(long nativeContentViewCoreImpl, String frameId,
            String message, String sourceOrigin, String targetOrigin);

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

    private native void nativeExtractSmartClipData(long nativeContentViewCoreImpl,
            int x, int y, int w, int h);

    private native void nativeSetBackgroundOpaque(long nativeContentViewCoreImpl, boolean opaque);
}
