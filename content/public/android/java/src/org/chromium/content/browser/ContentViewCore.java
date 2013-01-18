// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import android.text.Editable;
import android.util.Log;
import android.util.Pair;
import android.view.ActionMode;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.WeakContext;
import org.chromium.content.R;
import org.chromium.content.browser.ContentViewGestureHandler.MotionEventDelegate;
import org.chromium.content.browser.accessibility.AccessibilityInjector;
import org.chromium.content.common.ProcessInitException;
import org.chromium.content.common.TraceEvent;
import org.chromium.ui.gfx.NativeWindow;

import java.lang.annotation.Annotation;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;

/**
 * Provides a Java-side 'wrapper' around a WebContent (native) instance.
 * Contains all the major functionality necessary to manage the lifecycle of a ContentView without
 * being tied to the view system.
 */
@JNINamespace("content")
public class ContentViewCore implements MotionEventDelegate, NavigationClient {
    private static final String TAG = ContentViewCore.class.getName();

    // Used when ContentView implements a standalone View.
    public static final int PERSONALITY_VIEW = 0;
    // Used for Chrome.
    public static final int PERSONALITY_CHROME = 1;

    // Used to avoid enabling zooming in / out if resulting zooming will
    // produce little visible difference.
    private static final float ZOOM_CONTROLS_EPSILON = 0.007f;

    // Used to represent gestures for long press and long tap.
    private static final int IS_LONG_PRESS = 1;
    private static final int IS_LONG_TAP = 2;

    // To avoid checkerboard, we clamp the fling velocity based on the maximum number of tiles
    // should be allowed to upload per 100ms.
    private final int mMaxNumUploadTiles;

    // Personality of the ContentView.
    private final int mPersonality;

    // If the embedder adds a JavaScript interface object that contains an indirect reference to
    // the ContentViewCore, then storing a strong ref to the interface object on the native
    // side would prevent garbage collection of the ContentViewCore (as that strong ref would
    // create a new GC root).
    // For that reason, we store only a weak reference to the interface object on the
    // native side. However we still need a strong reference on the Java side to
    // prevent garbage collection if the embedder doesn't maintain their own ref to the
    // interface object - the Java side ref won't create a new GC root.
    // This map stores those refernces. We put into the map on addJavaScriptInterface()
    // and remove from it in removeJavaScriptInterface().
    private Map<String, Object> mJavaScriptInterfaces = new HashMap<String, Object>();

    // Additionally, we keep track of all Java bound JS objects that are in use on the
    // current page to ensure that they are not garbage collected until the page is
    // navigated. This includes interface objects that have been removed
    // via the removeJavaScriptInterface API and transient objects returned from methods
    // on the interface object.
    // TODO(benm): Implement the transient object retention - likely by moving the
    // management of this set into the native Java bridge. (crbug/169228) (crbug/169228)
    private Set<Object> mRetainedJavaScriptObjects = new HashSet<Object>();

    /**
     * Interface that consumers of {@link ContentViewCore} must implement to allow the proper
     * dispatching of view methods through the containing view.
     *
     * <p>
     * All methods with the "super_" prefix should be routed to the parent of the
     * implementing container view.
     */
    @SuppressWarnings("javadoc")
    public static interface InternalAccessDelegate {
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
        void onScrollChanged(int l, int t, int oldl, int oldt);

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
     * Interface for subscribing to content size changes.
     */
    public static interface ContentSizeChangeListener {
        /**
         * Called when the content size changes.
         * The containing view may want to adjust its size to match the content.
         */
        void onContentSizeChanged(int contentWidth, int contentHeight);
    }

    private final Context mContext;
    private ViewGroup mContainerView;
    private InternalAccessDelegate mContainerViewInternals;
    private WebContentsObserverAndroid mWebContentsObserver;
    private ContentSizeChangeListener mContentSizeChangeListener;

    private ContentViewClient mContentViewClient;

    private ContentSettings mContentSettings;

    // Native pointer to C++ ContentViewCoreImpl object which will be set by nativeInit().
    private int mNativeContentViewCore = 0;

    private boolean mAttachedToWindow = false;

    private ContentViewGestureHandler mContentViewGestureHandler;
    private ZoomManager mZoomManager;

    // Currently ContentView's scrolling is handled by the native side. We keep a cached copy of the
    // scroll offset and content size so that we can display the scrollbar correctly. In the future,
    // we may switch to tile rendering and handle the scrolling in the Java level.

    // Cached scroll offset from the native
    private int mNativeScrollX;
    private int mNativeScrollY;

    // Cached content size from native.
    private int mContentWidth;
    private int mContentHeight;

    // Cached size of the viewport
    private int mViewportWidth;
    private int mViewportHeight;

    // Cached page scale factor from native
    private float mNativePageScaleFactor = 1.0f;
    private float mNativeMinimumScale = 1.0f;
    private float mNativeMaximumScale = 1.0f;

    private PopupZoomer mPopupZoomer;

    private Runnable mFakeMouseMoveRunnable = null;

    // Only valid when focused on a text / password field.
    private ImeAdapter mImeAdapter;
    private ImeAdapter.AdapterInputConnection mInputConnection;

    private SelectionHandleController mSelectionHandleController;
    private InsertionHandleController mInsertionHandleController;
    // These offsets in document space with page scale normalized to 1.0.
    private final PointF mStartHandleNormalizedPoint = new PointF();
    private final PointF mEndHandleNormalizedPoint = new PointF();
    private final PointF mInsertionHandleNormalizedPoint = new PointF();

    // Tracks whether a selection is currently active.  When applied to selected text, indicates
    // whether the last selected text is still highlighted.
    private boolean mHasSelection;
    private String mLastSelectedText;
    private boolean mSelectionEditable;
    private ActionMode mActionMode;

    // Delegate that will handle GET downloads, and be notified of completion of POST downloads.
    private ContentViewDownloadDelegate mDownloadDelegate;

    // Whether a physical keyboard is connected.
    private boolean mKeyboardConnected;

    // The AccessibilityInjector that handles loading Accessibility scripts into the web page.
    private AccessibilityInjector mAccessibilityInjector;

    // Temporary notification to tell onSizeChanged to focus a form element,
    // because the OSK was just brought up.
    private boolean mFocusOnNextSizeChanged = false;
    private boolean mUnfocusOnNextSizeChanged = false;

    private boolean mNeedUpdateOrientationChanged;

    // Used to keep track of whether we should try to undo the last zoom-to-textfield operation.
    private boolean mScrolledAndZoomedFocusedEditableNode = false;

    // Whether we use hardware-accelerated drawing.
    private boolean mHardwareAccelerated = false;

    /**
     * Enable multi-process ContentView. This should be called by the application before
     * constructing any ContentView instances. If enabled, ContentView will run renderers in
     * separate processes up to the number of processes specified by maxRenderProcesses. If this is
     * not called then the default is to run the renderer in the main application on a separate
     * thread.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses Limit on the number of renderers to use. Each tab runs in its own
     * process until the maximum number of processes is reached. The special value of
     * MAX_RENDERERS_SINGLE_PROCESS requests single-process mode where the renderer will run in the
     * application process in a separate thread. If the special value MAX_RENDERERS_AUTOMATIC is
     * used then the number of renderers will be determined based on the device memory class. The
     * maximum number of allowed renderers is capped by MAX_RENDERERS_LIMIT.
     * @return Whether the process actually needed to be initialized (false if already running).
     */
    public static boolean enableMultiProcess(Context context, int maxRendererProcesses)
            throws ProcessInitException {
        return AndroidBrowserProcess.initContentViewProcess(context, maxRendererProcesses);
    }

    /**
     * Initialize the process as the platform browser. This must be called before accessing
     * ContentView in order to treat this as a Chromium browser process.
     *
     * @param context Context used to obtain the application context.
     * @param maxRendererProcesses Same as ContentView.enableMultiProcess()
     * @return Whether the process actually needed to be initialized (false if already running).
     */
    public static boolean initChromiumBrowserProcess(Context context, int maxRendererProcesses)
            throws ProcessInitException {
        return AndroidBrowserProcess.initChromiumBrowserProcess(context, maxRendererProcesses);
    }

    /**
     * Constructs a new ContentViewCore. Embedders must call initialize() after constructing
     * a ContentViewCore and before using it.
     *
     * @param context The context used to create this.
     * @param personality The type of ContentViewCore being created.
     */
    public ContentViewCore(Context context, int personality) {
        mContext = context;

        WeakContext.initializeWeakContext(context);
        mPersonality = personality;
        HeapStatsLogger.init(mContext.getApplicationContext());

        // We should set this constant based on the GPU performance. As it doesn't exist in the
        // framework yet, we use the memory class as an indicator. Here are some good values that
        // we determined via manual experimentation:
        //
        // Device            Screen size        Memory class   Tiles per 100ms
        // ================= ================== ============== =====================
        // Nexus S            480 x  800         128            9 (3 rows portrait)
        // Galaxy Nexus       720 x 1280         256           12 (3 rows portrait)
        // Nexus 7           1280 x  800         384           18 (3 rows landscape)
        // Nexus 10          2560 x 1600         512           44 (4 rows landscape)
        //
        // Here is a spreadsheet with the data, plus a curve fit:
        // https://docs.google.com/a/chromium.org/spreadsheet/pub?key=0AlNYk7HM2CgQdG1vUWRVWkU3ODRTc1B2SVF3ZTJBUkE&output=html
        // That gives us tiles-per-100ms of 8, 13, 22, 37 for the devices listed above.
        // Not too bad, and it should behave reasonably sensibly for unknown devices.
        // If you want to tweak these constants, please update the spreadsheet appropriately.
        //
        // The curve is y = b * m^x, with coefficients as follows:
        double b = 4.70009671080384;
        double m = 1.00404437546897;
        int memoryClass = ((ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE))
                .getLargeMemoryClass();
        mMaxNumUploadTiles = (int) Math.round(b * Math.pow(m, memoryClass));
    }

