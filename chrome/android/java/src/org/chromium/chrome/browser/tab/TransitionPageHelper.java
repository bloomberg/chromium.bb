// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.os.Handler;
import android.util.Pair;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.CalledByNative;
import org.chromium.base.FieldTrialList;
import org.chromium.chrome.browser.ChromeContentViewClient;
import org.chromium.chrome.browser.ContentViewUtil;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationTransitionDelegate;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;

/**
 * Responsible for transition content view creation and management.
 */
public class TransitionPageHelper extends EmptyTabObserver {
    private static final String TRANSITION_URL = "about:blank";

    private static final int TRANSITION_MS = 300;
    private static final int TRANSITION_DEFAULT_TIMEOUT = 500;
    private static final int TRANSITION_DEFAULT_FRAMES = 2;

    // Transition state when the transition page is active.
    private static final int STATE_NONE = 0;
    private static final int STATE_TRANSITION_STARTING = 1;
    private static final int STATE_WAIT_FOR_OUTGOING_UNLOADED = 2;
    private static final int STATE_WAIT_FOR_INCOMING_DCL = 3;
    private static final int STATE_UNLOAD_TRANSITION = 4;
    private static final int STATE_WAIT_FOR_TRANSITION_UNLOAD = 5;
    // Web to native app transition states
    private static final int STATE_WAIT_FOR_FETCH_TRANSITION_ELEMENTS = 6;
    private static final int STATE_WAIT_FOR_CURRENT_UNLOAD_TO_NATIVE_APP = 7;
    private static final int STATE_WAIT_FOR_NAVIGATE_TO_NATIVE_APP = 8;
    private static final int STATE_WAIT_FOR_BACK_FROM_NATIVE_APP = 9;

    // The activity associated with the tab.
    private final Activity mActivity;

    // Native pointer corresponding to the current object.
    private long mNativeTransitionPageHelperPtr;

    // The webcontents observer on current content view of the transition page.
    private WebContentsObserver mTransitionObserver;

    // The webcontents observer on current content view of the tab.
    private WebContentsObserver mSourceObserver;

    // The context used to create content view.
    private final Context mContext;

    // Android window used to created content view.
    private final WindowAndroid mWindowAndroid;

    // The transition content view created to display over the tab.
    private ContentViewCore mTransitionContentViewCore;

    // The source content view being navigated.
    private ContentViewCore mSourceContentViewCore;

    // The tab associated with the background view.
    private final Tab mTab;

    // Set when the transition has had TRANSITION_MS ms to show.
    private boolean mTransitionFinished;

    // Set once the incoming page is ready to be shown, either because it's
    // fired DOMContentLoaded, or timed out.
    private boolean mDestinationReady;

    // Set once the transition page has been painted.
    private boolean mTransitionPainted;

    // Incremented when the transition page is painted.
    private int mTransitionFramesPainted;

    // Set once the transition content view has fully loaded.
    private boolean mTransitionLoaded;

    // Set once the transition page has been unloaded (stylesheets have been applied
    // and TRANSITION_MS ms have passed).
    private boolean mTransitionUnloaded;

    // Set once the incoming page has signalled that it's received bytes and has
    // paused the navigation.
    private boolean mDestinationReadyToResume;

    // Set once the incoming page has been painted.
    private boolean mDestinationPainted;

    // Set once the incoming page has signaled DOMContentLoaded.
    private boolean mDestinationDOMContentLoaded;

    // Set once current page starts navigation transition to the native app.
    private boolean mTransitionToNativeApp;

    // Set once current page has been unloaded (exit transitions have been applied and
    // TRANSITION_MS ms have passed).
    private boolean mCurrentUnloadedToNativeApp;

    // Set once current page has finished navigation to the native app.
    private boolean mNavigateToNativeAppFinished;

    // Set once current page is back from the native app.
    private boolean mBackFromNativeApp;

