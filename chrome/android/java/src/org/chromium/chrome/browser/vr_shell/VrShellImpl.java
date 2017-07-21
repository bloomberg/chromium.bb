// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.os.StrictMode;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewTreeObserver.OnPreDrawListener;
import android.widget.FrameLayout;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.GvrLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.page_info.PageInfoPopup;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.MotionEventSynthesizer;
import org.chromium.content.browser.WindowAndroidChangedObserver;
import org.chromium.content.browser.WindowAndroidProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.VirtualDisplayAndroid;

/**
 * This view extends from GvrLayout which wraps a GLSurfaceView that renders VR shell.
 */
@JNINamespace("vr_shell")
public class VrShellImpl
        extends GvrLayout implements VrShell, SurfaceHolder.Callback, WindowAndroidProvider {
    private static final String TAG = "VrShellImpl";

    // TODO(mthiesse): These values work well for Pixel/Pixel XL in VR, but we need to come up with
    // a way to compute good values for any screen size/scaling ratio.

    // Increasing DPR any more than this doesn't appear to increase text quality.
    private static final float DEFAULT_DPR = 1.7f;
    // Fairly arbitrary values that put a good amount of content on the screen without making the
    // text too small to read.
    @VisibleForTesting
    public static final float DEFAULT_CONTENT_WIDTH = 645f;
    @VisibleForTesting
    public static final float DEFAULT_CONTENT_HEIGHT = 430f;

    // Make full screen 16:9 until we get exact dimensions from playing video.
    private static final float FULLSCREEN_DPR = 1.4f;
    private static final float FULLSCREEN_CONTENT_WIDTH = 1024f;
    private static final float FULLSCREEN_CONTENT_HEIGHT = 576f;

    private final ChromeActivity mActivity;
    private final VrShellDelegate mDelegate;
    private final VirtualDisplayAndroid mContentVirtualDisplay;
    private final InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    private final TabRedirectHandler mTabRedirectHandler;
    private final TabObserver mTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final View.OnTouchListener mTouchListener;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private OnPreDrawListener mPredrawListener;

    private long mNativeVrShell;

    private FrameLayout mRenderToSurfaceLayoutParent;
    private FrameLayout mRenderToSurfaceLayout;
    private Surface mSurface;
    private View mPresentationView;

    // The tab that holds the main ContentViewCore.
    private Tab mTab;
    private ContentViewCore mContentViewCore;
    private NativePage mNativePage;
    private Boolean mCanGoBack;
    private Boolean mCanGoForward;

    private VrWindowAndroid mContentVrWindowAndroid;

    private boolean mReprojectedRendering;

    private InterceptNavigationDelegateImpl mNonVrInterceptNavigationDelegate;
    private TabRedirectHandler mNonVrTabRedirectHandler;
    private TabModelSelector mTabModelSelector;
    private float mLastContentWidth;
    private float mLastContentHeight;
    private float mLastContentDpr;
    private Boolean mPaused;

    private MotionEventSynthesizer mMotionEventSynthesizer;

    private OnDispatchTouchEventCallback mOnDispatchTouchEventForTesting;

    public VrShellImpl(
            ChromeActivity activity, VrShellDelegate delegate, TabModelSelector tabModelSelector) {
        super(activity);
        mActivity = activity;
        mDelegate = delegate;
        mTabModelSelector = tabModelSelector;

        // This overrides the default intent created by GVR to return to Chrome when the DON flow
        // is triggered by resuming the GvrLayout, which is the usual way Daydream apps enter VR.
        // See VrShellDelegate#getEnterVrPendingIntent for why we need to do this.
        setReentryIntent(VrShellDelegate.getEnterVrPendingIntent(activity));

        mReprojectedRendering = setAsyncReprojectionEnabled(true);
        if (mReprojectedRendering) {
            // No need render to a Surface if we're reprojected. We'll be rendering with surfaceless
            // EGL.
            mPresentationView = new FrameLayout(mActivity);

            // Only enable sustained performance mode when Async reprojection decouples the app
            // framerate from the display framerate.
            AndroidCompat.setSustainedPerformanceMode(mActivity, true);
        } else {
            SurfaceView surfaceView = new SurfaceView(mActivity);
            surfaceView.getHolder().addCallback(this);
            mPresentationView = surfaceView;
        }

        setPresentationView(mPresentationView);

        getUiLayout().setCloseButtonListener(new Runnable() {
            @Override
            public void run() {
                mDelegate.shutdownVr(true /* disableVrMode */, false /* canReenter */,
                        true /* stayingInChrome */);
            }
        });

        DisplayAndroid primaryDisplay = DisplayAndroid.getNonMultiDisplay(activity);
        mContentVirtualDisplay = VirtualDisplayAndroid.createVirtualDisplay();
        mContentVirtualDisplay.setTo(primaryDisplay);

        mInterceptNavigationDelegate = new InterceptNavigationDelegateImpl(
                new VrExternalNavigationDelegate(mActivity.getActivityTab()),
                mActivity.getActivityTab());

        mTabRedirectHandler = new TabRedirectHandler(mActivity) {
            @Override
            public boolean shouldStayInChrome(boolean hasExternalProtocol) {
                return true;
            }
        };

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onContentChanged(Tab tab) {
                // Restore proper focus on the old CVC.
                if (mContentViewCore != null) mContentViewCore.onWindowFocusChanged(false);
                mContentViewCore = null;
                if (mNativeVrShell == 0) return;
                if (tab.isShowingSadTab()) {
                    // For now we don't support the sad tab page. crbug.com/661609.
                    forceExitVr();
                    return;
                }
                if (mNativePage != null) {
                    UiUtils.removeViewFromParent(mNativePage.getView());
                    mNativePage = null;
                    mMotionEventSynthesizer = null;
                    if (tab.getNativePage() == null) {
                        nativeRestoreContentSurface(mNativeVrShell);
                        mRenderToSurfaceLayoutParent.setVisibility(View.INVISIBLE);
                        mSurface = null;
                    }
                }
                if (tab.getNativePage() != null) {
                    mRenderToSurfaceLayoutParent.setVisibility(View.VISIBLE);
                    mNativePage = tab.getNativePage();
                    if (mSurface == null) mSurface = nativeTakeContentSurface(mNativeVrShell);
                    mRenderToSurfaceLayout.addView(mNativePage.getView(),
                            new FrameLayout.LayoutParams(
                                    LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
                    mNativePage.getView().invalidate();
                    mMotionEventSynthesizer =
                            new MotionEventSynthesizer(mRenderToSurfaceLayout, VrShellImpl.this);
                }
                setContentCssSize(mLastContentWidth, mLastContentHeight, mLastContentDpr);
                if (tab.getNativePage() == null && tab.getContentViewCore() != null) {
                    mContentViewCore = tab.getContentViewCore();
                    mContentViewCore.onAttachedToWindow();
                    mContentViewCore.getContainerView().requestFocus();
                    // We need the CVC to think it has Window Focus so it doesn't blur the page,
                    // even though we're drawing VR layouts over top of it.
                    mContentViewCore.onWindowFocusChanged(true);
                    nativeSwapContents(mNativeVrShell, mContentViewCore.getWebContents(), null);
                } else {
                    nativeSwapContents(mNativeVrShell, null, mMotionEventSynthesizer);
                }
                updateHistoryButtonsVisibility();
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                onContentChanged(tab);
            }

            @Override
            public void onLoadProgressChanged(Tab tab, int progress) {
                if (mNativeVrShell == 0) return;
                nativeOnLoadProgressChanged(mNativeVrShell, progress / 100.0);
            }

            @Override
            public void onCrash(Tab tab, boolean sadTabShown) {
                updateHistoryButtonsVisibility();
            }

            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                updateHistoryButtonsVisibility();
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                updateHistoryButtonsVisibility();
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                updateHistoryButtonsVisibility();
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onChange() {
                swapToForegroundTab();
            }

            @Override
            public void onNewTabCreated(Tab tab) {
                if (mNativeVrShell == 0) return;
                nativeOnTabUpdated(mNativeVrShell, tab.isIncognito(), tab.getId(), tab.getTitle());
            }
        };

        mTouchListener = new View.OnTouchListener() {
            @Override
            @SuppressLint("ClickableViewAccessibility")
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    nativeOnTriggerEvent(mNativeVrShell, true);
                    return true;
                } else if (event.getActionMasked() == MotionEvent.ACTION_UP
                        || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                    nativeOnTriggerEvent(mNativeVrShell, false);
                    return true;
                }
                return false;
            }
        };
        // We need a parent for the RenderToSurfaceLayout because we want screen taps to only be
        // routed to the GvrUiLayout, and not propagate through to the NativePage. So screen taps
        // fall through the RenderToSurfaceLayoutParent, onto the GvrUiLayout, while touch events
        // generated from the VR controller are injected directly into the RenderToSurfaceLayout,
        // bypassing the parent.
        mRenderToSurfaceLayoutParent = new FrameLayout(mActivity) {
            @Override
            public boolean dispatchTouchEvent(MotionEvent event) {
                return false;
            }
        };
        mRenderToSurfaceLayoutParent.setVisibility(View.INVISIBLE);
        mRenderToSurfaceLayout = new FrameLayout(mActivity) {
            @Override
            protected void dispatchDraw(Canvas canvas) {
                if (mSurface == null) return;
                // TODO(mthiesse): Support mSurface.lockHardwareCanvas(); crbug.com/692775
                final Canvas surfaceCanvas = mSurface.lockCanvas(null);
                super.dispatchDraw(surfaceCanvas);
                mSurface.unlockCanvasAndPost(surfaceCanvas);
            }
        };
        mRenderToSurfaceLayout.setVisibility(View.VISIBLE);
        // We need a pre-draw listener to invalidate the native page because scrolling usually
        // doesn't trigger an onDraw call, so our texture won't get updated.
        mPredrawListener = new OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                if (mRenderToSurfaceLayout.isDirty()) {
                    mRenderToSurfaceLayout.invalidate();
                    if (mNativePage != null) mNativePage.getView().invalidate();
                }
                return true;
            }
        };
        mRenderToSurfaceLayout.getViewTreeObserver().addOnPreDrawListener(mPredrawListener);
        mRenderToSurfaceLayoutParent.addView(mRenderToSurfaceLayout);
        addView(mRenderToSurfaceLayoutParent);
    }

    private void setSplashScreenIcon() {
        new AsyncTask<Void, Void, Bitmap>() {
            @Override
            protected Bitmap doInBackground(Void... params) {
                Drawable drawable = ApiCompatibilityUtils.getDrawable(
                        mActivity.getResources(), R.mipmap.app_icon);
                if (drawable instanceof BitmapDrawable) {
                    BitmapDrawable bd = (BitmapDrawable) drawable;
                    return bd.getBitmap();
                }
                assert false : "The drawable was not a bitmap drawable as expected";
                return null;
            }
            @Override
            protected void onPostExecute(Bitmap bitmap) {
                if (mNativeVrShell == 0) return;
                nativeSetSplashScreenIcon(mNativeVrShell, bitmap);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public void initializeNative(Tab currentTab, boolean forWebVr,
            boolean webVrAutopresentationExpected, boolean inCct) {
        mContentVrWindowAndroid = new VrWindowAndroid(mActivity, mContentVirtualDisplay);
        mNativeVrShell = nativeInit(mDelegate, mContentVrWindowAndroid.getNativePointer(), forWebVr,
                webVrAutopresentationExpected, inCct, getGvrApi().getNativeGvrContext(),
                mReprojectedRendering);

        // We need to set the icon bitmap from here because we can't read the app icon from native
        // code.
        setSplashScreenIcon();

        // Set the UI and content sizes before we load the UI.
        setContentCssSize(DEFAULT_CONTENT_WIDTH, DEFAULT_CONTENT_HEIGHT, DEFAULT_DPR);

        reparentAllTabs(mContentVrWindowAndroid);
        swapToForegroundTab();
        createTabList();
        mActivity.getTabModelSelector().addObserver(mTabModelSelectorObserver);
        createTabModelSelectorTabObserver();
        updateHistoryButtonsVisibility();

        mPresentationView.setOnTouchListener(mTouchListener);
    }

    private void createTabList() {
        assert mNativeVrShell != 0;
        TabModel main = mTabModelSelector.getModel(false);
        int count = main.getCount();
        Tab[] mainTabs = new Tab[count];
        for (int i = 0; i < count; ++i) {
            mainTabs[i] = main.getTabAt(i);
        }
        TabModel incognito = mTabModelSelector.getModel(true);
        count = incognito.getCount();
        Tab[] incognitoTabs = new Tab[count];
        for (int i = 0; i < count; ++i) {
            incognitoTabs[i] = incognito.getTabAt(i);
        }
        nativeOnTabListCreated(mNativeVrShell, mainTabs, incognitoTabs);
    }

    private void swapToForegroundTab() {
        Tab tab = mActivity.getActivityTab();
        if (tab == mTab) return;
        if (!mDelegate.canEnterVr(tab, false)) {
            forceExitVr();
            return;
        }
        if (mTab != null) {
            mTab.removeObserver(mTabObserver);
            restoreTabFromVR();
            mTab.updateFullscreenEnabledState();
        }

        mTab = tab;
        initializeTabForVR();
        mTab.addObserver(mTabObserver);
        mTab.updateFullscreenEnabledState();
        mTabObserver.onContentChanged(mTab);
    }

    private void initializeTabForVR() {
        mNonVrInterceptNavigationDelegate = mTab.getInterceptNavigationDelegate();
        mTab.setInterceptNavigationDelegate(mInterceptNavigationDelegate);
        // Make sure we are not redirecting to another app, i.e. out of VR mode.
        mNonVrTabRedirectHandler = mTab.getTabRedirectHandler();
        mTab.setTabRedirectHandler(mTabRedirectHandler);
        assert mTab.getWindowAndroid() == mContentVrWindowAndroid;
    }

    private void restoreTabFromVR() {
        mTab.setInterceptNavigationDelegate(mNonVrInterceptNavigationDelegate);
        mTab.setTabRedirectHandler(mNonVrTabRedirectHandler);
        mNonVrTabRedirectHandler = null;
    }

    private void reparentAllTabs(WindowAndroid window) {
        // Ensure new tabs are created with the correct window.
        boolean[] values = {true, false};
        for (boolean incognito : values) {
            TabCreator tabCreator = mActivity.getTabCreator(incognito);
            if (tabCreator instanceof ChromeTabCreator) {
                ((ChromeTabCreator) tabCreator).setWindowAndroid(window);
            }
        }

        // Reparent all existing tabs.
        for (TabModel model : mActivity.getTabModelSelector().getModels()) {
            for (int i = 0; i < model.getCount(); ++i) {
                model.getTabAt(i).updateWindowAndroid(window);
            }
        }
    }

    // Exits VR, telling the user to remove their headset, and returning to Chromium.
    @CalledByNative
    public void forceExitVr() {
        VrShellDelegate.showDoffAndExitVr(false);
    }

    // Called because showing PageInfo isn't supported in VR. This happens when the user clicks on
    // the security icon in the URL bar.
    @CalledByNative
    public void onUnhandledPageInfo() {
        mDelegate.requestToExitVr(new OnExitVrRequestListener() {
            @Override
            public void onSucceeded() {
                PageInfoPopup.show(
                        mActivity, mActivity.getActivityTab(), null, PageInfoPopup.OPENED_FROM_VR);
            }

            @Override
            public void onDenied() {}
        }, UiUnsupportedMode.UNHANDLED_PAGE_INFO);
    }

    // Exits CCT, returning to the app that opened it.
    @CalledByNative
    public void exitCct() {
        mDelegate.exitCct();
    }

    @CalledByNative
    public void setContentCssSize(float width, float height, float dpr) {
        ThreadUtils.assertOnUiThread();
        mLastContentWidth = width;
        mLastContentHeight = height;
        mLastContentDpr = dpr;

        if (mNativePage != null) {
            // Native pages don't listen to our DPR changes, so to get them to render at the correct
            // size we need to make them larger.
            DisplayAndroid primaryDisplay = DisplayAndroid.getNonMultiDisplay(mActivity);
            float dip = primaryDisplay.getDipScale();
            width *= (dip / dpr);
            height *= (dip / dpr);
        }

        int surfaceWidth = (int) Math.ceil(width * dpr);
        int surfaceHeight = (int) Math.ceil(height * dpr);

        Point size = new Point(surfaceWidth, surfaceHeight);
        mContentVirtualDisplay.update(size, dpr, null, null, null, null, null);
        if (mTab != null && mTab.getContentViewCore() != null) {
            mTab.getContentViewCore().onSizeChanged(surfaceWidth, surfaceHeight, 0, 0);
            nativeOnPhysicalBackingSizeChanged(mNativeVrShell,
                    mTab.getContentViewCore().getWebContents(), surfaceWidth, surfaceHeight);
        }
        mRenderToSurfaceLayout.setLayoutParams(
                new FrameLayout.LayoutParams(surfaceWidth, surfaceHeight));
        nativeContentPhysicalBoundsChanged(mNativeVrShell, surfaceWidth, surfaceHeight, dpr);
    }

    @CalledByNative
    public void onFullscreenChanged(boolean enabled) {
        if (enabled) {
            setContentCssSize(FULLSCREEN_CONTENT_WIDTH, FULLSCREEN_CONTENT_HEIGHT, FULLSCREEN_DPR);
        } else {
            setContentCssSize(DEFAULT_CONTENT_WIDTH, DEFAULT_CONTENT_HEIGHT, DEFAULT_DPR);
        }
    }

    @CalledByNative
    public void contentSurfaceChanged() {
        if (mSurface != null || mNativePage == null) return;
        mSurface = nativeTakeContentSurface(mNativeVrShell);
        mNativePage.getView().invalidate();
        mRenderToSurfaceLayout.invalidate();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        boolean parentConsumed = super.dispatchTouchEvent(event);
        if (mOnDispatchTouchEventForTesting != null) {
            mOnDispatchTouchEventForTesting.onDispatchTouchEvent(parentConsumed);
        }
        return parentConsumed;
    }

    @Override
    public void onResume() {
        if (mPaused != null && !mPaused) return;
        mPaused = false;
        super.onResume();
        if (mNativeVrShell != 0) {
            // Refreshing the viewer profile may accesses disk under some circumstances outside of
            // our control.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            try {
                nativeOnResume(mNativeVrShell);
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
    }

    @Override
    public void onPause() {
        if (mPaused != null && mPaused) return;
        mPaused = true;
        super.onPause();
        if (mNativeVrShell != 0) {
            nativeOnPause(mNativeVrShell);
        }
    }

    @Override
    public void onBeforeWindowDetached() {
        mRenderToSurfaceLayout.getViewTreeObserver().removeOnPreDrawListener(mPredrawListener);
    }

    @Override
    public void shutdown() {
        reparentAllTabs(mActivity.getWindowAndroid());
        if (mNativeVrShell != 0) {
            nativeDestroy(mNativeVrShell);
            mNativeVrShell = 0;
        }
        if (mNativePage != null) UiUtils.removeViewFromParent(mNativePage.getView());
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mTabModelSelectorTabObserver.destroy();
        mTab.removeObserver(mTabObserver);
        restoreTabFromVR();

        if (mTab != null) {
            mTab.updateBrowserControlsState(BrowserControlsState.SHOWN, true);
        }

        mContentVirtualDisplay.destroy();
        super.shutdown();
    }

    @Override
    public void pause() {
        onPause();
    }

    @Override
    public void resume() {
        onResume();
    }

    @Override
    public void teardown() {
        shutdown();
    }

    @Override
    public void setWebVrModeEnabled(boolean enabled, boolean showToast) {
        mContentVrWindowAndroid.setVSyncPaused(enabled);
        nativeSetWebVrMode(mNativeVrShell, enabled, showToast);
    }

    @Override
    public boolean getWebVrModeEnabled() {
        return nativeGetWebVrMode(mNativeVrShell);
    }

    @Override
    public boolean isDisplayingUrlForTesting() {
        return nativeIsDisplayingUrlForTesting(mNativeVrShell);
    }

    @Override
    public FrameLayout getContainer() {
        return this;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        nativeSetSurface(mNativeVrShell, holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // No need to do anything here, we don't care about surface width/height.
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        // TODO(mthiesse): For now we don't need to handle this because we exit VR on activity pause
        // (which destroys the surface). If in the future we don't destroy VR Shell on exiting,
        // we will need to handle this, or at least properly handle surfaceCreated being called
        // multiple times.
    }

    private void createTabModelSelectorTabObserver() {
        assert mTabModelSelectorTabObserver == null;
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onTitleUpdated(Tab tab) {
                if (mNativeVrShell == 0) return;
                nativeOnTabUpdated(mNativeVrShell, tab.isIncognito(), tab.getId(), tab.getTitle());
            }

            @Override
            public void onClosingStateChanged(Tab tab, boolean closing) {
                if (mNativeVrShell == 0) return;
                if (closing) {
                    nativeOnTabRemoved(mNativeVrShell, tab.isIncognito(), tab.getId());
                } else {
                    nativeOnTabUpdated(mNativeVrShell, tab.isIncognito(), tab.getId(),
                            tab.getTitle());
                }
            }

            @Override
            public void onDestroyed(Tab tab) {
                if (mNativeVrShell == 0) return;
                nativeOnTabRemoved(mNativeVrShell, tab.isIncognito(), tab.getId());
            }
        };
    }

    @CalledByNative
    public boolean hasDaydreamSupport() {
        return mDelegate.hasDaydreamSupport();
    }

    @Override
    public void requestToExitVr(@UiUnsupportedMode int reason) {
        nativeRequestToExitVr(mNativeVrShell, reason);
    }

    @CalledByNative
    private void onExitVrRequestResult(@UiUnsupportedMode int reason, boolean shouldExit) {
        if (shouldExit) {
            nativeLogUnsupportedModeUserMetric(mNativeVrShell, reason);
        }
        mDelegate.onExitVrRequestResult(shouldExit);
    }

    @CalledByNative
    private void showTab(int id) {
        Tab tab = mActivity.getTabModelSelector().getTabById(id);
        if (tab == null) {
            return;
        }
        int index = mActivity.getTabModelSelector().getModel(tab.isIncognito()).indexOf(tab);
        if (index == TabModel.INVALID_TAB_INDEX) {
            return;
        }
        TabModelUtils.setIndex(mActivity.getTabModelSelector().getModel(tab.isIncognito()), index);
    }

    @CalledByNative
    private void openNewTab(boolean incognito) {
        mActivity.getTabCreator(incognito).launchUrl(
                UrlConstants.NTP_URL, TabLaunchType.FROM_CHROME_UI);
    }

    @CalledByNative
    public void navigateForward() {
        if (!mCanGoForward) return;
        mActivity.getToolbarManager().forward();
        updateHistoryButtonsVisibility();
    }

    @CalledByNative
    public void navigateBack() {
        if (!mCanGoBack) return;
        if (mActivity instanceof ChromeTabbedActivity) {
            // TODO(mthiesse): We should do this for custom tabs as well, as back for custom tabs
            // is also expected to close tabs.
            ((ChromeTabbedActivity) mActivity).handleBackPressed();
        } else {
            mActivity.getToolbarManager().back();
        }
        updateHistoryButtonsVisibility();
    }

    private void updateHistoryButtonsVisibility() {
        if (mTab == null) {
            nativeSetHistoryButtonsEnabled(mNativeVrShell, false, false);
            return;
        }
        boolean willCloseTab = false;
        if (mActivity instanceof ChromeTabbedActivity) {
            // If hitting back would minimize Chrome, disable the back button.
            // See ChromeTabbedActivity#handleBackPressed().
            willCloseTab = ChromeTabbedActivity.backShouldCloseTab(mTab)
                    && !mTab.isCreatedForExternalApp();
        }
        boolean canGoBack = mTab.canGoBack() || willCloseTab;
        boolean canGoForward = mTab.canGoForward();
        if ((mCanGoBack != null && canGoBack == mCanGoBack)
                && (mCanGoForward != null && canGoForward == mCanGoForward)) {
            return;
        }
        mCanGoBack = canGoBack;
        mCanGoForward = canGoForward;
        nativeSetHistoryButtonsEnabled(mNativeVrShell, mCanGoBack, mCanGoForward);
    }

    @CalledByNative
    public void reload() {
        mTab.reload();
    }

    @CalledByNative
    public float getNativePageScrollRatio() {
        return mActivity.getWindowAndroid().getDisplay().getDipScale()
                / mContentVrWindowAndroid.getDisplay().getDipScale();
    }

    @Override
    public WindowAndroid getWindowAndroid() {
        return mContentVrWindowAndroid;
    }

    @Override
    public void addWindowAndroidChangedObserver(WindowAndroidChangedObserver observer) {}

    @Override
    public void removeWindowAndroidChangedObserver(WindowAndroidChangedObserver observer) {}

    /**
     * Sets the callback that will be run when VrShellImpl's dispatchTouchEvent
     * is run and the parent consumed the event.
     * @param callback The Callback to be run.
     */
    @VisibleForTesting
    public void setOnDispatchTouchEventForTesting(OnDispatchTouchEventCallback callback) {
        mOnDispatchTouchEventForTesting = callback;
    }

    @VisibleForTesting
    @Override
    public Boolean isBackButtonEnabled() {
        return mCanGoBack;
    }

    private native long nativeInit(VrShellDelegate delegate, long nativeWindowAndroid,
            boolean forWebVR, boolean webVrAutopresentationExpected, boolean inCct, long gvrApi,
            boolean reprojectedRendering);
    private native void nativeSetSurface(long nativeVrShell, Surface surface);
    private native void nativeSetSplashScreenIcon(long nativeVrShell, Bitmap bitmap);
    private native void nativeSwapContents(
            long nativeVrShell, WebContents webContents, MotionEventSynthesizer eventSynthesizer);
    private native void nativeDestroy(long nativeVrShell);
    private native void nativeOnTriggerEvent(long nativeVrShell, boolean touched);
    private native void nativeOnPause(long nativeVrShell);
    private native void nativeOnResume(long nativeVrShell);
    private native void nativeOnLoadProgressChanged(long nativeVrShell, double progress);
    private native void nativeOnPhysicalBackingSizeChanged(
            long nativeVrShell, WebContents webContents, int width, int height);
    private native void nativeContentPhysicalBoundsChanged(long nativeVrShell, int width,
            int height, float dpr);
    private native void nativeSetWebVrMode(long nativeVrShell, boolean enabled, boolean showToast);
    private native boolean nativeGetWebVrMode(long nativeVrShell);
    private native boolean nativeIsDisplayingUrlForTesting(long nativeVrShell);
    private native void nativeOnTabListCreated(long nativeVrShell, Tab[] mainTabs,
            Tab[] incognitoTabs);
    private native void nativeOnTabUpdated(long nativeVrShell, boolean incognito, int id,
            String title);
    private native void nativeOnTabRemoved(long nativeVrShell, boolean incognito, int id);
    private native Surface nativeTakeContentSurface(long nativeVrShell);
    private native void nativeRestoreContentSurface(long nativeVrShell);
    private native void nativeSetHistoryButtonsEnabled(
            long nativeVrShell, boolean canGoBack, boolean canGoForward);
    private native void nativeRequestToExitVr(long nativeVrShell, @UiUnsupportedMode int reason);
    private native void nativeLogUnsupportedModeUserMetric(
            long nativeVrShell, @UiUnsupportedMode int mode);
}