    /**
     * @return The context used for creating this ContentViewCore.
     */
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
     * Returns a delegate that can be used to add and remove views from the ContainerView.
     *
     * NOTE: Use with care, as not all ContentViewCore users setup their ContainerView in the same
     * way. In particular, the Android WebView has limitations on what implementation details can
     * be provided via a child view, as they are visible in the API and could introduce
     * compatibility breaks with existing applications. If in doubt, contact the
     * android_webview/OWNERS
     *
     * @return A ContainerViewDelegate that can be used to add and remove views.
     */
    @CalledByNative
    public ContainerViewDelegate getContainerViewDelegate() {
        return new ContainerViewDelegate() {
            @Override
            public void addViewToContainerView(View view) {
                mContainerView.addView(view);
            }

            @Override
            public void removeViewFromContainerView(View view) {
                mContainerView.removeView(view);
            }
        };
    }

    public ImeAdapter getImeAdapterForTest() {
        return mImeAdapter;
    }

    private ImeAdapter createImeAdapter(Context context) {
        return new ImeAdapter(context, getSelectionHandleController(),
                getInsertionHandleController(),
                new ImeAdapter.ViewEmbedder() {
                    @Override
                    public void onImeEvent(boolean isFinish) {
                        getContentViewClient().onImeEvent();
                        if (!isFinish) {
                            undoScrollFocusedEditableNodeIntoViewIfNeeded(false);
                        }
                    }

                    @Override
                    public void onSetFieldValue() {
                        scrollFocusedEditableNodeIntoView();
                    }

                    @Override
                    public void onDismissInput() {
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
                                    mFocusOnNextSizeChanged = true;
                                } else if (resultCode ==
                                        InputMethodManager.RESULT_UNCHANGED_SHOWN) {
                                    // If the OSK was already there, focus the form immediately.
                                    scrollFocusedEditableNodeIntoView();
                                } else {
                                    undoScrollFocusedEditableNodeIntoViewIfNeeded(false);
                                }
                            }
                        };
                    }
                }
        );
    }

    /**
     * Returns true if the given Activity has hardware acceleration enabled
     * in its manifest, or in its foreground window.
     *
     * TODO(husky): Remove when initialize() is refactored (see TODO there)
     * TODO(dtrainor) This is still used by other classes.  Make sure to pull some version of this
     * out before removing it.
     */
    public static boolean hasHardwareAcceleration(Activity activity) {
        // Has HW acceleration been enabled manually in the current window?
        Window window = activity.getWindow();
        if (window != null) {
            if ((window.getAttributes().flags
                    & WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED) != 0) {
                return true;
            }
        }

        // Has HW acceleration been enabled in the manifest?
        try {
            ActivityInfo info = activity.getPackageManager().getActivityInfo(
                    activity.getComponentName(), 0);
            if ((info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0) {
                return true;
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.e("Chrome", "getActivityInfo(self) should not fail");
        }

        return false;
    }

    /**
     * Returns true if the given Context is a HW-accelerated Activity.
     *
     * TODO(husky): Remove when initialize() is refactored (see TODO there)
     */
    private static boolean hasHardwareAcceleration(Context context) {
        if (context instanceof Activity) {
            return hasHardwareAcceleration((Activity) context);
        }
        return false;
    }

    /**
     *
     * @param containerView The view that will act as a container for all views created by this.
     * @param internalDispatcher Handles dispatching all hidden or super methods to the
     *                           containerView.
     * @param nativeWebContents A pointer to the native web contents.
     * @param nativeWindow An instance of the NativeWindow.
     * @param isAccessFromFileURLsGrantedByDefault Default WebSettings configuration.
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
            int nativeWebContents, NativeWindow nativeWindow,
            boolean isAccessFromFileURLsGrantedByDefault) {
        // Check whether to use hardware acceleration. This is a bit hacky, and
        // only works if the Context is actually an Activity (as it is in the
        // Chrome application).
        //
        // What we're doing here is checking whether the app has *requested*
        // hardware acceleration by setting the appropriate flags. This does not
        // necessarily mean we're going to *get* hardware acceleration -- that's
        // up to the Android framework.
        //
        // TODO(husky): Once the native code has been updated so that the
        // HW acceleration flag can be set dynamically (Grace is doing this),
        // move this check into onAttachedToWindow(), where we can test for
        // HW support directly.
        mHardwareAccelerated = hasHardwareAcceleration(mContext);

        // Input events are delivered at vsync time on JB+.
        boolean inputEventsDeliveredAtVSync =
                (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN);

        mContainerView = containerView;
        mNativeContentViewCore = nativeInit(mHardwareAccelerated, inputEventsDeliveredAtVSync,
                nativeWebContents, nativeWindow.getNativePointer());
        mContentSettings = new ContentSettings(
                this, mNativeContentViewCore, isAccessFromFileURLsGrantedByDefault);
        initializeContainerView(internalDispatcher);
        if (mPersonality == PERSONALITY_VIEW) {
            setAllUserAgentOverridesInHistory();
        }

        mAccessibilityInjector = AccessibilityInjector.newInstance(this);
        mAccessibilityInjector.addOrRemoveAccessibilityApisIfNecessary();

        String contentDescription = "Web View";
        if (R.string.accessibility_content_view == 0) {
            Log.w(TAG, "Setting contentDescription to 'Web View' as no value was specified.");
        } else {
            contentDescription = mContext.getResources().getString(
                    R.string.accessibility_content_view);
        }
        mContainerView.setContentDescription(contentDescription);
        mWebContentsObserver = new WebContentsObserverAndroid(this) {
            @Override
            public void didStartLoading(String url) {
                hidePopupDialog();
                resetGestureDetectors();
                // TODO(benm): This isn't quite the right place to do this. Management
                // of this set should be moving into the native java bridge in crbug/169228
                // and until that's ready this will do.
                mRetainedJavaScriptObjects.clear();
            }
        };
    }

    @CalledByNative
    void onNativeContentViewCoreDestroyed(int nativeContentViewCore) {
        assert nativeContentViewCore == mNativeContentViewCore;
        mNativeContentViewCore = 0;
    }

    /**
     * Initializes the View that will contain all Views created by the ContentViewCore.
     *
     * @param internalDispatcher Handles dispatching all hidden or super methods to the
     *                           containerView.
     */
    private void initializeContainerView(InternalAccessDelegate internalDispatcher) {
        TraceEvent.begin();
        mContainerViewInternals = internalDispatcher;

        mContainerView.setWillNotDraw(false);
        mContainerView.setFocusable(true);
        mContainerView.setFocusableInTouchMode(true);
        mContainerView.setClickable(true);

        if (mPersonality == PERSONALITY_CHROME) {
            // Doing this in PERSONALITY_VIEW mode causes rendering problems in our
            // current WebView test case (the HTMLViewer application).
            // TODO(benm): Figure out why this is the case.
            if (mContainerView.getScrollBarStyle() == View.SCROLLBARS_INSIDE_OVERLAY) {
                mContainerView.setHorizontalScrollBarEnabled(false);
                mContainerView.setVerticalScrollBarEnabled(false);
            }
        }

        mZoomManager = new ZoomManager(mContext, this);
        mZoomManager.updateMultiTouchSupport();
        mContentViewGestureHandler = new ContentViewGestureHandler(mContext, this, mZoomManager);

        mNativeScrollX = mNativeScrollY = 0;
        mNativePageScaleFactor = 1.0f;
        initPopupZoomer(mContext);
        mImeAdapter = createImeAdapter(mContext);
        mKeyboardConnected = mContainerView.getResources().getConfiguration().keyboard
                != Configuration.KEYBOARD_NOKEYS;
        TraceEvent.end();
    }

    private void initPopupZoomer(Context context){
        mPopupZoomer = new PopupZoomer(context);
        mPopupZoomer.setOnVisibilityChangedListener(new PopupZoomer.OnVisibilityChangedListener() {
            @Override
            public void onPopupZoomerShown(final PopupZoomer zoomer) {
                mContainerView.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mContainerView.indexOfChild(zoomer) == -1) {
                            mContainerView.addView(zoomer);
                        } else {
                            assert false : "PopupZoomer should never be shown without being hidden";
                        }
                    }
                });
            }

            @Override
            public void onPopupZoomerHidden(final PopupZoomer zoomer) {
                mContainerView.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mContainerView.indexOfChild(zoomer) != -1) {
                            mContainerView.removeView(zoomer);
                            mContainerView.invalidate();
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
            @Override
            public boolean onSingleTap(View v, MotionEvent e) {
                mContainerView.requestFocus();
                if (mNativeContentViewCore != 0) {
                    nativeSingleTap(mNativeContentViewCore, e.getEventTime(), (int) e.getX(),
                            (int) e.getY(), true);
                }
                return true;
            }

            @Override
            public boolean onLongPress(View v, MotionEvent e) {
                if (mNativeContentViewCore != 0) {
                    nativeLongPress(mNativeContentViewCore, e.getEventTime(), (int) e.getX(),
                            (int) e.getY(), true);
                }
                return true;
            }
        };
        mPopupZoomer.setOnTapListener(listener);
    }

    /**
     * @return Whether the configured personality of this ContentView is {@link #PERSONALITY_VIEW}.
     */
    boolean isPersonalityView() {
        switch (mPersonality) {
            case PERSONALITY_VIEW:
                return true;
            case PERSONALITY_CHROME:
                return false;
            default:
                Log.e(TAG, "Unknown ContentView personality: " + mPersonality);
                return false;
        }
    }

    /**
     * Destroy the internal state of the ContentView. This method may only be
     * called after the ContentView has been removed from the view system. No
     * other methods may be called on this ContentView after this method has
     * been called.
     */
    public void destroy() {
        hidePopupDialog();
        if (mNativeContentViewCore != 0) {
            nativeOnJavaContentViewCoreDestroyed(mNativeContentViewCore);
        }
        mNativeContentViewCore = 0;
        mContentSettings = null;
        mJavaScriptInterfaces.clear();
        mRetainedJavaScriptObjects.clear();
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
    public int getNativeContentViewCore() {
        return mNativeContentViewCore;
    }

    /**
     * For internal use. Throws IllegalStateException if mNativeContentView is 0.
     * Use this to ensure we get a useful Java stack trace, rather than a native
     * crash dump, from use-after-destroy bugs in Java code.
     */
    void checkIsAlive() throws IllegalStateException {
        if (!isAlive()) {
            throw new IllegalStateException("ContentView used after destroy() was called");
        }
    }

    public void setContentViewClient(ContentViewClient client) {
        if (client == null) {
            throw new IllegalArgumentException("The client can't be null.");
        }
        mContentViewClient = client;
    }

    ContentViewClient getContentViewClient() {
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

    public void setContentSizeChangeListener(ContentSizeChangeListener listener) {
        mContentSizeChangeListener = listener;
    }

    public int getBackgroundColor() {
        if (mNativeContentViewCore != 0) {
            return nativeGetBackgroundColor(mNativeContentViewCore);
        }
        return Color.WHITE;
    }

    public void setBackgroundColor(int color) {
        if (mNativeContentViewCore != 0 && getBackgroundColor() != color) {
            nativeSetBackgroundColor(mNativeContentViewCore, color);
        }
    }

    /**
     * Load url without fixing up the url string. Consumers of ContentView are responsible for
     * ensuring the URL passed in is properly formatted (i.e. the scheme has been added if left
     * off during user input).
     *
     * @param pararms Parameters for this load.
     */
    public void loadUrl(LoadUrlParams params) {
        if (mNativeContentViewCore == 0) return;
        if (isPersonalityView()) {
            // For WebView, always use the user agent override, which is set
            // every time the user agent in ContentSettings is modified.
            params.setOverrideUserAgent(LoadUrlParams.UA_OVERRIDE_TRUE);
        }

        nativeLoadUrl(mNativeContentViewCore,
                params.mUrl,
                params.mLoadUrlType,
                params.mTransitionType,
                params.mUaOverrideOption,
                params.getExtraHeadersString(),
                params.mPostData,
                params.mBaseUrlForDataUrl,
                params.mVirtualUrlForDataUrl,
                params.mCanLoadLocalResources);
    }

    void setAllUserAgentOverridesInHistory() {
        nativeSetAllUserAgentOverridesInHistory(mNativeContentViewCore,
                mContentSettings.getUserAgentString());
    }

    /**
     * Stops loading the current web contents.
     */
    public void stopLoading() {
        if (mNativeContentViewCore != 0) nativeStopLoading(mNativeContentViewCore);
    }

    /**
     * Get the URL of the current page.
     *
     * @return The URL of the current page.
     */
    public String getUrl() {
        if (mNativeContentViewCore != 0) return nativeGetURL(mNativeContentViewCore);
        return null;
    }

    /**
     * Get the title of the current page.
     *
     * @return The title of the current page.
     */
    public String getTitle() {
        if (mNativeContentViewCore != 0) return nativeGetTitle(mNativeContentViewCore);
        return null;
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
        if (mNativeContentViewCore == 0) return;
        nativeShowInterstitialPage(mNativeContentViewCore, url, delegate.getNative());
    }

    /**
     * @return Whether the page is currently showing an interstitial, such as a bad HTTPS page.
     */
    public boolean isShowingInterstitialPage() {
        return mNativeContentViewCore == 0 ?
                false : nativeIsShowingInterstitialPage(mNativeContentViewCore);
    }

    /**
     * Indicate that the browser compositor has consumed a pending renderer frame.
     *
     * @return Whether there was a pending renderer frame.
     */
    public boolean consumePendingRendererFrame() {
        return mNativeContentViewCore == 0 ?
                false : nativeConsumePendingRendererFrame(mNativeContentViewCore);
    }

    @CalledByNative
    public int getWidth() {
        return mViewportWidth;
    }

    @CalledByNative
    public int getHeight() {
        return mViewportHeight;
    }

    /**
     * @see android.webkit.WebView#getContentHeight()
     */
    public int getContentHeight() {
        return (int) (mContentHeight / mNativePageScaleFactor);
    }

    /**
     * @see android.webkit.WebView#getContentWidth()
     */
    public int getContentWidth() {
        return (int) (mContentWidth / mNativePageScaleFactor);
    }

    public Bitmap getBitmap() {
        return getBitmap(getWidth(), getHeight());
    }

    public Bitmap getBitmap(int width, int height) {
        if (width == 0 || height == 0 || getWidth() == 0 || getHeight() == 0) return null;

        Bitmap b = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);

        if (mNativeContentViewCore != 0 &&
                nativePopulateBitmapFromCompositor(mNativeContentViewCore, b)) {
            // If we successfully grabbed a bitmap, check if we have to draw the Android overlay
            // components as well.
            if (mContainerView.getChildCount() > 0) {
                Canvas c = new Canvas(b);
                c.scale(width / (float) getWidth(), height / (float) getHeight());
                mContainerView.draw(c);
            }
            return b;
        }

        return null;
    }

    /**
     * Generates a bitmap of the content that is performance optimized based on capture time.
     *
     * <p>
     * To have a consistent capture time across devices, we will scale down the captured bitmap
     * where necessary to reduce the time to generate the bitmap.
     *
     * @param width The width of the content to be captured.
     * @param height The height of the content to be captured.
     * @return A pair of the generated bitmap, and the scale that needs to be applied to return the
     *         bitmap to it's original size (i.e. if the bitmap is scaled down 50%, this
     *         will be 2).
     */
    public Pair<Bitmap, Float> getScaledPerformanceOptimizedBitmap(int width, int height) {
        float scale = 1f;
        // On tablets, always scale down to MDPI for performance reasons.
        if (DeviceUtils.isTablet(getContext())) {
            scale = getContext().getResources().getDisplayMetrics().density;
        }
        return Pair.create(
                getBitmap((int) (width / scale), (int) (height / scale)),
                scale);
    }

    /**
     * @return Whether the ContentView is covered by an overlay that is more than half
     *         of it's surface. This is used to determine if we need to do a slow bitmap capture or
     *         to show the ContentView without them.
     */
    public boolean hasLargeOverlay() {
        // TODO(nileshagrawal): Implement this.
        return false;
    }

    /**
     * @return Whether the current WebContents has a previous navigation entry.
     */
    public boolean canGoBack() {
        return mNativeContentViewCore != 0 && nativeCanGoBack(mNativeContentViewCore);
    }

    /**
     * @return Whether the current WebContents has a navigation entry after the current one.
     */
    public boolean canGoForward() {
        return mNativeContentViewCore != 0 && nativeCanGoForward(mNativeContentViewCore);
    }

    /**
     * @param offset The offset into the navigation history.
     * @return Whether we can move in history by given offset
     */
    public boolean canGoToOffset(int offset) {
        return mNativeContentViewCore != 0 && nativeCanGoToOffset(mNativeContentViewCore, offset);
    }

    /**
     * Navigates to the specified offset from the "current entry". Does nothing if the offset is out
     * of bounds.
     * @param offset The offset into the navigation history.
     */
    public void goToOffset(int offset) {
        if (mNativeContentViewCore != 0) nativeGoToOffset(mNativeContentViewCore, offset);
    }

    @Override
    public void goToNavigationIndex(int index) {
        if (mNativeContentViewCore != 0) nativeGoToNavigationIndex(mNativeContentViewCore, index);
    }

    /**
     * Goes to the navigation entry before the current one.
     */
    public void goBack() {
        if (mNativeContentViewCore != 0) nativeGoBack(mNativeContentViewCore);
    }

    /**
     * Goes to the navigation entry following the current one.
     */
    public void goForward() {
        if (mNativeContentViewCore != 0) nativeGoForward(mNativeContentViewCore);
    }

    /**
     * Reload the current page.
     */
    public void reload() {
        mAccessibilityInjector.addOrRemoveAccessibilityApisIfNecessary();
        if (mNativeContentViewCore != 0) nativeReload(mNativeContentViewCore);
    }

    /**
     * Cancel the pending reload.
     */
    public void cancelPendingReload() {
        if (mNativeContentViewCore != 0) nativeCancelPendingReload(mNativeContentViewCore);
    }

    /**
     * Continue the pending reload.
     */
    public void continuePendingReload() {
        if (mNativeContentViewCore != 0) nativeContinuePendingReload(mNativeContentViewCore);
    }

    /**
     * Clears the ContentViewCore's page history in both the backwards and
     * forwards directions.
     */
    public void clearHistory() {
        if (mNativeContentViewCore != 0) nativeClearHistory(mNativeContentViewCore);
    }

    String getSelectedText() {
        return mHasSelection ? mLastSelectedText : "";
    }

    // End FrameLayout overrides.

    /**
     * @see {@link android.webkit.WebView#flingScroll(int, int)}
     */
    public void flingScroll(int vx, int vy) {
        // Notes:
        //   (1) Use large negative values for the x/y parameters so we don't accidentally scroll a
        //       nested frame.
        //   (2) vx and vy are inverted to match WebView behavior.
        mContentViewGestureHandler.fling(
                System.currentTimeMillis(), -Integer.MAX_VALUE, -Integer.MIN_VALUE, -vx, -vy);
    }

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    public boolean onTouchEvent(MotionEvent event) {
        undoScrollFocusedEditableNodeIntoViewIfNeeded(false);
        return mContentViewGestureHandler.onTouchEvent(event);
    }

    /**
     * @return ContentViewGestureHandler for all MotionEvent and gesture related calls.
     */
    ContentViewGestureHandler getContentViewGestureHandler() {
        return mContentViewGestureHandler;
    }

    @Override
    public boolean sendTouchEvent(long timeMs, int action, TouchPoint[] pts) {
        if (mNativeContentViewCore != 0) {
            return nativeSendTouchEvent(mNativeContentViewCore, timeMs, action, pts);
        }
        return false;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void hasTouchEventHandlers(boolean hasTouchHandlers) {
        mContentViewGestureHandler.hasTouchEventHandlers(hasTouchHandlers);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void confirmTouchEvent(int ackResult) {
        mContentViewGestureHandler.confirmTouchEvent(ackResult);
    }

    @Override
    public boolean sendGesture(int type, long timeMs, int x, int y, Bundle b) {
        if (mNativeContentViewCore == 0) return false;
        switch (type) {
            case ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE:
                nativeShowPressState(mNativeContentViewCore, timeMs, x, y);
                return true;
            case ContentViewGestureHandler.GESTURE_SHOW_PRESS_CANCEL:
                nativeShowPressCancel(mNativeContentViewCore, timeMs, x, y);
                return true;
            case ContentViewGestureHandler.GESTURE_DOUBLE_TAP:
                nativeDoubleTap(mNativeContentViewCore, timeMs, x, y);
                return true;
            case ContentViewGestureHandler.GESTURE_SINGLE_TAP_UP:
                nativeSingleTap(mNativeContentViewCore, timeMs, x, y, false);
                return true;
            case ContentViewGestureHandler.GESTURE_SINGLE_TAP_CONFIRMED:
                handleTapOrPress(timeMs, x, y, 0,
                        b.getBoolean(ContentViewGestureHandler.SHOW_PRESS, false));
                return true;
            case ContentViewGestureHandler.GESTURE_LONG_PRESS:
                handleTapOrPress(timeMs, x, y, IS_LONG_PRESS, false);
                return true;
            case ContentViewGestureHandler.GESTURE_LONG_TAP:
                handleTapOrPress(timeMs, x, y, IS_LONG_TAP, false);
                return true;
            case ContentViewGestureHandler.GESTURE_SCROLL_START:
                nativeScrollBegin(mNativeContentViewCore, timeMs, x, y);
                return true;
            case ContentViewGestureHandler.GESTURE_SCROLL_BY: {
                int dx = b.getInt(ContentViewGestureHandler.DISTANCE_X);
                int dy = b.getInt(ContentViewGestureHandler.DISTANCE_Y);
                nativeScrollBy(mNativeContentViewCore, timeMs, x, y, dx, dy);
                return true;
            }
            case ContentViewGestureHandler.GESTURE_SCROLL_END:
                nativeScrollEnd(mNativeContentViewCore, timeMs);
                return true;
            case ContentViewGestureHandler.GESTURE_FLING_START:
                nativeFlingStart(mNativeContentViewCore, timeMs, x, y,
                        clampFlingVelocityX(b.getInt(ContentViewGestureHandler.VELOCITY_X, 0)),
                        clampFlingVelocityY(b.getInt(ContentViewGestureHandler.VELOCITY_Y, 0)));
                return true;
            case ContentViewGestureHandler.GESTURE_FLING_CANCEL:
                nativeFlingCancel(mNativeContentViewCore, timeMs);
                return true;
            case ContentViewGestureHandler.GESTURE_PINCH_BEGIN:
                nativePinchBegin(mNativeContentViewCore, timeMs, x, y);
                return true;
            case ContentViewGestureHandler.GESTURE_PINCH_BY:
                nativePinchBy(mNativeContentViewCore, timeMs, x, y,
                        b.getFloat(ContentViewGestureHandler.DELTA, 0));
                return true;
            case ContentViewGestureHandler.GESTURE_PINCH_END:
                nativePinchEnd(mNativeContentViewCore, timeMs);
                return true;
            default:
                return false;
        }
    }

    public interface JavaScriptCallback {
        void handleJavaScriptResult(String jsonResult);
    }

    /**
     * Injects the passed Javascript code in the current page and evaluates it.
     * If a result is required, pass in a callback.
     * Used in automation tests.
     *
     * @param script The Javascript to execute.
     * @param message The callback to be fired off when a result is ready. The script's
     *                result will be json encoded and passed as the parameter, and the call
     *                will be made on the main thread.
     *                If no result is required, pass null.
     * @throws IllegalStateException If the ContentView has been destroyed.
     */
    public void evaluateJavaScript(
            String script, JavaScriptCallback callback) throws IllegalStateException {
        checkIsAlive();
        nativeEvaluateJavaScript(mNativeContentViewCore, script, callback);
    }

    /**
     * This method should be called when the containing activity is paused.
     */
    public void onActivityPause() {
        TraceEvent.begin();
        hidePopupDialog();
        nativeOnHide(mNativeContentViewCore);
        setAccessibilityState(false);
        TraceEvent.end();
    }

    /**
     * This method should be called when the containing activity is resumed.
     */
    public void onActivityResume() {
        nativeOnShow(mNativeContentViewCore);
        setAccessibilityState(true);
    }

    /**
     * To be called when the ContentView is shown.
     */
    public void onShow() {
        nativeOnShow(mNativeContentViewCore);
        setAccessibilityState(true);
    }

    /**
     * To be called when the ContentView is hidden.
     */
    public void onHide() {
        hidePopupDialog();
        setAccessibilityState(false);
        nativeOnHide(mNativeContentViewCore);
    }

    /**
     * Return the ContentSettings object used to control the settings for this
     * ContentViewCore.
     *
     * Note that when ContentView is used in the PERSONALITY_CHROME role,
     * ContentSettings can only be used for retrieving settings values. For
     * modifications, ChromeNativePreferences is to be used.
     * @return A ContentSettings object that can be used to control this
     *         ContentViewCore's settings.
     */
    public ContentSettings getContentSettings() {
        return mContentSettings;
    }

    @Override
    public boolean didUIStealScroll(float x, float y) {
        return getContentViewClient().shouldOverrideScroll(
                x, y, computeHorizontalScrollOffset(), computeVerticalScrollOffset());
    }

    @Override
    public boolean hasFixedPageScale() {
        return mNativeMinimumScale == mNativeMaximumScale;
    }

    private void hidePopupDialog() {
        SelectPopupDialog.hide(this);
        hideHandles();
        hideSelectActionBar();
    }

    void hideSelectActionBar() {
        if (mActionMode != null) {
            mActionMode.finish();
        }
    }

    private void resetGestureDetectors() {
        mContentViewGestureHandler.resetGestureHandlers();
    }

    /**
     * @see View#onAttachedToWindow()
     */
    @SuppressWarnings("javadoc")
    public void onAttachedToWindow() {
        mAttachedToWindow = true;
        if (mNativeContentViewCore != 0) {
            int pid = nativeGetCurrentRenderProcessId(mNativeContentViewCore);
            if (pid > 0) {
                SandboxedProcessLauncher.bindAsHighPriority(pid);
            }
        }
        setAccessibilityState(true);
    }

    /**
     * @see View#onDetachedFromWindow()
     */
    @SuppressWarnings("javadoc")
    public void onDetachedFromWindow() {
        mAttachedToWindow = false;
        if (mNativeContentViewCore != 0) {
            int pid = nativeGetCurrentRenderProcessId(mNativeContentViewCore);
            if (pid > 0) {
                SandboxedProcessLauncher.unbindAsHighPriority(pid);
            }
        }
        setAccessibilityState(false);
    }

    /**
     * @see View#onVisibilityChanged(android.view.View, int)
     */
    public void onVisibilityChanged(View changedView, int visibility) {
      if (visibility != View.VISIBLE) {
          if (mContentSettings.supportZoom()) {
              mZoomManager.dismissZoomPicker();
          }
      }
    }

    @CalledByNative
    private void onWebPreferencesUpdated() {
        mContentSettings.syncSettings();
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
        mInputConnection = ImeAdapter.AdapterInputConnection.getInstance(
                mContainerView, mImeAdapter, outAttrs);
        return mInputConnection;
    }

    public Editable getEditableForTest() {
        return mInputConnection.getEditable();
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

        mKeyboardConnected = newConfig.keyboard != Configuration.KEYBOARD_NOKEYS;

        if (mKeyboardConnected) {
            mImeAdapter.attach(nativeGetNativeImeAdapter(mNativeContentViewCore),
                    ImeAdapter.sTextInputTypeNone);
            InputMethodManager manager = (InputMethodManager)
                    getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            manager.restartInput(mContainerView);
        }
        mContainerViewInternals.super_onConfigurationChanged(newConfig);
        mNeedUpdateOrientationChanged = true;
        TraceEvent.end();
    }

    /**
     * @see View#onSizeChanged(int, int, int, int)
     */
    @SuppressWarnings("javadoc")
    public void onSizeChanged(int w, int h, int ow, int oh) {
        mPopupZoomer.hide(false);

        // Update the content size to make sure it is at least the View size
        if (mContentWidth < w) mContentWidth = w;
        if (mContentHeight < h) mContentHeight = h;

        if (mViewportWidth != w || mViewportHeight != h) {
            mViewportWidth = w;
            mViewportHeight = h;
            if (mNativeContentViewCore != 0) {
                nativeSetSize(mNativeContentViewCore, mViewportWidth, mViewportHeight);
            }
        }

        updateAfterSizeChanged();
    }

    public void updateAfterSizeChanged() {
        // Execute a delayed form focus operation because the OSK was brought
        // up earlier.
        if (mFocusOnNextSizeChanged) {
            scrollFocusedEditableNodeIntoView();
            mFocusOnNextSizeChanged = false;
        } else if (mUnfocusOnNextSizeChanged) {
            undoScrollFocusedEditableNodeIntoViewIfNeeded(true);
            mUnfocusOnNextSizeChanged = false;
        }

        if (mNeedUpdateOrientationChanged) {
            sendOrientationChangeEvent();
            mNeedUpdateOrientationChanged = false;
        }
    }

    private void scrollFocusedEditableNodeIntoView() {
        if (mNativeContentViewCore != 0) {
            Runnable scrollTask = new Runnable() {
                @Override
                public void run() {
                    if (mNativeContentViewCore != 0) {
                        nativeScrollFocusedEditableNodeIntoView(mNativeContentViewCore);
                    }
                }
            };

            scrollTask.run();

            // The native side keeps track of whether the zoom and scroll actually occurred. It is
            // more efficient to do it this way and sometimes fire an unnecessary message rather
            // than synchronize with the renderer and always have an additional message.
            mScrolledAndZoomedFocusedEditableNode = true;
        }
    }

    private void undoScrollFocusedEditableNodeIntoViewIfNeeded(boolean backButtonPressed) {
        // The only call to this function that matters is the first call after the
        // scrollFocusedEditableNodeIntoView function call.
        // If the first call to this function is a result of a back button press we want to undo the
        // preceding scroll. If the call is a result of some other action we don't want to perform
        // an undo.
        // All subsequent calls are ignored since only the scroll function sets
        // mScrolledAndZoomedFocusedEditableNode to true.
        if (mScrolledAndZoomedFocusedEditableNode && backButtonPressed &&
                mNativeContentViewCore != 0) {
            Runnable scrollTask = new Runnable() {
                @Override
                public void run() {
                    if (mNativeContentViewCore != 0) {
                        nativeUndoScrollFocusedEditableNodeIntoView(mNativeContentViewCore);
                    }
                }
            };

            scrollTask.run();
        }
        mScrolledAndZoomedFocusedEditableNode = false;
    }

    /**
     * @see View#onFocusedChanged(boolean, int, Rect)
     */
    @SuppressWarnings("javadoc")
    public void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
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
            if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && mImeAdapter.isActive()) {
                mUnfocusOnNextSizeChanged = true;
            } else {
                undoScrollFocusedEditableNodeIntoViewIfNeeded(false);
            }
            return mContainerViewInternals.super_dispatchKeyEventPreIme(event);
        } finally {
            TraceEvent.end();
        }
    }

    /**
     * @see View#dispatchKeyEvent(KeyEvent)
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mImeAdapter != null &&
                !mImeAdapter.isNativeImeAdapterAttached() && mNativeContentViewCore != 0) {
            mImeAdapter.attach(nativeGetNativeImeAdapter(mNativeContentViewCore));
        }

        if (getContentViewClient().shouldOverrideKeyEvent(event)) {
            return mContainerViewInternals.super_dispatchKeyEvent(event);
        }

        if (mKeyboardConnected && mImeAdapter.dispatchKeyEvent(event)) return true;

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
        mContainerView.removeCallbacks(mFakeMouseMoveRunnable);
        if (mNativeContentViewCore != 0) {
            nativeSendMouseMoveEvent(mNativeContentViewCore, event.getEventTime(),
                    (int) event.getX(), (int) event.getY());
        }
        TraceEvent.end("onHoverEvent");
        return true;
    }

    /**
     * @see View#onGenericMotionEvent(MotionEvent)
     */
    public boolean onGenericMotionEvent(MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_SCROLL:
                    nativeSendMouseWheelEvent(mNativeContentViewCore, event.getEventTime(),
                            (int) event.getX(), (int) event.getY(),
                            event.getAxisValue(MotionEvent.AXIS_VSCROLL));

                    mContainerView.removeCallbacks(mFakeMouseMoveRunnable);
                    // Send a delayed onMouseMove event so that we end
                    // up hovering over the right position after the scroll.
                    final MotionEvent eventFakeMouseMove = MotionEvent.obtain(event);
                    mFakeMouseMoveRunnable = new Runnable() {
                          @Override
                          public void run() {
                              onHoverEvent(eventFakeMouseMove);
                          }
                    };
                    mContainerView.postDelayed(mFakeMouseMoveRunnable, 250);
                    return true;
            }
        }
        return mContainerViewInternals.super_onGenericMotionEvent(event);
    }

    /**
     * @see View#scrollBy(int, int)
     * Currently the ContentView scrolling happens in the native side. In
     * the Java view system, it is always pinned at (0, 0). scrollBy() and scrollTo()
     * are overridden, so that View's mScrollX and mScrollY will be unchanged at
     * (0, 0). This is critical for drawing ContentView correctly.
     */
    public void scrollBy(int x, int y) {
        if (mNativeContentViewCore != 0) {
            nativeScrollBy(mNativeContentViewCore, System.currentTimeMillis(), 0, 0, x, y);
        }
    }

    /**
     * @see View#scrollTo(int, int)
     */
    public void scrollTo(int x, int y) {
        if (mNativeContentViewCore == 0) return;
        int dx = x - mNativeScrollX, dy = y - mNativeScrollY;
        if (dx != 0 || dy != 0) {
            long time = System.currentTimeMillis();
            nativeScrollBegin(mNativeContentViewCore, time, mNativeScrollX, mNativeScrollY);
            nativeScrollBy(mNativeContentViewCore, time, mNativeScrollX, mNativeScrollY,
                    dx, dy);
            nativeScrollEnd(mNativeContentViewCore, time);
        }
    }

    // NOTE: this can go away once ContentView.getScrollX() reports correct values.
    //       see: b/6029133
    public int getNativeScrollXForTest() {
        return mNativeScrollX;
    }

    // NOTE: this can go away once ContentView.getScrollY() reports correct values.
    //       see: b/6029133
    public int getNativeScrollYForTest() {
        return mNativeScrollY;
    }

    /**
     * @see View#computeHorizontalScrollExtent()
     */
    @SuppressWarnings("javadoc")
    public int computeHorizontalScrollExtent() {
        return getWidth();
    }

    /**
     * @see View#computeHorizontalScrollOffset()
     */
    @SuppressWarnings("javadoc")
    public int computeHorizontalScrollOffset() {
        return mNativeScrollX;
    }

    /**
     * @see View#computeHorizontalScrollRange()
     */
    @SuppressWarnings("javadoc")
    public int computeHorizontalScrollRange() {
        return mContentWidth;
    }

    /**
     * @see View#computeVerticalScrollExtent()
     */
    @SuppressWarnings("javadoc")
    public int computeVerticalScrollExtent() {
        return getHeight();
    }

    /**
     * @see View#computeVerticalScrollOffset()
     */
    @SuppressWarnings("javadoc")
    public int computeVerticalScrollOffset() {
        return mNativeScrollY;
    }

    /**
     * @see View#computeVerticalScrollRange()
     */
    @SuppressWarnings("javadoc")
    public int computeVerticalScrollRange() {
        return mContentHeight;
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

    @SuppressWarnings("unused")
    @CalledByNative
    private void onTabCrash() {
        getContentViewClient().onTabCrash();
    }

    private void handleTapOrPress(
            long timeMs, int x, int y, int isLongPressOrTap, boolean showPress) {
        if (!mContainerView.isFocused()) mContainerView.requestFocus();

        if (!mPopupZoomer.isShowing()) mPopupZoomer.setLastTouch(x, y);

        if (isLongPressOrTap == IS_LONG_PRESS) {
            getInsertionHandleController().allowAutomaticShowing();
            getSelectionHandleController().allowAutomaticShowing();
            if (mNativeContentViewCore != 0) {
                nativeLongPress(mNativeContentViewCore, timeMs, x, y, false);
            }
        } else if (isLongPressOrTap == IS_LONG_TAP) {
            getInsertionHandleController().allowAutomaticShowing();
            getSelectionHandleController().allowAutomaticShowing();
            if (mNativeContentViewCore != 0) {
                nativeLongTap(mNativeContentViewCore, timeMs, x, y, false);
            }
        } else {
            if (!showPress && mNativeContentViewCore != 0) {
                nativeShowPressState(mNativeContentViewCore, timeMs, x, y);
            }
            if (mSelectionEditable) getInsertionHandleController().allowAutomaticShowing();
            if (mNativeContentViewCore != 0) {
                nativeSingleTap(mNativeContentViewCore, timeMs, x, y, false);
            }
        }
    }

    void updateMultiTouchZoomSupport() {
        mZoomManager.updateMultiTouchSupport();
    }

    public boolean isMultiTouchZoomSupported() {
        return mZoomManager.isMultiTouchZoomSupported();
    }

    void selectPopupMenuItems(int[] indices) {
        if (mNativeContentViewCore != 0) {
            nativeSelectPopupMenuItems(mNativeContentViewCore, indices);
        }
    }

    /*
     * To avoid checkerboard, we clamp the fling velocity based on the maximum number of tiles
     * allowed to be uploaded per 100ms. Calculation is limited to one direction. We assume the
     * tile size is 256x256. The precise distance / velocity should be calculated based on the
     * logic in Scroller.java. As it is almost linear for the first 100ms, we use a simple math.
     */
    private int clampFlingVelocityX(int velocity) {
        int cols = mMaxNumUploadTiles / (int) (Math.ceil((float) getHeight() / 256) + 1);
        int maxVelocity = cols > 0 ? cols * 2560 : 1000;
        if (Math.abs(velocity) > maxVelocity) {
            return velocity > 0 ? maxVelocity : -maxVelocity;
        } else {
            return velocity;
        }
    }

    private int clampFlingVelocityY(int velocity) {
        int rows = mMaxNumUploadTiles / (int) (Math.ceil((float) getWidth() / 256) + 1);
        int maxVelocity = rows > 0 ? rows * 2560 : 1000;
        if (Math.abs(velocity) > maxVelocity) {
            return velocity > 0 ? maxVelocity : -maxVelocity;
        } else {
            return velocity;
        }
    }

    /**
     * Get the screen orientation from the OS and push it to WebKit.
     *
     * TODO(husky): Add a hook for mock orientations.
     *
     * TODO(husky): Currently each new tab starts with an orientation of 0 until you actually
     * rotate the device. This is wrong if you actually started in landscape mode. To fix this, we
     * need to push the correct orientation, but only after WebKit's Frame object has been fully
     * initialized. Need to find a good time to do that. onPageFinished() would probably work but
     * it isn't implemented yet.
     */
    private void sendOrientationChangeEvent() {
        if (mNativeContentViewCore == 0) return;

        WindowManager windowManager =
                (WindowManager) getContext().getSystemService(Context.WINDOW_SERVICE);
        switch (windowManager.getDefaultDisplay().getRotation()) {
            case Surface.ROTATION_90:
                nativeSendOrientationChangeEvent(mNativeContentViewCore, 90);
                break;
            case Surface.ROTATION_180:
                nativeSendOrientationChangeEvent(mNativeContentViewCore, 180);
                break;
            case Surface.ROTATION_270:
                nativeSendOrientationChangeEvent(mNativeContentViewCore, -90);
                break;
            case Surface.ROTATION_0:
                nativeSendOrientationChangeEvent(mNativeContentViewCore, 0);
                break;
            default:
                Log.w(TAG, "Unknown rotation!");
                break;
        }
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

    private SelectionHandleController getSelectionHandleController() {
        if (mSelectionHandleController == null) {
            mSelectionHandleController = new SelectionHandleController(getContainerView()) {
                @Override
                public void selectBetweenCoordinates(int x1, int y1, int x2, int y2) {
                    if (mNativeContentViewCore != 0 && !(x1 == x2 && y1 == y2)) {
                        nativeSelectBetweenCoordinates(mNativeContentViewCore, x1, y1, x2, y2);
                    }
                }

                @Override
                public void showHandlesAt(int x1, int y1, int dir1, int x2, int y2, int dir2) {
                    super.showHandlesAt(x1, y1, dir1, x2, y2, dir2);
                    mStartHandleNormalizedPoint.set(
                            (x1 + mNativeScrollX) / mNativePageScaleFactor,
                            (y1 + mNativeScrollY) / mNativePageScaleFactor);
                    mEndHandleNormalizedPoint.set(
                            (x2 + mNativeScrollX) / mNativePageScaleFactor,
                            (y2 + mNativeScrollY) / mNativePageScaleFactor);

                    showSelectActionBar();
                }

            };

            mSelectionHandleController.hideAndDisallowAutomaticShowing();
        }

        return mSelectionHandleController;
    }

    private InsertionHandleController getInsertionHandleController() {
        if (mInsertionHandleController == null) {
            mInsertionHandleController = new InsertionHandleController(getContainerView()) {
                private static final int AVERAGE_LINE_HEIGHT = 14;

                @Override
                public void setCursorPosition(int x, int y) {
                    if (mNativeContentViewCore != 0) {
                        nativeMoveCaret(mNativeContentViewCore, x, y);
                    }
                }

                @Override
                public void paste() {
                    mImeAdapter.paste();
                    hideHandles();
                }

                @Override
                public int getLineHeight() {
                    return (int) (mNativePageScaleFactor * AVERAGE_LINE_HEIGHT);
                }

                @Override
                public void showHandleAt(int x, int y) {
                    super.showHandleAt(x, y);
                    mInsertionHandleNormalizedPoint.set(
                            (x + mNativeScrollX) / mNativePageScaleFactor,
                            (y + mNativeScrollY) / mNativePageScaleFactor);
                }
            };

            mInsertionHandleController.hideAndDisallowAutomaticShowing();
        }

        return mInsertionHandleController;
    }

    public InsertionHandleController getInsertionHandleControllerForTest() {
        return mInsertionHandleController;
    }

    private void updateHandleScreenPositions() {
        if (mSelectionHandleController != null && mSelectionHandleController.isShowing()) {
            float startX = mStartHandleNormalizedPoint.x * mNativePageScaleFactor - mNativeScrollX;
            float startY = mStartHandleNormalizedPoint.y * mNativePageScaleFactor - mNativeScrollY;
            mSelectionHandleController.setStartHandlePosition((int) startX, (int) startY);

            float endX = mEndHandleNormalizedPoint.x * mNativePageScaleFactor - mNativeScrollX;
            float endY = mEndHandleNormalizedPoint.y * mNativePageScaleFactor - mNativeScrollY;
            mSelectionHandleController.setEndHandlePosition((int) endX, (int) endY);
        }

        if (mInsertionHandleController != null && mInsertionHandleController.isShowing()) {
            float x = mInsertionHandleNormalizedPoint.x * mNativePageScaleFactor - mNativeScrollX;
            float y = mInsertionHandleNormalizedPoint.y * mNativePageScaleFactor - mNativeScrollY;
            mInsertionHandleController.setHandlePosition((int) x, (int) y);
        }
    }

    private void hideHandles() {
        if (mSelectionHandleController != null) {
            mSelectionHandleController.hideAndDisallowAutomaticShowing();
        }
        if (mInsertionHandleController != null) {
            mInsertionHandleController.hideAndDisallowAutomaticShowing();
        }
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
            public boolean selectAll() {
                return mImeAdapter.selectAll();
            }

            @Override
            public boolean cut() {
                return mImeAdapter.cut();
            }

            @Override
            public boolean copy() {
                return mImeAdapter.copy();
            }

            @Override
            public boolean paste() {
                return mImeAdapter.paste();
            }

            @Override
            public boolean isSelectionEditable() {
                return mSelectionEditable;
            }

            @Override
            public String getSelectedText() {
                return ContentViewCore.this.getSelectedText();
            }

            @Override
            public void onDestroyActionMode() {
                mActionMode = null;
                mImeAdapter.unselect();
                getContentViewClient().onContextualActionBarHidden();
            }
        };
        mActionMode = mContainerView.startActionMode(
                getContentViewClient().getSelectActionModeCallback(getContext(), actionHandler,
                        nativeIsIncognito(mNativeContentViewCore)));
        if (mActionMode == null) {
            // There is no ActionMode, so remove the selection.
            mImeAdapter.unselect();
        } else {
            getContentViewClient().onContextualActionBarShown();
        }
    }

    public boolean getUseDesktopUserAgent() {
        if (mNativeContentViewCore != 0) {
            return nativeGetUseDesktopUserAgent(mNativeContentViewCore);
        }
        return false;
    }

    /**
     * Set whether or not we're using a desktop user agent for the currently loaded page.
     * @param override If true, use a desktop user agent.  Use a mobile one otherwise.
     * @param reloadOnChange Reload the page if the UA has changed.
     */
    public void setUseDesktopUserAgent(boolean override, boolean reloadOnChange) {
        if (mNativeContentViewCore != 0) {
            nativeSetUseDesktopUserAgent(mNativeContentViewCore, override, reloadOnChange);
        }
    }

    public void clearSslPreferences() {
        nativeClearSslPreferences(mNativeContentViewCore);
    }

    /**
     * @return Whether the native ContentView has crashed.
     */
    public boolean isCrashed() {
        if (mNativeContentViewCore == 0) return false;
        return nativeCrashed(mNativeContentViewCore);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void updateContentSize(int width, int height) {
        // Make sure the content size is at least the View size
        final int newContentWidth = Math.max(width, getWidth());
        final int newContentHeight = Math.max(height, getHeight());
        if (mContentWidth != newContentWidth || mContentHeight != newContentHeight) {
            mPopupZoomer.hide(true);
            mContentWidth = newContentWidth;
            mContentHeight = newContentHeight;

            if (mContentSizeChangeListener != null) {
                mContentSizeChangeListener.onContentSizeChanged(width, height);
            }
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void updateScrollOffsetAndPageScaleFactor(int x, int y, float scale) {
        if (mNativeScrollX == x && mNativeScrollY == y && mNativePageScaleFactor == scale) return;

        mContainerViewInternals.onScrollChanged(x, y, mNativeScrollX, mNativeScrollY);

        // This function should be called back from native as soon
        // as the scroll is applied to the backbuffer.  We should only
        // update mNativeScrollX/Y here for consistency.
        mNativeScrollX = x;
        mNativeScrollY = y;

        if (mNativePageScaleFactor != scale && mContentViewClient != null) {
            mContentViewClient.onScaleChanged(mNativePageScaleFactor, scale);
        }

        mNativePageScaleFactor = scale;

        mPopupZoomer.hide(true);
        updateHandleScreenPositions();

        mZoomManager.updateZoomControls();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void updateOffsetsForFullscreen(float controlsOffsetY, float contentOffsetY) {
        if (mContentViewClient == null) return;
        mContentViewClient.onOffsetsForFullscreenChanged(controlsOffsetY, contentOffsetY);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void updatePageScaleLimits(float minimumScale, float maximumScale) {
        mNativeMinimumScale = minimumScale;
        mNativeMaximumScale = maximumScale;
        mZoomManager.updateZoomControls();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void imeUpdateAdapter(int nativeImeAdapterAndroid, int textInputType,
            String text, int selectionStart, int selectionEnd,
            int compositionStart, int compositionEnd, boolean showImeIfNeeded) {
        TraceEvent.begin();

        // Non-breaking spaces can cause the IME to get confused. Replace with normal spaces.
        text = text.replace('\u00A0', ' ');

        mSelectionEditable = (textInputType != ImeAdapter.sTextInputTypeNone);

        if (mActionMode != null) mActionMode.invalidate();

        mImeAdapter.attachAndShowIfNeeded(nativeImeAdapterAndroid, textInputType,
                text, showImeIfNeeded);

        if (mInputConnection != null) {
            // In WebKit if there's a composition then the selection will usually be the
            // same as the composition, whereas Android IMEs expect the selection to be
            // just a caret at the end of the composition.
            if (selectionStart == compositionStart && selectionEnd == compositionEnd) {
                selectionStart = selectionEnd;
            }
            mInputConnection.setEditableText(text, selectionStart, selectionEnd,
                    compositionStart, compositionEnd);
        }
        TraceEvent.end();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void setTitle(String title) {
        getContentViewClient().onUpdateTitle(title);
    }

    /**
     * Called (from native) when the <select> popup needs to be shown.
     * @param items           Items to show.
     * @param enabled         POPUP_ITEM_TYPEs for items.
     * @param multiple        Whether the popup menu should support multi-select.
     * @param selectedIndices Indices of selected items.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void showSelectPopup(String[] items, int[] enabled, boolean multiple,
            int[] selectedIndices) {
        SelectPopupDialog.show(this, items, enabled, multiple, selectedIndices);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void showDisambiguationPopup(Rect targetRect, Bitmap zoomedBitmap) {
        mPopupZoomer.setBitmap(zoomedBitmap);
        mPopupZoomer.show(targetRect);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onSelectionChanged(String text) {
        mLastSelectedText = text;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onSelectionBoundsChanged(Rect startRect, int dir1, Rect endRect, int dir2) {
        int x1 = startRect.left;
        int y1 = startRect.bottom;
        int x2 = endRect.left;
        int y2 = endRect.bottom;
        if (x1 != x2 || y1 != y2) {
            if (mInsertionHandleController != null) {
                mInsertionHandleController.hide();
            }
            getSelectionHandleController().onSelectionChanged(x1, y1, dir1, x2, y2, dir2);
            mHasSelection = true;
        } else {
            hideSelectActionBar();
            if (x1 != 0 && y1 != 0
                    && (mSelectionHandleController == null
                            || !mSelectionHandleController.isDragging())
                    && mSelectionEditable) {
                // Selection is a caret, and a text field is focused.
                if (mSelectionHandleController != null) {
                    mSelectionHandleController.hide();
                }
                getInsertionHandleController().onCursorPositionChanged(x1, y1);
                InputMethodManager manager = (InputMethodManager)
                        getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                if (manager.isWatchingCursor(mContainerView)) {
                    manager.updateCursor(mContainerView, startRect.left, startRect.top,
                            startRect.right, startRect.bottom);
                }
            } else {
                // Deselection
                if (mSelectionHandleController != null
                        && !mSelectionHandleController.isDragging()) {
                    mSelectionHandleController.hideAndDisallowAutomaticShowing();
                }
                if (mInsertionHandleController != null) {
                    mInsertionHandleController.hideAndDisallowAutomaticShowing();
                }
            }
            mHasSelection = false;
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private static void onEvaluateJavaScriptResult(
            String jsonResult, JavaScriptCallback callback) {
        callback.handleJavaScriptResult(jsonResult);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void showPastePopup(int x, int y) {
        getInsertionHandleController()
                .showHandleWithPastePopupAt(x - mNativeScrollX, y - mNativeScrollY);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onRenderProcessSwap(int oldPid, int newPid) {
        if (mAttachedToWindow && oldPid != newPid) {
            if (oldPid > 0) {
                SandboxedProcessLauncher.unbindAsHighPriority(oldPid);
            }
            if (newPid > 0) {
                SandboxedProcessLauncher.bindAsHighPriority(newPid);
            }
        }
    }

    /**
     * @return Whether a reload happens when this ContentView is activated.
     */
    public boolean needsReload() {
        return mNativeContentViewCore != 0 && nativeNeedsReload(mNativeContentViewCore);
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
        return mNativeMaximumScale - mNativePageScaleFactor > ZOOM_CONTROLS_EPSILON;
    }

    /**
     * Checks whether the ContentViewCore can be zoomed out.
     *
     * @return True if the ContentViewCore can be zoomed out.
     */
    // This method uses the term 'zoom' for legacy reasons, but relates
    // to what chrome calls the 'page scale factor'.
    public boolean canZoomOut() {
        return mNativePageScaleFactor - mNativeMinimumScale > ZOOM_CONTROLS_EPSILON;
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
        return zoomByDelta(1.25f);
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
        return zoomByDelta(0.8f);
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
        if (mNativePageScaleFactor - mNativeMinimumScale < ZOOM_CONTROLS_EPSILON) {
            return false;
        }
        return zoomByDelta(mNativeMinimumScale / mNativePageScaleFactor);
    }

    private boolean zoomByDelta(float delta) {
        if (mNativeContentViewCore == 0) {
            return false;
        }

        long timeMs = System.currentTimeMillis();
        int x = getWidth() / 2;
        int y = getHeight() / 2;

        getContentViewGestureHandler().pinchBegin(timeMs, x, y);
        getContentViewGestureHandler().pinchBy(timeMs, x, y, delta);
        getContentViewGestureHandler().pinchEnd(timeMs);

        return true;
    }

    /**
     * Invokes the graphical zoom picker widget for this ContentView.
     */
    @Override
    public void invokeZoomPicker() {
        if (mContentSettings != null && mContentSettings.supportZoom()) {
            mZoomManager.invokeZoomPicker();
        }
    }

    // Unlike legacy WebView getZoomControls which returns external zoom controls,
    // this method returns built-in zoom controls. This method is used in tests.
    public View getZoomControlsForTest() {
        return mZoomManager.getZoomControlsViewForTest();
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
            mJavaScriptInterfaces.put(name, object);
            nativeAddJavascriptInterface(mNativeContentViewCore, object, name, requiredAnnotation);
        }
    }

    /**
     * Removes a previously added JavaScript interface with the given name.
     *
     * @param name The name of the interface to remove.
     */
    public void removeJavascriptInterface(String name) {
        // TODO(benm): Move the management of this retained object set
        // into the native java bridge. (crbug/169228)
        mRetainedJavaScriptObjects.add(mJavaScriptInterfaces.get(name));
        mJavaScriptInterfaces.remove(name);
        if (mNativeContentViewCore != 0) {
            nativeRemoveJavascriptInterface(mNativeContentViewCore, name);
        }
    }

    /**
     * Return the current scale of the ContentView.
     * @return The current scale.
     */
    public float getScale() {
        return mNativePageScaleFactor;
    }

    /**
     * If the view is ready to draw contents to the screen. In hardware mode,
     * the initialization of the surface texture may not occur until after the
     * view has been added to the layout. This method will return {@code true}
     * once the texture is actually ready.
     */
    public boolean isReady() {
        return nativeIsRenderWidgetHostViewReady(mNativeContentViewCore);
    }

    /**
     * @return Whether or not the texture view is available or not.
     */
    public boolean isAvailable() {
        // TODO(nileshagrawal): Implement this.
        return false;
    }

    @CalledByNative
    private void startContentIntent(String contentUrl) {
        getContentViewClient().onStartContentIntent(getContext(), contentUrl);
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
     * @see View#onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo)
     */
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        mAccessibilityInjector.onInitializeAccessibilityNodeInfo(info);
    }

    /**
     * @see View#onInitializeAccessibilityEvent(AccessibilityEvent)
     */
    public void onInitializeAccessibilityEvent(AccessibilityEvent event) {
        event.setClassName(this.getClass().getName());

        // Identify where the top-left of the screen currently points to.
        event.setScrollX(mNativeScrollX);
        event.setScrollY(mNativeScrollY);

        // The maximum scroll values are determined by taking the content dimensions and
        // subtracting off the actual dimensions of the ChromeView.
        int maxScrollX = Math.max(0, mContentWidth - getWidth());
        int maxScrollY = Math.max(0, mContentHeight - getHeight());
        event.setScrollable(maxScrollX > 0 || maxScrollY > 0);

        // Setting the maximum scroll values requires API level 15 or higher.
        final int SDK_VERSION_REQUIRED_TO_SET_SCROLL = 15;
        if (Build.VERSION.SDK_INT >= SDK_VERSION_REQUIRED_TO_SET_SCROLL) {
            event.setMaxScrollX(maxScrollX);
            event.setMaxScrollY(maxScrollY);
        }
    }

    /**
     * Returns whether or not accessibility injection is being used.
     */
    public boolean isInjectingAccessibilityScript() {
        return mAccessibilityInjector.accessibilityIsAvailable();
    }

    /**
     * Enable or disable accessibility features.
     */
    public void setAccessibilityState(boolean state) {
        mAccessibilityInjector.setScriptEnabled(state);
    }

    /**
     * Stop any TTS notifications that are currently going on.
     */
    public void stopCurrentAccessibilityNotifications() {
        mAccessibilityInjector.onPageLostFocus();
    }

    /**
     * @See android.webkit.WebView#pageDown(boolean)
     */
    public boolean pageDown(boolean bottom) {
        if (computeVerticalScrollOffset() >= mContentHeight - getHeight()) {
            // We seem to already be at the bottom of the page, so no scrolling will occur.
            return false;
        }

        if (bottom) {
            scrollTo(computeHorizontalScrollOffset(), mContentHeight - getHeight());
        } else {
            dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PAGE_DOWN));
            dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_PAGE_DOWN));
        }
        return true;
    }

    /**
     * @See android.webkit.WebView#pageUp(boolean)
     */
    public boolean pageUp(boolean top) {
        if (computeVerticalScrollOffset() == 0) {
            // We seem to already be at the top of the page, so no scrolling will occur.
            return false;
        }

        if (top) {
            scrollTo(computeHorizontalScrollOffset(), 0);
        } else {
            dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PAGE_UP));
            dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_PAGE_UP));
        }
        return true;
    }

    /**
     * Callback factory method for nativeGetNavigationHistory().
     */
    @CalledByNative
    private void addToNavigationHistory(Object history, int index, String url, String virtualUrl,
            String originalUrl, String title, Bitmap favicon) {
        NavigationEntry entry = new NavigationEntry(
                index, url, virtualUrl, originalUrl, title, favicon);
        ((NavigationHistory) history).addEntry(entry);
    }

    /**
     * Get a copy of the navigation history of the view.
     */
    public NavigationHistory getNavigationHistory() {
        NavigationHistory history = new NavigationHistory();
        int currentIndex = nativeGetNavigationHistory(mNativeContentViewCore, history);
        history.setCurrentEntryIndex(currentIndex);
        return history;
    }

    @Override
    public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
        NavigationHistory history = new NavigationHistory();
        nativeGetDirectedNavigationHistory(mNativeContentViewCore, history, isForward, itemLimit);
        return history;
    }

    /**
     * Update of the latest vsync parameters.
     * @param tickTimeMicros The latest vsync tick time in microseconds.
     * @param intervalMicros The vsync interval in microseconds.
     */
    public void updateVSync(long tickTimeMicros, long intervalMicros) {
        if (mNativeContentViewCore != 0) {
            nativeUpdateVSyncParameters(mNativeContentViewCore, tickTimeMicros, intervalMicros);
        }
    }

    private native int nativeInit(boolean hardwareAccelerated, boolean inputEventsDeliveredAtVSync,
            int webContentsPtr, int windowAndroidPtr);

    private native void nativeOnJavaContentViewCoreDestroyed(int nativeContentViewCoreImpl);

    private native void nativeLoadUrl(
            int nativeContentViewCoreImpl,
            String url,
            int loadUrlType,
            int transitionType,
            int uaOverrideOption,
            String extraHeaders,
            byte[] postData,
            String baseUrlForDataUrl,
            String virtualUrlForDataUrl,
            boolean canLoadLocalResources);

    private native void nativeSetAllUserAgentOverridesInHistory(int nativeContentViewCoreImpl,
            String userAgentOverride);

    private native String nativeGetURL(int nativeContentViewCoreImpl);

    private native String nativeGetTitle(int nativeContentViewCoreImpl);

    private native void nativeShowInterstitialPage(
            int nativeContentViewCoreImpl, String url, int nativeInterstitialPageDelegateAndroid);
    private native boolean nativeIsShowingInterstitialPage(int nativeContentViewCoreImpl);

    private native boolean nativeConsumePendingRendererFrame(int nativeContentViewCoreImpl);

    private native boolean nativeIsIncognito(int nativeContentViewCoreImpl);

    // Returns true if the native side crashed so that java side can draw a sad tab.
    private native boolean nativeCrashed(int nativeContentViewCoreImpl);

    private native void nativeSetFocus(int nativeContentViewCoreImpl, boolean focused);

    private native void nativeSendOrientationChangeEvent(
            int nativeContentViewCoreImpl, int orientation);

    private native boolean nativeSendTouchEvent(
            int nativeContentViewCoreImpl, long timeMs, int action, TouchPoint[] pts);

    private native int nativeSendMouseMoveEvent(
            int nativeContentViewCoreImpl, long timeMs, int x, int y);

    private native int nativeSendMouseWheelEvent(
            int nativeContentViewCoreImpl, long timeMs, int x, int y, float verticalAxis);

    private native void nativeScrollBegin(int nativeContentViewCoreImpl, long timeMs, int x, int y);

    private native void nativeScrollEnd(int nativeContentViewCoreImpl, long timeMs);

    private native void nativeScrollBy(
            int nativeContentViewCoreImpl, long timeMs, int x, int y, int deltaX, int deltaY);

    private native void nativeFlingStart(
            int nativeContentViewCoreImpl, long timeMs, int x, int y, int vx, int vy);

    private native void nativeFlingCancel(int nativeContentViewCoreImpl, long timeMs);

    private native void nativeSingleTap(
            int nativeContentViewCoreImpl, long timeMs, int x, int y, boolean linkPreviewTap);

    private native void nativeShowPressState(
            int nativeContentViewCoreImpl, long timeMs, int x, int y);

    private native void nativeShowPressCancel(
            int nativeContentViewCoreImpl, long timeMs, int x, int y);

    private native void nativeDoubleTap(int nativeContentViewCoreImpl, long timeMs, int x, int y);

    private native void nativeLongPress(int nativeContentViewCoreImpl, long timeMs, int x, int y,
            boolean linkPreviewTap);

    private native void nativeLongTap(int nativeContentViewCoreImpl, long timeMs, int x, int y,
            boolean linkPreviewTap);

    private native void nativePinchBegin(int nativeContentViewCoreImpl, long timeMs, int x, int y);

    private native void nativePinchEnd(int nativeContentViewCoreImpl, long timeMs);

    private native void nativePinchBy(int nativeContentViewCoreImpl, long timeMs,
            int anchorX, int anchorY, float deltaScale);

    private native void nativeSelectBetweenCoordinates(
            int nativeContentViewCoreImpl, int x1, int y1, int x2, int y2);

    private native void nativeMoveCaret(int nativeContentViewCoreImpl, int x, int y);

    private native boolean nativeCanGoBack(int nativeContentViewCoreImpl);
    private native boolean nativeCanGoForward(int nativeContentViewCoreImpl);
    private native boolean nativeCanGoToOffset(int nativeContentViewCoreImpl, int offset);
    private native void nativeGoBack(int nativeContentViewCoreImpl);
    private native void nativeGoForward(int nativeContentViewCoreImpl);
    private native void nativeGoToOffset(int nativeContentViewCoreImpl, int offset);
    private native void nativeGoToNavigationIndex(int nativeContentViewCoreImpl, int index);

    private native void nativeStopLoading(int nativeContentViewCoreImpl);

    private native void nativeReload(int nativeContentViewCoreImpl);

    private native void nativeCancelPendingReload(int nativeContentViewCoreImpl);

    private native void nativeContinuePendingReload(int nativeContentViewCoreImpl);

    private native void nativeSelectPopupMenuItems(int nativeContentViewCoreImpl, int[] indices);

    private native void nativeScrollFocusedEditableNodeIntoView(int nativeContentViewCoreImpl);
    private native void nativeUndoScrollFocusedEditableNodeIntoView(int nativeContentViewCoreImpl);
    private native boolean nativeNeedsReload(int nativeContentViewCoreImpl);

    private native void nativeClearHistory(int nativeContentViewCoreImpl);

    private native void nativeEvaluateJavaScript(int nativeContentViewCoreImpl,
            String script, JavaScriptCallback callback);

    private native int nativeGetNativeImeAdapter(int nativeContentViewCoreImpl);

    private native int nativeGetCurrentRenderProcessId(int nativeContentViewCoreImpl);

    private native int nativeGetBackgroundColor(int nativeContentViewCoreImpl);

    private native void nativeSetBackgroundColor(int nativeContentViewCoreImpl, int color);

    private native void nativeOnShow(int nativeContentViewCoreImpl);
    private native void nativeOnHide(int nativeContentViewCoreImpl);

    private native void nativeSetUseDesktopUserAgent(int nativeContentViewCoreImpl,
            boolean enabled, boolean reloadOnChange);
    private native boolean nativeGetUseDesktopUserAgent(int nativeContentViewCoreImpl);

    private native void nativeClearSslPreferences(int nativeContentViewCoreImpl);

    private native void nativeAddJavascriptInterface(int nativeContentViewCoreImpl, Object object,
            String name, Class requiredAnnotation);

    private native void nativeRemoveJavascriptInterface(int nativeContentViewCoreImpl, String name);

    private native int nativeGetNavigationHistory(int nativeContentViewCoreImpl, Object context);
    private native void nativeGetDirectedNavigationHistory(int nativeContentViewCoreImpl,
            Object context, boolean isForward, int maxEntries);

    private native void nativeUpdateVSyncParameters(int nativeContentViewCoreImpl,
            long timebaseMicros, long intervalMicros);

    private native boolean nativePopulateBitmapFromCompositor(int nativeContentViewCoreImpl,
            Bitmap bitmap);

    private native void nativeSetSize(int nativeContentViewCoreImpl, int width, int height);

    private native boolean nativeIsRenderWidgetHostViewReady(int nativeContentViewCoreImpl);
}
