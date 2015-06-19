// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
import android.net.Uri;
import android.os.Handler;
import android.text.TextUtils;

import org.chromium.base.CalledByNative;
import org.chromium.base.FieldTrialList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.components.web_contents_delegate_android.WebContentsDelegateAndroid;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.WindowAndroid;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.concurrent.TimeUnit;

/**
 * Responsible for background content view creation and management.
 */
public class BackgroundContentViewHelper extends EmptyTabObserver {
    private static final String GOOGLE_PREVIEW_SERVER_PREFIX = "icl";

    // Google preview staging and canary servers. Only to be used in dev builds.
    private static final String GOOGLE_PREVIEW_SERVER_SUFFIX = ".googleusercontent.com";

    private static final String GWS_CHROME_JOINT_EXPERIMENT_ID_STRING = "gcjeid";

    private static final String GOOGLE_DOMAIN_PREFIX = "www.google.";

    // Instant search clicks swap reasons.
    // IMPORTANT: Do not renumber any existing constants (except the BOUNDARY ones). Keep these in
    // sync with histograms.xml file.
    private static final int SWAP_REASON_REGULAR = 0;
    private static final int SWAP_REASON_TIMEOUT = 1;
    private static final int SWAP_REASON_ABORT_ON_NAVIGATE = 2;
    private static final int SWAP_REASON_ON_PREVIEW_FAIL = 3;
    private static final int SWAP_REASON_ORIGINAL_FAIL = 4;
    private static final int SWAP_REASON_FORCE = 5;
    private static final int SWAP_REASON_BOUNDARY = 6;

    // Instant search clicks scroll state while swapping.
    // IMPORTANT: Do not renumber any existing constants (except the BOUNDARY ones). Keep these in
    // sync with histograms.xml file.
    private static final int PREVIEW_SCROLL_STATE_NO_SCROLL = 0;
    private static final int PREVIEW_SCROLL_STATE_SCROLL = 1;
    private static final int PREVIEW_SCROLL_STATE_SCROLL_AT_BOTTOM = 2;
    private static final int PREVIEW_SCROLL_STATE_BOUNDARY = 3;

    // Instant search clicks experiment state.
    private static final int INSTANT_SEARCH_CLICKS_EXPERIMENT_DISABLED = 0;
    private static final int INSTANT_SEARCH_CLICKS_EXPERIMENT_ENABLED = 1;
    private static final int INSTANT_SEARCH_CLICKS_EXPERIMENT_CONTROL = 2;

    /**
     * The maximum amount of time to wait for the background page to swap. We force the swap after
     * this timeout.
     */
    private final int mSwapTimeoutMs;

    /**
     * The minimum number of renderer frames needed before swap is triggered.
     */
    private final int mMinRendererFramesNeededForSwap;

    // Native pointer corresponding to the current object.
    private long mNativeBackgroundContentViewHelperPtr;

    // The content view core created to load url in background.
    private ContentViewCore mContentViewCore;

    // Android window used to created content view.
    private final WindowAndroid mWindowAndroid;

    // The webcontents observer on current content view of the tab.
    private WebContentsObserver mCurrentViewWebContentsObserver;

    // The webcontents observer that listens for events to swap in the new WebContents.
    private WebContentsObserver mWebContentsObserver;

    // The webcontents delegate that gets notified when background view is ready to be swapped.
    private final WebContentsDelegateAndroid mWebContentsDelegate;

    // Whether or not the preview page finished loading.
    private boolean mPreviewLoaded = false;

    // Whether or not the background view has painted anything non-empty.
    private boolean mPaintedNonEmpty = false;

    private int mNumRendererFramesReceived = 0;

    // Whether or not the swap is in progress. We will be in this state while syncing scroll offsets
    // between content views.
    private boolean mSwapInProgress = false;

    // Whether or not the background view started loading content.
    private boolean mDidStartLoad = false;

    // Whether or not the background view finished loading.
    private boolean mDidFinishLoad = false;

    // The tab associated with the background view.
    private final Tab mTab;

    // The delegate who is notified when background content view is ready.
    private final BackgroundContentViewDelegate mDelegate;

    // GestureStateListener attached to the background contentView.
    private final GestureStateListener mBackgroundGestureStateListener;

