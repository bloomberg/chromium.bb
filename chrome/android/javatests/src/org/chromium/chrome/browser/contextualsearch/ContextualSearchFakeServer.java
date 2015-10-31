// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentProgressObserver;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContentFactory;

import java.net.MalformedURLException;
import java.net.URL;

import javax.annotation.Nullable;

/**
 * Implements a fake Contextual Search server, for testing purposes.
 * TODO(donnd): add more functionality to this class once the overall approach has been validated.
 * TODO(donnd): rename this class when we refactor and rename the interface it implements.  Should
 *              be something like ContextualSearchFakeEnvironment.
 */
@VisibleForTesting
class ContextualSearchFakeServer
        implements ContextualSearchNetworkCommunicator, OverlayPanelContentFactory {

    private final ContextualSearchNetworkCommunicator mBaseManager;

    private final OverlayContentDelegate mContentDelegate;
    private final OverlayContentProgressObserver mProgressObserver;
    private final ChromeActivity mActivity;

    private OverlayPanelContent mContent;

    private String mLoadedUrl;
    private int mLoadedUrlCount;
    private String mSearchTermRequested;
    private boolean mShouldUseHttps;

    /**
     * Constructs a fake Contextual Search server that will callback to the given baseManager.
     * @param baseManager The manager to call back to for server responses.
     */
    @VisibleForTesting
    ContextualSearchFakeServer(ContextualSearchNetworkCommunicator baseManager,
            OverlayContentDelegate contentDelegate,
            OverlayContentProgressObserver progressObserver,
            ChromeActivity activity) {
        mBaseManager = baseManager;

        mContentDelegate = contentDelegate;
        mProgressObserver = progressObserver;
        mActivity = activity;
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        mContent =  new OverlayPanelContent(mContentDelegate, mProgressObserver, mActivity) {
            @Override
            public void loadUrl(String url) {
                mLoadedUrl = url;
                mLoadedUrlCount++;
                super.loadUrl(url);
            }

            @Override
            public void removeLastHistoryEntry(String url, long timeInMs) {
                // Override to prevent call to native code.
            }
        };

        return mContent;
    }

    @VisibleForTesting
    public boolean didCreateContentView() {
        return mContent != null ? mContent.didCreateContentView() : false;
    }

    @Override
    public void startSearchTermResolutionRequest(String selection) {
        mLoadedUrl = null;
        mSearchTermRequested = selection;
    }

    @Override
    public void handleSearchTermResolutionResponse(boolean isNetworkUnavailable, int responseCode,
            String searchTerm, String displayText, String alternateTerm, boolean doPreventPreload,
            int selectionStartAdjust, int selectionEndAdjust) {
        mBaseManager.handleSearchTermResolutionResponse(isNetworkUnavailable, responseCode,
                searchTerm, displayText, alternateTerm, doPreventPreload, selectionStartAdjust,
                selectionEndAdjust);
    }

    @Override
    @Nullable public URL getBasePageUrl() {
        URL baseUrl = mBaseManager.getBasePageUrl();
        if (mShouldUseHttps && baseUrl != null) {
            try {
                return new URL(baseUrl.toString().replace("http://", "https://"));
            } catch (MalformedURLException e) {
                // TODO(donnd): Auto-generated catch block
                e.printStackTrace();
            }
        }
        return baseUrl;
    }

    /**
     * @return The search term requested, or {@code null} if no search term was requested.
     */
    @VisibleForTesting
    String getSearchTermRequested() {
        return mSearchTermRequested;
    }

    /**
     * @return the loaded search result page URL if any was requested.
     */
    @VisibleForTesting
    String getLoadedUrl() {
        return mLoadedUrl;
    }

    /**
     * @return The number of times we loaded a URL in the Content View.
     */
    @VisibleForTesting
    int loadedUrlCount() {
        return mLoadedUrlCount;
    }

    /**
     * Sets whether to return an HTTPS URL instead of HTTP, from {@link #getBasePageUrl}.
     */
    @VisibleForTesting
    void setShouldUseHttps(boolean setting) {
        mShouldUseHttps = setting;
    }

    /**
     * Resets the fake server's member data.
     */
    @VisibleForTesting
    void reset() {
        mContent = null;
        mLoadedUrl = null;
        mSearchTermRequested = null;
        mShouldUseHttps = false;
        mLoadedUrlCount = 0;
    }
}
