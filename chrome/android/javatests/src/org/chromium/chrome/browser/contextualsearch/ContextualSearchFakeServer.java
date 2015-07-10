// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.base.VisibleForTesting;

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
class ContextualSearchFakeServer implements ContextualSearchNetworkCommunicator {

    private final ContextualSearchNetworkCommunicator mBaseManager;
    private String mLoadedUrl;
    private String mSearchTermRequested;
    private boolean mShouldUseHttps;
    private int mLoadedUrlCount;
    private boolean mIsSearchContentViewCreated;

    /**
     * Constructs a fake Contextual Search server that will callback to the given baseManager.
     * @param baseManager The manager to call back to for server responses.
     */
    @VisibleForTesting
    ContextualSearchFakeServer(ContextualSearchNetworkCommunicator baseManager) {
        mBaseManager = baseManager;
    }

    @Override
    public void startSearchTermResolutionRequest(String selection) {
        mLoadedUrl = null;
        mSearchTermRequested = selection;
    }

    @Override
    public void loadUrl(String url) {
        mLoadedUrl = url;
        mLoadedUrlCount++;
        // This will not actually load a URL because no Search Content View will be created
        // when under test -- see comments in createNewSearchContentView.
        mBaseManager.loadUrl(url);
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

    @Override
    public void handleDidNavigateMainFrame(String url, int httpResultCode) {
        mBaseManager.handleDidNavigateMainFrame(url, httpResultCode);
    }

    @Override
    public void createNewSearchContentView() {
        mIsSearchContentViewCreated = true;
        // Don't call the super method because that will cause loadUrl to make a live request!
        // This method is only called by loadUrl, which will subseqently check if the CV was
        // successfully created before issuing the search request.
        // TODO(donnd): This is brittle, improve!  E.g. make live requests to a local server.
    }

    @Override
    public void destroySearchContentView() {
        mIsSearchContentViewCreated = false;
        mBaseManager.destroySearchContentView();
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
     * @return Whether we tried to create the Search Content View.
     */
    @VisibleForTesting
    boolean isSearchContentViewCreated() {
        return mIsSearchContentViewCreated;
    }

    /**
     * Resets the fake server's member data.
     */
    @VisibleForTesting
    void reset() {
        mLoadedUrl = null;
        mSearchTermRequested = null;
        mShouldUseHttps = false;
        mLoadedUrlCount = 0;
        mIsSearchContentViewCreated = false;
    }
}