    // GestureStateListener attached to the current contentView of tab.
    private final GestureStateListener mCurrentViewGestureStateListener;

    private final Handler mHandler;

    private final Runnable mSwapOnTimeout = new Runnable() {
        @Override
        public void run() {
            forceSwappingContentViews();
            RecordHistogram.recordEnumeratedHistogram(
                    "InstantSearchClicks.ReasonForSwap",
                    SWAP_REASON_TIMEOUT, SWAP_REASON_BOUNDARY);
        }
    };

    private long mBackgroundLoadStartTimeStampMs;

    /**
     * Whether or not to record 'instant search clicks' uma for the current loaded page in tab.
     * This is set to true, for instant search clicks enabled urls(preview urls) and also for the
     * 'would have been' instant search click enabled urls.
     */
    private boolean mRecordUma = false;

    private final int mExperimentGroup;

    /**
     * The recent load progress before swapping. This will be used to return the progress after
     * swapping the page views.
     */
    private int mLoadProgress;

    /**
     * Whether we transferred pagescale from preview to original.
     */
    private boolean mOriginalPageZoomed = false;

    /**
     * The delegate that is notified when BackgroundContentView is ready to be swapped in.
     */
    public interface BackgroundContentViewDelegate {
        /**
         * Called when the background content view has drawn non-empty screen..
         * @param cvc The background ContentViewCore that is ready to be swapped in.
         * @param didStartLoad  Whether WebContentsObserver::DidStartProvisionalLoadForFrame() has
         *                      been called for background ContentViewCore.
         * @param didFinishLoad Whether WebContentsObserver::DidFinishLoad() has already been
         *                      called for background {@link ContentViewCore}.
         * @param progress      The recent load progress that was received for the
         *                      {@link ContentView}.
         */
        void onBackgroundViewReady(
                ContentViewCore cvc, boolean didStartLoad, boolean didFinishLoad, int progress);

        /**
         * Called to notify the load progress of backgrounc content view.
         * @param progress      The recent load progress that was received for the
         *                      {@link ContentView}.
         */
        void onLoadProgressChanged(int progress);
    }

    /**
     * Creates an instance of BackgroundContentViewHelper.
     * @param window    An instance of a {@link WindowAndroid}.
     * @param tab       The {@link ChromeTab} associated with current backgruond view.
     * @param delegate  The {@link BackgroundContentViewDelegate} to notify when backgruond view is
     *                  ready to swap in.
     */
    public BackgroundContentViewHelper(WindowAndroid windowAndroid, Tab tab,
            BackgroundContentViewDelegate delegate) {
        mWindowAndroid = windowAndroid;
        mNativeBackgroundContentViewHelperPtr = nativeInit();

        mTab = tab;
        assert delegate != null;
        mDelegate = delegate;
        mTab.addObserver(this);
        mBackgroundGestureStateListener = new GestureStateListener() {
            @Override
            public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
                syncPageStateAndSwapIfReady();
            }
        };

        mCurrentViewGestureStateListener = new GestureStateListener() {
            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                syncPageStateAndSwapIfReady();
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                syncPageStateAndSwapIfReady();
            }