    // The id of the main frame of the navigating page.
    private long mDestinationMainFrameID;

    // An array of stylesheet URL's to apply to the transition page.
    private final ArrayList<String> mTransitionStylesheets;

    // The background colour of the incoming app.
    private String mTransitionColor;

    // The CSS selector that selects the transition elements of the source page.
    private String mCSSSelector;

    // The background of the outgoing app.
    private String mSourceColor;

    // The current state of the transition.
    private int mState;

    // The current visibility state of the transition page.
    private boolean mVisible;

    // The current opacity of the transition page.
    private float mOpacity;

    // Used to animate the opacity when fading the transition page.
    private final ObjectAnimator mAnimator;

    private final Handler mHandler;

    // The Intent URL used during web to native app navigation transitions.
    private String mIntentUrl;

    // The Intent used during web to native app navigation transitions.
    private Intent mIntent;

    // The offset of the visible content.
    private int mVisibleContentOffset;

    // The transition elements.
    private ArrayList<TransitionElement> mTransitionElements;

    // The Views for the transition elements.
    private ArrayList<TransitionElementView> mTransitionElementViews;

    // Set once the transition elements are fetched.
    private boolean mTransitionElementsFetched;

    // Used to check if the transition elements are visible.
    private boolean mTransitionElementVisible;

    private ActivityStateListener mActivityStateListener = new ActivityStateListener() {
        @Override
        public void onActivityStateChange(Activity activity, int newState) {
            assert activity == mActivity;

            if (newState == ActivityState.RESUMED) {
                backFromNativeAppOnResume();
            } else if (newState == ActivityState.STARTED) {
                backFromNativeAppOnStart();
            }
        }
    };

    private static class TransitionElement {
        // Transition element's name.
        private final String mName;

        // Transition element's position and size.
        private final Rect mRect;

        public TransitionElement(String name, Rect rect) {
            mName = name;
            mRect = rect;
        }

        public String name() {
            return mName;
        }

        public Rect rect() {
            return mRect;
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private class TransitionElementView extends View {
        public TransitionElementView(Context context) {
            super(context);
        }

        @Override
        public void setAlpha(float alpha) {
            if (mCSSSelector != null) {
                if (!mTransitionElementVisible && Float.compare(alpha, 0) != 0) {
                    mTransitionElementVisible = true;
                    mSourceContentViewCore.getWebContents().showTransitionElements(mCSSSelector);

                    // This is a hack. May only work on Android 5.0.
                    // Delay 150ms. We need to wait for the CSS opacity property to take effect
                    // before hiding the view. Thread.sleep(150ms) does not work because it pauses
                    // the whole UI thread, including the CSS opacity effect. In Android 5.0,
                    // Activity Transitions uses 3 postOnAnimation()s to delay hiding the view for
                    // double-buffering. So we can add more delays there and give the run loop
                    // chances to push frames.
                    // Reference - Android 5.0 Lollipop source code:
                    // frameworks/base/core/java/android/app/EnterTransitionCoordinator.java
                    // - startSharedElementTransition()
                    postOnAnimation(new Runnable() {
                        static final int MIN_ANIMATION_FRAMES = 3;
                        int mIterations = 0;
                        @Override
                        public void run() {
                            try {
                                Thread.sleep(50);
                            } catch (InterruptedException e) {
                                // Ignore
                            }
                            mIterations++;
                            if (mIterations < MIN_ANIMATION_FRAMES) {
                                postOnAnimation(this);
                            }
                        }
                    });
                } else if (mTransitionElementVisible && Float.compare(alpha, 0) == 0) {
                    mTransitionElementVisible = false;
                    mSourceContentViewCore.getWebContents().hideTransitionElements(mCSSSelector);
                }
            }
            super.setAlpha(alpha);
        }
    }

    /**
     * Creates an instance of TransitionPageHelper.
     * @param activity An instance of a {@link Activity}.
     * @param windowAndroid An instance of a {@link WindowAndroid}.
     * @param tab The {@link Tab} associated with current background view.
     */
    public TransitionPageHelper(Activity activity, WindowAndroid windowAndroid, Tab tab) {
        mActivity = activity;
        mContext = activity.getApplicationContext();
        mWindowAndroid = windowAndroid;
        mTab = tab;
        mTab.addObserver(this);

        mNativeTransitionPageHelperPtr = nativeInit();
        mHandler = new Handler();
        mTransitionStylesheets = new ArrayList<String>();
        mTransitionElements = new ArrayList<TransitionElement>();
        mTransitionElementViews = new ArrayList<TransitionElementView>();
        mTransitionElementVisible = true;
        mAnimator = ObjectAnimator.ofFloat(this, "transitionOpacity", 1.0f, 0.0f);
        mAnimator.setDuration(TRANSITION_MS);
        mAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (mTransitionContentViewCore != null) {
                    mTransitionContentViewCore.onHide();
                }
                mTransitionUnloaded = true;
                updateState();
            }
        });
        ApplicationStatus.registerStateListenerForActivity(mActivityStateListener, mActivity);

