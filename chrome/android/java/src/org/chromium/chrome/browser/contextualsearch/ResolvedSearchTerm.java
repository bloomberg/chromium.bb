// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

/**
 * Encapsulates the results of a server Resolve request into a single immutable object.
 */
public class ResolvedSearchTerm {
    private final boolean mIsNetworkUnavailable;
    private final int mResponseCode;
    private final String mSearchTerm;
    private final String mDisplayText;
    private final String mAlternateTerm;
    private final String mMid;
    private final boolean mDoPreventPreload;
    private final int mSelectionStartAdjust;
    private final int mSelectionEndAdjust;
    private final String mContextLanguage;
    private final String mThumbnailUrl;
    private final String mCaption;
    @QuickActionCategory
    private final String mQuickActionUri;
    private final int mQuickActionCategory;
    private final long mLoggedEventId;
    private final String mSearchUrlFull;
    private final String mSearchUrlPreload;

    /**
     * Called in response to the
     * {@link ContextualSearchManager#nativeStartSearchTermResolutionRequest} method.
     * @param isNetworkUnavailable Indicates if the network is unavailable, in which case all other
     *        parameters should be ignored.
     * @param responseCode The HTTP response code. If the code is not OK, the query should be
     *        ignored.
     * @param searchTerm The term to use in our subsequent search.
     * @param displayText The text to display in our UX.
     * @param alternateTerm The alternate term to display on the results page.
     * @param mid the MID for an entity to use to trigger a Knowledge Panel, or an empty string.
     *        A MID is a unique identifier for an entity in the Search Knowledge Graph.
     * @param doPreventPreload Whether we should prevent preloading on this search.
     * @param selectionStartAdjust A positive number of characters that the start of the existing
     *        selection should be expanded by.
     * @param selectionEndAdjust A positive number of characters that the end of the existing
     *        selection should be expanded by.
     * @param contextLanguage The language of the original search term, or an empty string.
     * @param thumbnailUrl The URL of the thumbnail to display in our UX.
     * @param caption The caption to display.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     * @param loggedEventId The EventID logged by the server, which should be recorded and sent back
     *        to the server along with user action results in a subsequent request.
     * @param searchUrlFull The URL for the full search to present in the overlay, or empty.
     * @param searchUrlPreload The URL for the search to preload into the overlay, or empty.
     */
    ResolvedSearchTerm(boolean isNetworkUnavailable, int responseCode, final String searchTerm,
            final String displayText, final String alternateTerm, final String mid,
            boolean doPreventPreload, int selectionStartAdjust, int selectionEndAdjust,
            final String contextLanguage, final String thumbnailUrl, final String caption,
            final String quickActionUri, final int quickActionCategory, final long loggedEventId,
            final String searchUrlFull, final String searchUrlPreload) {
        mIsNetworkUnavailable = isNetworkUnavailable;
        mResponseCode = responseCode;
        mSearchTerm = searchTerm;
        mDisplayText = displayText;
        mAlternateTerm = alternateTerm;
        mMid = mid;
        mDoPreventPreload = doPreventPreload;
        mSelectionStartAdjust = selectionStartAdjust;
        mSelectionEndAdjust = selectionEndAdjust;
        mContextLanguage = contextLanguage;
        mThumbnailUrl = thumbnailUrl;
        mCaption = caption;
        mQuickActionUri = quickActionUri;
        mQuickActionCategory = quickActionCategory;
        mLoggedEventId = loggedEventId;
        mSearchUrlFull = searchUrlFull;
        mSearchUrlPreload = searchUrlPreload;
    }

    /**
     * Called in response to the
     * {@link ContextualSearchManager#nativeStartSearchTermResolutionRequest} method.
     * @param isNetworkUnavailable Indicates if the network is unavailable, in which case all other
     *        parameters should be ignored.
     * @param responseCode The HTTP response code. If the code is not OK, the query should be
     *        ignored.
     * @param searchTerm The term to use in our subsequent search.
     * @param displayText The text to display in our UX.
     * @param alternateTerm The alternate term to display on the results page.
     * @param doPreventPreload Whether we should prevent preloading on this search.
     */
    ResolvedSearchTerm(boolean isNetworkUnavailable, int responseCode, final String searchTerm,
            final String displayText, final String alternateTerm, boolean doPreventPreload) {
        this(isNetworkUnavailable, responseCode, searchTerm, displayText, alternateTerm, "",
                doPreventPreload, 0, 0, "", "", "", "", QuickActionCategory.NONE, 0L, "", "");
    }

    public boolean isNetworkUnavailable() {
        return mIsNetworkUnavailable;
    }

    public int responseCode() {
        return mResponseCode;
    }

    public String searchTerm() {
        return mSearchTerm;
    }

    public String displayText() {
        return mDisplayText;
    }

    public String alternateTerm() {
        return mAlternateTerm;
    }

    public String mid() {
        return mMid;
    }

    public boolean doPreventPreload() {
        return mDoPreventPreload;
    }

    public int selectionStartAdjust() {
        return mSelectionStartAdjust;
    }

    public int selectionEndAdjust() {
        return mSelectionEndAdjust;
    }

    public String contextLanguage() {
        return mContextLanguage;
    }

    public String thumbnailUrl() {
        return mThumbnailUrl;
    }

    public String caption() {
        return mCaption;
    }

    public String quickActionUri() {
        return mQuickActionUri;
    }

    public @QuickActionCategory int quickActionCategory() {
        return mQuickActionCategory;
    }

    public long loggedEventId() {
        return mLoggedEventId;
    }

    public String searchUrlFull() {
        return mSearchUrlFull;
    }

    public String searchUrlPreload() {
        return mSearchUrlPreload;
    }
}