            @Override
            public void onSingleTap(boolean consumed, int x, int y) {
                recordSingleTap();
            }
        };

        mWebContentsDelegate = new WebContentsDelegateAndroid() {
            @Override
            public void onLoadProgressChanged(int progress) {
                mLoadProgress = progress;
                mDelegate.onLoadProgressChanged(progress);
                trySwappingBackgroundView();
            }
        };
        mHandler = new Handler();

        mSwapTimeoutMs = nativeGetSwapTimeoutMs(mNativeBackgroundContentViewHelperPtr);
        mMinRendererFramesNeededForSwap =
            nativeGetNumFramesNeededForSwap(mNativeBackgroundContentViewHelperPtr);
        mBackgroundLoadStartTimeStampMs = System.currentTimeMillis();
        mExperimentGroup = getExperimentGroup();
    }

    /*
     * @return Experiment group that the current user fall into.
     */
    private int getExperimentGroup() {
        if (!DeviceClassManager.enableInstantSearchClicks()) {
            return INSTANT_SEARCH_CLICKS_EXPERIMENT_DISABLED;
        }

        String instantSearchClicksFieldTrialValue =
                FieldTrialList.findFullName("InstantSearchClicks");
        if (TextUtils.equals(instantSearchClicksFieldTrialValue, "Enabled")
                || TextUtils.equals(instantSearchClicksFieldTrialValue,
                        "InstantSearchClicksEnabled")) {
            return INSTANT_SEARCH_CLICKS_EXPERIMENT_ENABLED;
        } else if (TextUtils.equals(instantSearchClicksFieldTrialValue, "Control")) {
            return INSTANT_SEARCH_CLICKS_EXPERIMENT_CONTROL;
        }
        return INSTANT_SEARCH_CLICKS_EXPERIMENT_DISABLED;
    }

    /**
     * @return True iff content height of original page is greater than preview page.
     */
    private boolean hasBackgroundPageLoadedMoreThanPreview() {
        if (!mPaintedNonEmpty || mTab.getContentViewCore() == null) return false;
        return getContentViewCore().getContentHeightCss()
                > mTab.getContentViewCore().getContentHeightCss();
    }

    /**
     * @return True iff original page is loading in background content view which is not swapped in.
     */
    public boolean hasPendingBackgroundPage() {
        return mContentViewCore != null;
    }

    /**
     * @return True iff swapping is in progress. This is true till the content view is completely
     *         swapped in.
     */
    public boolean isPageSwappingInProgress() {
        return mSwapInProgress;
    }

    /**
     * @return The {@link ContentViewCore} associated with the current background page.
     */
    public ContentViewCore getContentViewCore() {
        return mContentViewCore;
    }

    /** @return The recent progress of background content view. */
    public int getProgress() {
        return mLoadProgress;
    }

    /**
     * Loads the specified URL in the background content view.
     * @param url the url to load.
     */
    private void loadUrl(String url) {
        if (mNativeBackgroundContentViewHelperPtr == 0) return;

        if (mContentViewCore == null) return;

        mContentViewCore.onShow();

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        // Use the referrer of preview url as referrer for original url.
        // We are always loading url during onDidStartProvisionalLoadForFrame of preview URL, where
        // load is not yet committed and so is the navigation entry. The current active
        // navigation entry is still the referrer for preview url.
        String referrerString = mTab.getWebContents()
                .getNavigationController().getOriginalUrlForVisibleNavigationEntry();
        Uri referrer = getUri(referrerString);
        String referrerHost = referrer.getHost();
        if (referrerHost == null
                || TextUtils.indexOf(referrerHost, GOOGLE_DOMAIN_PREFIX) != 0) {
            referrerHost = "www.google.com";
        }
        // We come here only through google.com and google.com always sends the referrer with
        // value at least equal to the origin (and full url in http requests).
        // Add 'gcjeid' param to referrer so that the PLT metrics are logged in a different bucket.
        // PLT metrics are logged in a  different bucket if the referer contains 'gcjeid' param.
        // For Experiment: gcjeid=19
        String referrerTrimmed = "http://" + referrerHost + "/?"
                + GWS_CHROME_JOINT_EXPERIMENT_ID_STRING + "=19";
        loadUrlParams.setReferrer(new Referrer(referrerTrimmed, Referrer.REFERRER_POLICY_ALWAYS));
        mContentViewCore.getWebContents().getNavigationController().loadUrl(loadUrlParams);
    }

    /**
     * Releases the ownership of the content view created. Also, resets the whole state of the
     * current object.
     */
    private ContentViewCore releaseContentViewCore() {
        mTab.detachOverlayContentViewCore(mContentViewCore);

        ContentViewCore originalContentViewCore = mContentViewCore;
        if (mContentViewCore != null && mNativeBackgroundContentViewHelperPtr != 0) {
            nativeReleaseWebContents(mNativeBackgroundContentViewHelperPtr);
        }
        reset();
        return originalContentViewCore;
    }

    /**
     * Destroys the contentview and the corresponding native object.
     */
    private void destroy() {
        if (mCurrentViewWebContentsObserver != null) {
            mCurrentViewWebContentsObserver.destroy();
            mCurrentViewWebContentsObserver = null;
        }
        destroyContentViewCore();

        if (mNativeBackgroundContentViewHelperPtr == 0) return;

        nativeDestroy(mNativeBackgroundContentViewHelperPtr);
        mNativeBackgroundContentViewHelperPtr = 0;
    }

    /**
     * Creates a new {@link ContentViewCore} and loads the given url.
     * @param url Url to load.
     */
    private void createContentViewCoreWithUrl(String url) {
        if (url == null || mNativeBackgroundContentViewHelperPtr == 0) return;

        destroyContentViewCore();

        Activity activity = mWindowAndroid.getActivity().get();
        mContentViewCore = new ContentViewCore(activity);
        ContentView cv = new ContentView(activity, mContentViewCore);
        mContentViewCore.initialize(cv, cv,
                WebContentsFactory.createWebContents(mTab.isIncognito(), false), mWindowAndroid);

        // The renderer should be set with the height considering the top controls(non-fullscreen
        // mode).
        mContentViewCore.setTopControlsHeight(mContentViewCore.getTopControlsHeightPix(), true);
        mWebContentsObserver = new WebContentsObserver(mContentViewCore.getWebContents()) {
            @Override
            public void didFirstVisuallyNonEmptyPaint() {
                mPaintedNonEmpty = true;
                trySwappingBackgroundView();
            }
            @Override
            public void didStartProvisionalLoadForFrame(long frameId, long parentFrameId,
                    boolean isMainFrame, String validatedUrl, boolean isErrorPage,
                    boolean isIframeSrcdoc) {
                if (isMainFrame) mDidStartLoad = true;
            }

            @Override
            public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
                if (isMainFrame) {
                    mDidFinishLoad = true;
                    trySwappingBackgroundView();
                }
            }

            @Override
            public void didFailLoad(boolean isProvisionalLoad,
                    boolean isMainFrame, int errorCode, String description, String failingUrl) {
                if (isMainFrame) {
                    RecordHistogram.recordEnumeratedHistogram(
                            "InstantSearchClicks.ReasonForSwap",
                            SWAP_REASON_ORIGINAL_FAIL, SWAP_REASON_BOUNDARY);
                    mDidFinishLoad = true;
                    trySwappingBackgroundView();
                }
            }
        };

        mContentViewCore.setContentViewClient(new ContentViewClient() {
            @Override
            public void onOffsetsForFullscreenChanged(
                    float topControlsOffsetYPix,
                    float contentOffsetYPix,
                    float overdrawBottomHeightPix) {
                // Using onOffsetsForFullscreenChanged, as it is called on every frame update.
                // Here we know that we have an update to frame. Ideally we need a function like
                // onUpdateFrame.
                if (mPaintedNonEmpty
                        // We need to make sure the frame we got has some content. Sometimes
                        // didFirstVisuallyNonEmptyPaint is called, but still the frame is empty and
                        // has the default width of Chrome '980'px. So waiting for the contentWidth
                        // of both preview and original to become same. Ideally in all cases they
                        // should be same as they both are same content. Also if we get hung here
                        // for some pages in the worst cases we swap on full load of original, or
                        // our hard timeout will kick in.
                        && getContentViewCore().getContentWidthCss()
                                == mTab.getContentViewCore().getContentWidthCss()
                        && !mSwapInProgress) {
                    mNumRendererFramesReceived++;
                    trySwappingBackgroundView();
                }
            }
        });

        if (mSwapTimeoutMs > 0) mHandler.postDelayed(mSwapOnTimeout, mSwapTimeoutMs);

        nativeSetWebContents(mNativeBackgroundContentViewHelperPtr,
                mContentViewCore, mWebContentsDelegate);

        mBackgroundLoadStartTimeStampMs = System.currentTimeMillis();

        loadUrl(url);
        // Keep the toolbar always show in background view.
        mContentViewCore.getWebContents().updateTopControlsState(false, true, false);
        mTab.attachOverlayContentViewCore(mContentViewCore, false);
    }

    /**
     * Helper to delete {@link ContentViewCore} and it's native web contents object.
     */
    private void destroyContentViewCore() {
        if (mContentViewCore != null && mNativeBackgroundContentViewHelperPtr != 0) {
            RecordHistogram.recordTimesHistogram(
                    "InstantSearchClicks.TimeInPreview",
                    System.currentTimeMillis() - mBackgroundLoadStartTimeStampMs,
                    TimeUnit.MILLISECONDS);
            mTab.detachOverlayContentViewCore(mContentViewCore);
            mContentViewCore.destroy();
            nativeDestroyWebContents(mNativeBackgroundContentViewHelperPtr);
        }
        reset();
        // Note: We cannot set these in reset(), as we call reset() from releaseContentView and
        // these values are used after that.
        mSwapInProgress = false;
        mLoadProgress = 0;
        mDidStartLoad = false;
        mDidFinishLoad = false;
    }

    /**
     * @return True iff Background view has finished loading or all the following conditions are
     *         met.
     *         1. Preview page has loaded completely.
     *         2. Background view has loaded atleast of preview page height.
     *         3. Background view has painted anything non-empty.
     */
    private boolean isBackgroundViewReady() {
        if (mDidFinishLoad) return true;

        if (!mPreviewLoaded) return false;

        if (!mPaintedNonEmpty) return false;

        if (mNumRendererFramesReceived < mMinRendererFramesNeededForSwap) return false;

        if (!hasBackgroundPageLoadedMoreThanPreview()) return false;

        return true;
    }

    /** Called when any of the conditions needed for background view swap are met. */
    private void trySwappingBackgroundView() {
        if (mSwapInProgress) return;
        // This function is asynchronously called from various places.
        // So make sure we have background content view before doing anything.
        if (!hasPendingBackgroundPage()) return;
        if (isBackgroundViewReady()) backgroundViewReady();
    }

    /** Called when we are ready to swap in background view to foreground. */
    private void backgroundViewReady() {
        if (mDelegate == null) return;
        mSwapInProgress = true;
        swapInBackgroundViewAfterScroll();
    }

    private boolean isGooglePreviewServerHost(String host) {
        if (host == null) return false;
        return TextUtils.equals(host, GOOGLE_PREVIEW_SERVER_PREFIX + GOOGLE_PREVIEW_SERVER_SUFFIX)
                || (ChromeVersionInfo.isDevBuild() && host.endsWith(GOOGLE_PREVIEW_SERVER_SUFFIX)
                && host.contains("promise"));
    }

    private Uri getUri(String url) {
        return Uri.parse(url == null ? "" : url);
    }

    /**
     * @return original url if the passed in url is a preview url, else null.
     */
    private String getOriginalUrlForPreviewUrl(String previewUrl) {
        Uri previewUri = getUri(previewUrl);
        String originalUrl = null;
        if (isGooglePreviewServerHost(previewUri.getHost())) {
            try {
                originalUrl = (new URL(previewUri.getQueryParameter("url"))).toString();
            } catch (MalformedURLException mue) {
                originalUrl = null;
            }
        }
        return originalUrl;
    }


    /**
     * Checks if the incoming url is instant search clicks eligible url. Only urls with referer
     * containing google search domain and 'gcjeid' param are instant search clicks eligible url.
     * @param url Url string.
     */
    private boolean isInstantSearchClicksControlUrl(String url) {
        String referrer = mTab.getContentViewCore().getWebContents()
                .getNavigationController().getOriginalUrlForVisibleNavigationEntry();
        Uri referrerUri = getUri(referrer);
        Uri previewUri = getUri(url);
        if (referrerUri.getHost() == null || previewUri.getHost() == null) return false;
        // Check if the url is coming from search page and has gcjeid param.
        // TODO(ksimbili): Make this check stricter.
        return (TextUtils.indexOf(referrerUri.getHost(), GOOGLE_DOMAIN_PREFIX) == 0
                && referrerUri.getQueryParameter(GWS_CHROME_JOINT_EXPERIMENT_ID_STRING) != null
                && TextUtils.indexOf(previewUri.getHost(), GOOGLE_DOMAIN_PREFIX) == -1);
    }

    private void setLogUma() {
        // We ideally need to listen for gestures on current tab only while swapping to detect when
        // scrolling ends(if one is active). But for UMA logging, we are listening for it always.
        mTab.getContentViewCore().addGestureStateListener(mCurrentViewGestureStateListener);
        mRecordUma = true;
    }

    private void resetLogUma() {
        if (mTab.getContentViewCore() != null) {
            mTab.getContentViewCore().removeGestureStateListener(mCurrentViewGestureStateListener);
        }
        mRecordUma = false;
    }

    /**
     * Loads the original url in the background view, if the passed in url is a preview url.
     * @param previewUrl Preview url string.
     */
    public void loadOriginalUrlIfPreview(String previewUrl) {
        if (mExperimentGroup == INSTANT_SEARCH_CLICKS_EXPERIMENT_DISABLED) {
            // If feature is disabled return;
            return;
        }

        if (mExperimentGroup == INSTANT_SEARCH_CLICKS_EXPERIMENT_CONTROL
                && isInstantSearchClicksControlUrl(previewUrl)) {
            setLogUma();
            return;
        }

        String originalUrl = getOriginalUrlForPreviewUrl(previewUrl);
        if (originalUrl == null) return;

        if (hasPendingBackgroundPage()) {
            // The following condition is safe, as browser reload will always be on original url.
            // Hence we will never encounter a situation where user is clicking on 'Reload' button
            // and the page is not reloading.
            if (originalUrl.equals(mContentViewCore.getWebContents().getUrl())) return;

            // Log uma for the previous preview getting aborted
            RecordHistogram.recordEnumeratedHistogram(
                    "InstantSearchClicks.ReasonForSwap",
                    SWAP_REASON_ABORT_ON_NAVIGATE, SWAP_REASON_BOUNDARY);
        }

        // Build content view.
        createContentViewCoreWithUrl(originalUrl);

        setLogUma();
    }

    /**
     * Transfers zoom level from current cvc to the background cvc.
     * @return Whether zoom level of background cvc had to be changed. It happens only when it is
     *    different from current cvc.
     */
    private boolean transferPageScaleFactor() {
        float currentViewPageScaleFactor =
                mTab.getContentViewCore().getRenderCoordinates().getPageScaleFactor();

        ContentViewCore backgroundViewCore = getContentViewCore();
        RenderCoordinates rc = backgroundViewCore.getRenderCoordinates();

        // Clamp the input value.
        float newPageScaleFactor = Math.max(rc.getMinPageScaleFactor(),
                Math.min(currentViewPageScaleFactor, rc.getMaxPageScaleFactor()));
        if (newPageScaleFactor == rc.getPageScaleFactor()) return false;

        float pageScaleFactorDelta = (newPageScaleFactor / rc.getPageScaleFactor());
        backgroundViewCore.pinchByDelta(pageScaleFactorDelta);
        mOriginalPageZoomed = true;
        return true;
    }

    /**
     * Transfers scroll offset from current cvc to the background cvc.
     * @return Whether scroll offset of background cvc had to be changed. It happens only when it
     *    is different from current cvc.
     */
    private boolean transferScrollOffset() {
        ContentViewCore currentViewCore = mTab.getContentViewCore();
        RenderCoordinates currentViewRenderCoordinates = currentViewCore.getRenderCoordinates();
        float currentViewScrollX = currentViewRenderCoordinates.getScrollXPix();
        float currentViewScrollY = currentViewRenderCoordinates.getScrollYPix();

        ContentViewCore backgroundViewCore = getContentViewCore();
        RenderCoordinates backgroundViewRenderCoordinates =
                backgroundViewCore.getRenderCoordinates();
        float maxHorizontalScroll = backgroundViewRenderCoordinates.getMaxHorizontalScrollPix();
        float maxVerticalScroll = backgroundViewRenderCoordinates.getMaxVerticalScrollPix();

        // Bound the scroll offsets to max scroll values.
        float newScrollX = Math.min(currentViewScrollX, maxHorizontalScroll);
        float newScrollY = Math.min(currentViewScrollY, maxVerticalScroll);

        float backgroundViewScrollX = backgroundViewRenderCoordinates.getScrollXPix();
        float backgroundViewScrollY = backgroundViewRenderCoordinates.getScrollYPix();
        int scrollByX = Math.round(backgroundViewRenderCoordinates.fromPixToLocalCss(
                newScrollX - backgroundViewScrollX));
        int scrollByY = Math.round(backgroundViewRenderCoordinates.fromPixToLocalCss(
                newScrollY - backgroundViewScrollY));

        if (Math.abs(scrollByX) <= 1 && Math.abs(scrollByY) <= 1) return false;

        if (!mOriginalPageZoomed) {
            backgroundViewCore.scrollBy(Math.round(newScrollX - backgroundViewScrollX),
                    Math.round(newScrollY - backgroundViewScrollY));
        } else {
            backgroundViewCore.scrollTo(Math.round(newScrollX), Math.round(newScrollY));
        }
        return true;
    }

    private void swapInBackgroundViewAfterScroll() {
        getContentViewCore().addGestureStateListener(mBackgroundGestureStateListener);
        syncPageStateAndSwapIfReady();
    }

    // Set scroll offset and zoom values from current cvc and swap the cvc if the both are synced.
    private void syncPageStateAndSwapIfReady() {
        // This function is asynchronously called from various places.
        // So make sure we have background content view before doing anything.
        if (!hasPendingBackgroundPage() || !isPageSwappingInProgress()) return;

        ContentViewCore currentViewCore = mTab.getContentViewCore();
        if (currentViewCore == null || currentViewCore.isScrollInProgress()) {
            // We'll comeback here on scrollEnded and flingEndGesture.
            return;
        }

        if (transferPageScaleFactor()) return;

        if (transferScrollOffset()) return;

        RenderCoordinates currentViewRenderCoordinates = currentViewCore.getRenderCoordinates();
        int currentViewScrollY = currentViewRenderCoordinates.getScrollYPixInt();

        int state;
        if (currentViewScrollY == 0) {
            state = PREVIEW_SCROLL_STATE_NO_SCROLL;
        } else if (currentViewScrollY >= currentViewRenderCoordinates.getMaxVerticalScrollPix()) {
            state = PREVIEW_SCROLL_STATE_SCROLL_AT_BOTTOM;
        } else {
            state = PREVIEW_SCROLL_STATE_SCROLL;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "InstantSearchClicks.PreviewScrollState", state, PREVIEW_SCROLL_STATE_BOUNDARY);
        RecordHistogram.recordEnumeratedHistogram(
                "InstantSearchClicks.ReasonForSwap", SWAP_REASON_REGULAR, SWAP_REASON_BOUNDARY);

        // We are ready swap the view now.
        swapInBackgroundView();
    }

    private void swapInBackgroundView() {
        // This function is asynchronously called from various places.
        // So make sure we have background content view before doing anything.
        if (!hasPendingBackgroundPage()) return;

        if (mTab.getContentViewCore() != null) {
            mTab.getContentViewCore().removeGestureStateListener(mCurrentViewGestureStateListener);
        }
        getContentViewCore().removeGestureStateListener(mBackgroundGestureStateListener);
        assert mDelegate != null;

        // Make sure the ContentViewCore is visible.
        getContentViewCore().setDrawsContent(true);

        // Merge history stack before swap.
        nativeMergeHistoryFrom(mNativeBackgroundContentViewHelperPtr, mTab.getWebContents());
        ContentViewCore cvc = releaseContentViewCore();
        mDelegate.onBackgroundViewReady(cvc, mDidStartLoad, mDidFinishLoad, mLoadProgress);

        RecordHistogram.recordTimesHistogram("InstantSearchClicks.TimeToSwap",
                System.currentTimeMillis() - mBackgroundLoadStartTimeStampMs,
                TimeUnit.MILLISECONDS);

        // Content view is released already, but still calling this to clear some state.
        destroyContentViewCore();

        // We need to include metrics from orignal page after swap too. For example, single tap
        // metric should include taps in both preview and original page after swap. This we reset
        // when tab commits a new navigation.
        setLogUma();
    }

    /** Stop the current navigation. */
    public void stopLoading() {
        destroyContentViewCore();
    }

    /**
     * Forces swapping of content views.
     */
    public void forceSwappingContentViews() {
        RecordHistogram.recordEnumeratedHistogram(
                "InstantSearchClicks.ReasonForSwap", SWAP_REASON_FORCE, SWAP_REASON_BOUNDARY);
        swapInBackgroundView();
    }

    /**
     * Calls 'unload' handler and deletes the webContents.
     * @param webContents The webcontents to be deleted.
     */
    public void unloadAndDeleteWebContents(WebContents webContents) {
        nativeUnloadAndDeleteWebContents(mNativeBackgroundContentViewHelperPtr, webContents);
    }

    /**
     * Helper to reset the state of the object.
     */
    private void reset() {
        mContentViewCore = null;
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }

        mPreviewLoaded = false;
        mPaintedNonEmpty = false;
        mNumRendererFramesReceived = 0;
        resetLogUma();
        mHandler.removeCallbacks(mSwapOnTimeout);
        mOriginalPageZoomed = false;
    }

    @Override
    public void onLoadProgressChanged(Tab tab, int progress) {
        if (!hasPendingBackgroundPage()) return;
        if (progress >= 100) {
            mPreviewLoaded = true;
            trySwappingBackgroundView();
        }
    }

    @Override
    public void onContentChanged(Tab tab) {
        if (mCurrentViewWebContentsObserver != null) {
            mCurrentViewWebContentsObserver.destroy();
        }
        if (tab.getContentViewCore() == null) return;

        loadOriginalUrlIfPreview(mTab.getUrl());

        mCurrentViewWebContentsObserver = new WebContentsObserver(mTab.getWebContents()) {
            @Override
            public void didFailLoad(boolean isProvisionalLoad,
                    boolean isMainFrame, int errorCode, String description, String failingUrl) {
                if (isMainFrame && hasPendingBackgroundPage()) {
                    if (TextUtils.equals(mContentViewCore.getWebContents().getUrl(),
                            getOriginalUrlForPreviewUrl(failingUrl))) {
                        forceSwappingContentViews();
                        RecordHistogram.recordEnumeratedHistogram(
                                "InstantSearchClicks.ReasonForSwap",
                                SWAP_REASON_ON_PREVIEW_FAIL, SWAP_REASON_BOUNDARY);
                    }
                }
            }

            @Override
            public void didStartProvisionalLoadForFrame(long frameId, long parentFrameId,
                    boolean isMainFrame, String validatedUrl, boolean isErrorPage,
                    boolean isIframeSrcdoc) {
                if (isMainFrame) {
                    if (mTab.getContentViewCore() != null) {
                        loadOriginalUrlIfPreview(validatedUrl);
                    } else {
                        destroyContentViewCore();
                    }
                }
            }

            @Override
            public void didCommitProvisionalLoadForFrame(
                    long frameId, boolean isMainFrame, String url, int transitionType) {
                if (!isMainFrame) return;

                if (!hasPendingBackgroundPage() && !isInstantSearchClicksControlUrl(url)) {
                    resetLogUma();
                }

                if (hasPendingBackgroundPage() && getOriginalUrlForPreviewUrl(url) == null) {
                    // We don't destroy the contentView in onDidStartProvisionalLoadForFrame, if the
                    // url is navigating to non-preview URL. We instead do that here. Some urls
                    // which are handled using intents, the url is never commited. In which case we
                    // still want to swap to happen.
                    destroyContentViewCore();
                }
            }
        };
    }

    @Override
    public void onDestroyed(Tab tab) {
        destroy();
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativeBackgroundContentViewHelperPtr;
    }

    public void recordBack() {
        if (mRecordUma) RecordUserAction.record("MobilePreviewPageBack");
    }

    public void recordReload() {
        if (mRecordUma) RecordUserAction.record("MobilePreviewPageReload");
    }

    public void recordTabClose() {
        if (mRecordUma) RecordUserAction.record("MobilePreviewPageTabClose");
    }

    public void recordSingleTap() {
        if (mRecordUma) RecordUserAction.record("MobilePreviewPageSingleTap");
    }

    private native long nativeInit();
    private native void nativeDestroy(long nativeBackgroundContentViewHelper);
    private native void nativeSetWebContents(long nativeBackgroundContentViewHelper,
            ContentViewCore contentViewCore, WebContentsDelegateAndroid delegate);
    private native void nativeDestroyWebContents(long nativeBackgroundContentViewHelper);
    private native void nativeReleaseWebContents(long nativeBackgroundContentViewHelper);
    private native void nativeMergeHistoryFrom(long nativeBackgroundContentViewHelper,
            WebContents webContents);
    private native void nativeUnloadAndDeleteWebContents(long nativeBackgroundContentViewHelper,
            WebContents webContents);
    private native int nativeGetSwapTimeoutMs(long nativeBackgroundContentViewHelper);
    private native int nativeGetNumFramesNeededForSwap(long nativeBackgroundContentViewHelper);
}