        // Access the field trial so that it gets saved.
        FieldTrialList.findFullName("NavigationTransitions");

        resetTransitionState();
    }

    /**
     * @return The {@link ContentViewCore} associated with the transition page.
     */
    public ContentViewCore getTransitionContentViewCore() {
        return mTransitionContentViewCore;
    }

    /**
     * Destroys the content view and the corresponding native object.
     */
    private void destroy() {
        if (mNativeTransitionPageHelperPtr == 0) return;

        ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);

        teardownNavigation();

        if (mSourceObserver != null) {
            mSourceObserver.destroy();
            mSourceObserver = null;
        }

        nativeDestroy(mNativeTransitionPageHelperPtr);
        mNativeTransitionPageHelperPtr = 0;
    }

    /**
     * @return True iff a navigation transition is currently underway.
     */
    public boolean isTransitionRunning() {
        return mState != STATE_NONE;
    }

    /**
     * Schedules a default timeout to signal if we haven't seen enough frames painted.
     */
    private void scheduleTransitionFramesTimeout() {
        mHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                mTransitionFramesPainted = TRANSITION_DEFAULT_FRAMES;
                updateState();
            }
        }, TRANSITION_DEFAULT_TIMEOUT);
    }

    /**
     * Notifies the outgoing page that the transition elements have been shown,
     * and posts a task to wait TRANSITION_MS ms for it to run.
     */
    private void scheduleCurrentToUnload() {
        mHandler.postDelayed(
                new Runnable() {
                    @Override
                    public void run() {
                        mTransitionFinished = true;
                        mTransitionContentViewCore.getWebContents().insertCSS(String.format(
                                "@-webkit-keyframes __transition_crossfade {"
                                + "from {background: %s; }%n"
                                + "to {background: %s; }}%n"
                                + "-webkit-animation: __transition_crossfade %dms forwards;",
                                mSourceColor, mTransitionColor, TRANSITION_MS));
                        scheduleTransitionFramesTimeout();
                        updateState();
                    }
                }, TRANSITION_MS);

        setTransitionVisibility(true);
        setTransitionOpacity(1.0f);

        if (mTransitionContentViewCore != null && mSourceContentViewCore != null) {
            // Notify the outgoing page that the transition elements have
            // been shown in the transition page, so that they can be hidden
            // in the outgoing page.
            mSourceContentViewCore.getWebContents().beginExitTransition(mCSSSelector, false);
        }

        applyTransitionStylesheets();
    }

    /**
     * Adds the transition stylesheets to the document.
     */
    private void applyTransitionStylesheets() {
        for (int i = 0; i < mTransitionStylesheets.size(); i++) {
            mTransitionContentViewCore.getWebContents().addStyleSheetByURL(
                    mTransitionStylesheets.get(i));
        }
    }

    /**
     * Smoothly fades out the transition page to cover up any per-pixel
     * imperfections.
     */
    private void fadeOutTransition() {
        mAnimator.start();
    }

    /**
     * Handles the current state of the transition state machine. The state
     * machine has two routes. The first one is web-to-web transition and the
     * second one is web-to-native-app transition. Both routes start with
     * STATE_TRANSITION_STARTING.
     */
    private void updateState() {
        // Web to web transition state machine
        if (mState == STATE_TRANSITION_STARTING) {
            // Transition page hasn't been shown yet. If it's ready to go, show
            // it and we can move on to waiting 300 ms for outgoing page to
            // unload.
            if (mTransitionPainted && mTransitionLoaded) {
                scheduleCurrentToUnload();
                mState = STATE_WAIT_FOR_OUTGOING_UNLOADED;
            }
        } else if (mState == STATE_WAIT_FOR_OUTGOING_UNLOADED) {
            // Outgoing page gets 300 ms to unload. Once that's done, we can
            // resume navigation, assuming the incoming page is ready to
            // to navigate.
            if (mDestinationReadyToResume && mTransitionFinished
                    && mTransitionFramesPainted >= TRANSITION_DEFAULT_FRAMES) {
                if (mSourceContentViewCore != null) {
                    mSourceContentViewCore.getWebContents().resumeResponseDeferredAtStart();
                }
                mState = STATE_WAIT_FOR_INCOMING_DCL;
                updateState();
            }
        } else if (mState == STATE_WAIT_FOR_INCOMING_DCL) {
            // Once the navigation is resumed, we wait until we get the
            // DOMContentLoaded signal from the incoming page before starting
            // to unload the transition page.
            if (mDestinationReady) {
                mState = STATE_UNLOAD_TRANSITION;
                updateState();
            }
        } else if (mState == STATE_UNLOAD_TRANSITION) {
            // Once the transition page is unloaded, need to give it 300 ms
            // to fade out to hide any per-pixel positioning differences.
            fadeOutTransition();
            mState = STATE_WAIT_FOR_TRANSITION_UNLOAD;
        } else if (mState == STATE_WAIT_FOR_TRANSITION_UNLOAD) {
            // Wait until the fade is done and tear down the transition page.
            if (mTransitionUnloaded) {
                teardownNavigation();
                mState = STATE_NONE;
            }
        }

        // Web to native app transition state machine
        if (mState == STATE_TRANSITION_STARTING) {
            if (mTransitionToNativeApp) {
                mState = STATE_WAIT_FOR_FETCH_TRANSITION_ELEMENTS;
                fetchTransitionElements();
            }
        } else if (mState == STATE_WAIT_FOR_FETCH_TRANSITION_ELEMENTS) {
            if (mTransitionElementsFetched) {
                mState = STATE_WAIT_FOR_CURRENT_UNLOAD_TO_NATIVE_APP;
                scheduleCurrentToUnloadToNativeApp();
            }
        } else if (mState == STATE_WAIT_FOR_CURRENT_UNLOAD_TO_NATIVE_APP) {
            if (mCurrentUnloadedToNativeApp) {
                mState = STATE_WAIT_FOR_NAVIGATE_TO_NATIVE_APP;
                navigateToNativeApp();
            }
        } else if (mState == STATE_WAIT_FOR_NAVIGATE_TO_NATIVE_APP) {
            if (mNavigateToNativeAppFinished) {
                mState = STATE_WAIT_FOR_BACK_FROM_NATIVE_APP;
            }
        } else if (mState == STATE_WAIT_FOR_BACK_FROM_NATIVE_APP) {
            if (mBackFromNativeApp) {
                cleanUpBackFromNativeApp();
                teardownNavigation();
                mState = STATE_NONE;
            }
        }
    }

    /**
     * Notifies any listeners of opacity changes.
     */
    private void setTransitionOpacity(float opacity) {
        mOpacity = opacity;

        nativeSetOpacity(mNativeTransitionPageHelperPtr, mTransitionContentViewCore, opacity);
    }

    /**
     * Notifies any listeners of visibility changes.
     */
    private void setTransitionVisibility(boolean visible) {
        mVisible = visible;

        if (mTransitionContentViewCore != null) mTransitionContentViewCore.setDrawsContent(visible);
    }

    /**
     * @return The visibility state of the transition page.
     */
    public boolean isTransitionVisible() {
        return mVisible;
    }

    /**
     * @return The opacity of the transition page.
     */
    public float getTransitionOpacity() {
        return mOpacity;
    }

    /**
     * Called when a transition navigation is initiated to reset values to their defaults.
     */
    private void resetTransitionState() {
        mTransitionUnloaded = false;
        mTransitionFinished = false;
        mDestinationReady = false;
        mDestinationReadyToResume = false;
        mDestinationPainted = false;
        mDestinationDOMContentLoaded = false;
        mDestinationMainFrameID = -1;
        mTransitionPainted = false;
        mTransitionFramesPainted = 0;
        mTransitionLoaded = false;
        mState = STATE_NONE;
        mVisible = false;
        mOpacity = 0.0f;
        mTransitionStylesheets.clear();
        mTransitionColor = null;
        mCSSSelector = null;
        mAnimator.cancel();
        mTransitionElements.clear();
        mTransitionElementsFetched = false;
        removeTransitionElementViewsFromLayout();
        mTransitionElementVisible = true;
        mTransitionToNativeApp = false;
        mCurrentUnloadedToNativeApp = false;
        mNavigateToNativeAppFinished = false;
        mBackFromNativeApp = false;
        mIntentUrl = null;
        mIntent = null;
        if (mSourceContentViewCore != null) {
            mVisibleContentOffset = mSourceContentViewCore.getViewportSizeOffsetHeightPix();
        }
    }

    /**
     * Called when a transition navigation is initiated. Creates a new transition
     * content view and resets the state machine.
     */
    private void startTransitionNavigation() {
        if (mState != STATE_NONE) return;

        teardownNavigation();
        resetTransitionState();

        mState = STATE_TRANSITION_STARTING;

        buildContentView();

        // Grab the background color from the outgoing page, so we can add it
        // to the transition layer.
        JavaScriptCallback jsCallback = new JavaScriptCallback() {
            @Override
            public void handleJavaScriptResult(String jsonResult) {
                mSourceColor = jsonResult.replaceAll("\"", "");
            }
        };

        String script = "getComputedStyle(document.body).backgroundColor";
        mSourceContentViewCore.getWebContents().evaluateJavaScript(script, jsCallback);
    }

    /**
     * Detaches any observers and destroys the transition content view.
     */
    private void teardownNavigation() {
        mHandler.removeCallbacksAndMessages(null);
        if (mNativeTransitionPageHelperPtr == 0) return;

        if (mTransitionObserver != null) {
            mTransitionObserver.destroy();
            mTransitionObserver = null;
        }
        if (mTransitionContentViewCore != null) {
            if (mTab != null) mTab.detachOverlayContentViewCore(mTransitionContentViewCore);
            mTransitionContentViewCore.destroy();
            mTransitionContentViewCore = null;
            nativeReleaseWebContents(mNativeTransitionPageHelperPtr);
        }
        mState = STATE_NONE;
    }

    /**
     * Creates a new transition content view and notifies listeners.
     */
    private void buildContentView() {
        if (mNativeTransitionPageHelperPtr == 0) return;

        mTransitionContentViewCore = new ContentViewCore(mContext);
        ContentView cv = ContentView.newInstance(mContext, mTransitionContentViewCore);
        mTransitionContentViewCore.initialize(cv, cv,
                ContentViewUtil.createWebContentsWithSharedSiteInstance(mSourceContentViewCore),
                mWindowAndroid);

        // The renderer should be set with the height considering the top controls(non-fullscreen
        // mode).
        mTransitionContentViewCore.setTopControlsHeight(
                mTransitionContentViewCore.getTopControlsHeightPix(), true);

        mTransitionObserver = createTransitionObserver();

        if (mTab != null) mTab.attachOverlayContentViewCore(mTransitionContentViewCore, true);

        nativeSetWebContents(mNativeTransitionPageHelperPtr, mTransitionContentViewCore);
        setTransitionOpacity(0.0f);

        mTransitionContentViewCore.setContentViewClient(new ChromeContentViewClient() {
            @Override
            public void onOffsetsForFullscreenChanged(
                    float topControlsOffsetYPix,
                    float contentOffsetYPix,
                    float overdrawBottomHeightPix) {
                if (mState != STATE_WAIT_FOR_OUTGOING_UNLOADED) return;

                if (mTransitionContentViewCore.getContentWidthCss()
                        == mTab.getContentViewCore().getContentWidthCss()) {
                    mTransitionFramesPainted++;
                    updateState();
                }
            }
        });
    }

    /**
     * Creates an observer for the transition web contents, and listens specifically
     * for paint and loading events.
     */
    private WebContentsObserver createTransitionObserver() {
        return new WebContentsObserver(mTransitionContentViewCore.getWebContents()) {
            @Override
            public void didFirstVisuallyNonEmptyPaint() {
                if (!mTransitionPainted) {
                    mTransitionPainted = true;
                    updateState();
                }
            }

            @Override
            public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
                if (isMainFrame) {
                    mTransitionLoaded = true;
                    updateState();
                }
            }
        };
    }

    /**
     * Creates an observer for the tab's web contents, and listens specifically
     * for transition, paint, and DOMContentLoaded events.
     */
    private WebContentsObserver createSourceObserver() {
        return new WebContentsObserver(mSourceContentViewCore.getWebContents()) {
            private void signalDestinationMaybeReady() {
                if ((mDestinationDOMContentLoaded && mDestinationPainted) && !mDestinationReady) {
                    mDestinationReady = true;
                    updateState();
                }
            }

            @Override
            public void didFirstVisuallyNonEmptyPaint() {
                mDestinationPainted = true;
                signalDestinationMaybeReady();
            }

            @Override
            public void documentLoadedInFrame(long frameId) {
                if (frameId == mDestinationMainFrameID) {
                    mDestinationDOMContentLoaded = true;
                    signalDestinationMaybeReady();
                    mHandler.postDelayed(
                            new Runnable() {
                                @Override
                                public void run() {
                                    mDestinationPainted = true;
                                    signalDestinationMaybeReady();
                                }
                            }, TRANSITION_DEFAULT_TIMEOUT);
                }
            }

            @Override
            public void didFailLoad(boolean isProvisionalLoad,
                    boolean isMainFrame, int errorCode, String description, String failingUrl) {
                // Just consider a failed load as the destination is ready. The transition
                // page will fade out naturally.
                if (isMainFrame) {
                    mDestinationReady = true;
                    updateState();
                }
            }
        };
    }

    @Override
    public void onContentChanged(Tab tab) {
        if (tab == mTab) {
            if (mSourceContentViewCore != null && mSourceContentViewCore.getWebContents() != null) {
                mSourceContentViewCore.getWebContents().setNavigationTransitionDelegate(null);
            }

            mSourceContentViewCore = tab.getContentViewCore();

            if (mSourceObserver != null) {
                mSourceObserver.destroy();
                mSourceObserver = null;
            }

            if (mSourceContentViewCore == null) return;

            mSourceObserver = createSourceObserver();
            mSourceContentViewCore.getWebContents().setNavigationTransitionDelegate(
                    new NavigationTransitionDelegate() {
                        @Override
                        public void didDeferAfterResponseStarted(String markup, String cssSelector,
                                String color) {
                            if (mTransitionContentViewCore != null && isTransitionRunning()) {
                                mDestinationReadyToResume = true;

                                LoadUrlParams navigationParams = new LoadUrlParams(TRANSITION_URL);

                                mTransitionContentViewCore.getWebContents()
                                        .getNavigationController().loadUrl(navigationParams);
                                mTransitionContentViewCore.setBackgroundOpaque(false);
                                mTransitionContentViewCore.getWebContents().setupTransitionView(
                                        markup);
                                mTransitionContentViewCore.onShow();

                                setTransitionOpacity(0.0f);
                                setTransitionVisibility(false);

                                mTransitionColor = color;
                                mCSSSelector = cssSelector;
                            }
                        }

                        @Override
                        public void didStartNavigationTransitionForFrame(long frameId) {
                            startTransitionNavigation();
                            mDestinationMainFrameID = frameId;
                        }

                        @Override
                        public boolean willHandleDeferAfterResponseStarted() {
                            return true;
                        }

                        @Override
                        public void addEnteringStylesheetToTransition(String stylesheet) {
                            if (mTransitionContentViewCore != null && isTransitionRunning()) {
                                mTransitionStylesheets.add(stylesheet);
                            }
                        }

                        @Override
                        public void addNavigationTransitionElements(
                                String name, int x, int y, int width, int height) {
                            if (isTransitionRunning()) {
                                Rect rect = new Rect(x, y, x + width, y + height);
                                TransitionElement element = new TransitionElement(name, rect);
                                mTransitionElements.add(element);
                            }
                        }

                        @Override
                        public void onTransitionElementsFetched(String cssSelector) {
                            if (isTransitionRunning()) {
                                mCSSSelector = cssSelector;
                                mTransitionElementsFetched = true;
                                updateState();
                            }
                        }
                    });
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        if (tab == mTab) {
            destroy();
        }
    }

    /**
     * Check Whether transition is starting.
     * @return Whether the transition is starting.
     */
    public boolean isTransitionStarting() {
        return mState == STATE_TRANSITION_STARTING;
    }

    /**
     * Transition to native app. This is the starting point of the web-to-native-app navigation
     * transitions.
     * @param url The Intent URL of the native app.
     * @param intent The Intent of the native app.
     */
    public void transitionToNativeApp(String url, Intent intent) {
        if (!isTransitionStarting()) return;
        mIntentUrl = url;
        mIntent = intent;
        mTransitionToNativeApp = true;
        updateState();
    }

    private void fetchTransitionElements() {
        mSourceContentViewCore.getWebContents().fetchTransitionElements(mIntentUrl);
    }

    private void scheduleCurrentToUnloadToNativeApp() {
        // Let the exit transition run for TRANSITION_MS.
        mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    mCurrentUnloadedToNativeApp = true;
                    updateState();
                }
        }, TRANSITION_MS);

        if (mSourceContentViewCore != null) {
            mSourceContentViewCore.getWebContents().beginExitTransition(mCSSSelector, true);
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void navigateToNativeApp() {
        FrameLayout rootContentView = (FrameLayout) mActivity.findViewById(android.R.id.content);
        @SuppressWarnings("unchecked")
        Pair<View, String>[] sharedElements = new Pair[mTransitionElements.size()];
        for (int i = 0; i < mTransitionElements.size(); i++) {
            // Create TransitionElementViews and add them to the root view
            Rect r = mTransitionElements.get(i).rect();
            int left = convertPagePxToDpi(r.left);
            int top = convertPagePxToDpi(r.top) + mVisibleContentOffset;
            int right = convertPagePxToDpi(r.right);
            int bottom = convertPagePxToDpi(r.bottom) + mVisibleContentOffset;
            FrameLayout.LayoutParams layoutParams =
                    new FrameLayout.LayoutParams(right - left, bottom - top);
            layoutParams.setMargins(left, top, right, bottom);

            TransitionElementView v = new TransitionElementView(mActivity);
            v.setId(View.generateViewId());
            v.setTransitionName(mTransitionElements.get(i).name());
            // Both setLayoutParams() and setLeft() etc are needed when setting the position and
            // size.
            v.setLayoutParams(layoutParams);
            v.setLeft(left);
            v.setTop(top);
            v.setRight(right);
            v.setBottom(bottom);

            rootContentView.addView(v);
            mTransitionElementViews.add(v);
            sharedElements[i] = Pair.create((View) v, mTransitionElements.get(i).name());
        }
        ActivityOptions activityOptions =
                ActivityOptions.makeSceneTransitionAnimation(mActivity, sharedElements);
        // Remove FLAG_ACTIVITY_NEW_TASK.
        int flags = mIntent.getFlags();
        flags &= ~Intent.FLAG_ACTIVITY_NEW_TASK;
        mIntent.setFlags(flags);
        mActivity.startActivity(mIntent, activityOptions.toBundle());
        transitionToNativeAppFinished();
    }

    /**
     * The web page is now transitioned to the native app. Set the tab to be covered by the native
     * app and update the state machine.
     */
    public void transitionToNativeAppFinished() {
        if (mState != STATE_WAIT_FOR_NAVIGATE_TO_NATIVE_APP) return;
        mNavigateToNativeAppFinished = true;
        mTab.setCoveredByChildActivity(true);
        updateState();
    }

    /**
     * The web page is back from the native app when Chrome is in onStart() stage. Revert the exit
     * transitions.
     */
    public void backFromNativeAppOnStart() {
        if (mState != STATE_WAIT_FOR_BACK_FROM_NATIVE_APP) return;
        if (mSourceContentViewCore != null) {
            mSourceContentViewCore.getWebContents().revertExitTransition();
        }
    }

    /**
     * The web page is back from the native app when Chrome is in onResume() stage. Set the tab to
     * be uncovered by the native app and update the state machine.
     */
    public void backFromNativeAppOnResume() {
        if (mState != STATE_WAIT_FOR_BACK_FROM_NATIVE_APP) return;
        mTab.setCoveredByChildActivity(false);
        mBackFromNativeApp = true;
        updateState();
    }

    private void cleanUpBackFromNativeApp() {
        // Currently there is data left in TransitionRequestManager. We need to
        // clear them to get correct future navigation behavior.
        mSourceContentViewCore.getWebContents().clearNavigationTransitionData();
        removeTransitionElementViewsFromLayout();
    }

    private void removeTransitionElementViewsFromLayout() {
        // Remove the TransitionElementViews from the layout's view hierarchy.
        FrameLayout rootContentView = (FrameLayout) mActivity.findViewById(android.R.id.content);
        for (int i = 0; i < mTransitionElementViews.size(); i++) {
            rootContentView.removeView(mTransitionElementViews.get(i));
        }
        mTransitionElementViews.clear();
    }

    /**
     * Convert page pixels to dpis.
     * @param pagePixel Page pixel value from the renderer.
     * @return Page pixel in dpis.
     */
    private int convertPagePxToDpi(int pagePixel) {
        float scale = mActivity.getResources().getDisplayMetrics().density;
        return (int) Math.ceil(pagePixel * scale);
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativeTransitionPageHelperPtr;
    }

    private native long nativeInit();
    private native void nativeDestroy(long nativeTransitionPageHelper);
    private native void nativeSetWebContents(long nativeTransitionPageHelper,
            ContentViewCore contentViewCore);
    private native void nativeReleaseWebContents(long nativeTransitionPageHelper);
    private native void nativeSetOpacity(long nativeTransitionPageHelper,
            ContentViewCore contentViewCore, float opacity);
}
