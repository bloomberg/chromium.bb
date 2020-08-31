// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ResourceRequestBody;

/**
 * Mediator class for preview tab, responsible for communicating with other objects.
 */
public class EphemeralTabMediator {
    /** The delay (four video frames) after which the hide progress will be hidden. */
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = (1000 / 60) * 4;

    private final BottomSheetController mBottomSheetController;
    private final EphemeralTabCoordinator.FaviconLoader mFaviconLoader;
    private final EphemeralTabMetrics mMetrics;
    private final int mTopControlsHeightDp;

    private WebContents mWebContents;
    private EphemeralTabSheetContent mSheetContent;
    private WebContentsObserver mWebContentsObserver;
    private WebContentsDelegateAndroid mWebContentsDelegate;
    private Profile mProfile;

    /**
     * Constructor.
     */
    public EphemeralTabMediator(BottomSheetController bottomSheetController,
            EphemeralTabCoordinator.FaviconLoader faviconLoader, EphemeralTabMetrics metrics,
            int topControlsHeightDp) {
        mBottomSheetController = bottomSheetController;
        mFaviconLoader = faviconLoader;
        mMetrics = metrics;
        mTopControlsHeightDp = topControlsHeightDp;
    }

    /**
     * Initializes various objects for a new tab.
     */
    void init(WebContents webContents, ContentView contentView,
            EphemeralTabSheetContent sheetContent, Profile profile) {
        // Ensures that initialization is performed only when a new tab is opened.
        assert mProfile == null && mWebContentsObserver == null && mWebContentsDelegate == null;

        mProfile = profile;
        mWebContents = webContents;
        mSheetContent = sheetContent;
        createWebContentsObserver();
        createWebContentsDelegate();
        mSheetContent.attachWebContents(mWebContents, contentView, mWebContentsDelegate);
    }

    /**
     * Loads a new URL into the tab and makes it visible.
     */
    void requestShowContent(String url, String title) {
        loadUrl(url);
        mSheetContent.updateTitle(title);
        mBottomSheetController.requestShowContent(mSheetContent, true);
    }

    private void loadUrl(String url) {
        mWebContents.getNavigationController().loadUrl(new LoadUrlParams(url));
    }

    private void createWebContentsObserver() {
        assert mWebContentsObserver == null;
        mWebContentsObserver = new WebContentsObserver(mWebContents) {
            /** Whether the currently loaded page is an error (interstitial) page. */
            private boolean mIsOnErrorPage;

            private String mCurrentUrl;

            @Override
            public void loadProgressChanged(float progress) {
                if (mSheetContent != null) mSheetContent.setProgress(progress);
            }

            @Override
            public void didStartNavigation(NavigationHandle navigation) {
                mMetrics.recordNavigateLink();
                if (navigation.isInMainFrame() && !navigation.isSameDocument()) {
                    String url = navigation.getUrl();
                    if (TextUtils.equals(mCurrentUrl, url)) return;

                    // The link Back to Safety on the interstitial page will go to the previous
                    // page. If there is no previous page, i.e. previous page is NTP, the preview
                    // tab will be closed.
                    if (mIsOnErrorPage && NewTabPage.isNTPUrl(url)) {
                        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                        mCurrentUrl = null;
                        return;
                    }

                    mCurrentUrl = url;
                    mFaviconLoader.loadFavicon(
                            url, (drawable) -> onFaviconAvailable(drawable), mProfile);
                }
            }

            @Override
            public void titleWasSet(String title) {
                mSheetContent.updateTitle(title);
            }

            @Override
            public void didFinishNavigation(NavigationHandle navigation) {
                if (navigation.hasCommitted() && navigation.isInMainFrame()) {
                    mIsOnErrorPage = navigation.isErrorPage();
                    mSheetContent.updateURL(mWebContents.get().getVisibleUrl());
                }
            }
        };
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (mSheetContent != null) mSheetContent.startFaviconAnimation(drawable);
    }

    private void createWebContentsDelegate() {
        assert mWebContentsDelegate == null;
        mWebContentsDelegate = new WebContentsDelegateAndroid() {
            @Override
            public void visibleSSLStateChanged() {
                if (mSheetContent == null) return;
                int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
                mSheetContent.setSecurityIcon(getSecurityIconResource(securityLevel));
                mSheetContent.updateURL(mWebContents.getVisibleUrl());
            }

            @Override
            public void openNewTab(String url, String extraHeaders, ResourceRequestBody postData,
                    int disposition, boolean isRendererInitiated) {
                // We never open a separate tab when navigating in a preview tab.
                loadUrl(url);
            }

            @Override
            public boolean shouldCreateWebContents(String targetUrl) {
                loadUrl(targetUrl);
                return false;
            }

            @Override
            public void loadingStateChanged(boolean toDifferentDocument) {
                boolean isLoading = mWebContents != null && mWebContents.isLoading();
                if (isLoading) {
                    if (mSheetContent == null) return;
                    mSheetContent.setProgress(0);
                    mSheetContent.setProgressVisible(true);
                } else {
                    // Hides the Progress Bar after a delay to make sure it is rendered for at least
                    // a few frames, otherwise its completion won't be visually noticeable.
                    new Handler().postDelayed(() -> {
                        if (mSheetContent != null) mSheetContent.setProgressVisible(false);
                    }, HIDE_PROGRESS_BAR_DELAY_MS);
                }
            }

            @Override
            public int getTopControlsHeight() {
                return mTopControlsHeightDp;
            }
        };
    }

    @DrawableRes
    private static int getSecurityIconResource(@ConnectionSecurityLevel int securityLevel) {
        switch (securityLevel) {
            case ConnectionSecurityLevel.NONE:
            case ConnectionSecurityLevel.WARNING:
                return R.drawable.omnibox_info;
            case ConnectionSecurityLevel.DANGEROUS:
                return R.drawable.omnibox_not_secure_warning;
            case ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT:
            case ConnectionSecurityLevel.SECURE:
            case ConnectionSecurityLevel.EV_SECURE:
                return R.drawable.omnibox_https_valid;
            default:
                assert false;
        }
        return 0;
    }

    /**
     * Destroys the objects used for the current preview tab.
     */
    void destroyContent() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
        mWebContentsDelegate = null;
        mWebContents = null;
        mSheetContent = null;
        mProfile = null;
    }
}
