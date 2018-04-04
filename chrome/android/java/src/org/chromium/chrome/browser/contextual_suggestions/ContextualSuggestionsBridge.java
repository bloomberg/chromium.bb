// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to contextual suggestions.
 */
@JNINamespace("contextual_suggestions")
public class ContextualSuggestionsBridge {
    private long mNativeContextualSuggestionsBridge;

    /** Result of fetching contextual suggestions. */
    public static class ContextualSuggestionsResult {
        private String mPeekText;
        private List<ContextualSuggestionsCluster> mClusters = new ArrayList<>();

        ContextualSuggestionsResult(String peekText) {
            mPeekText = peekText;
        }

        /** Peek text to show in UI. */
        public String getPeekText() {
            return mPeekText;
        }

        /** Clusters of suggestions. */
        public List<ContextualSuggestionsCluster> getClusters() {
            return mClusters;
        }
    }

    /**
     * Creates a ContextualSuggestionsBridge for getting snippet data for the current user.
     *
     * @param profile Profile of the user that we will retrieve snippets for.
     */
    public ContextualSuggestionsBridge(Profile profile) {
        mNativeContextualSuggestionsBridge = nativeInit(profile);
    }

    /** Destroys the brige. */
    public void destroy() {
        assert mNativeContextualSuggestionsBridge != 0;
        nativeDestroy(mNativeContextualSuggestionsBridge);
        mNativeContextualSuggestionsBridge = 0;
    }

    /**
     * Fetches suggestions for a given URL.
     * @param url URL for which to fetch suggestions.
     * @param callback Callback used to return suggestions for a given URL.
     */
    public void fetchSuggestions(String url, Callback<ContextualSuggestionsResult> callback) {
        assert mNativeContextualSuggestionsBridge != 0;
        nativeFetchSuggestions(mNativeContextualSuggestionsBridge, url, callback);
    }

    /** Fetches a thumbnail for the suggestion. */
    public void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback) {
        assert mNativeContextualSuggestionsBridge != 0;
        nativeFetchSuggestionImage(
                mNativeContextualSuggestionsBridge, suggestion.mIdWithinCategory, callback);
    }

    /** Fetches a favicon for the suggestion. */
    public void fetchSuggestionFavicon(SnippetArticle suggestion, Callback<Bitmap> callback) {
        assert mNativeContextualSuggestionsBridge != 0;
        nativeFetchSuggestionFavicon(
                mNativeContextualSuggestionsBridge, suggestion.mIdWithinCategory, callback);
    }

    /** Requests the backend to clear state related to this bridge. */
    public void clearState() {
        assert mNativeContextualSuggestionsBridge != 0;
        nativeClearState(mNativeContextualSuggestionsBridge);
    }

    /**
     * Reports an event happening in the context of the current URL.
     *
     * @param webContents Web contents with the document for which event is reported.
     * @param eventId Id of the reported event.
     */
    public void reportEvent(WebContents webContents, int eventId) {
        assert mNativeContextualSuggestionsBridge != 0;
        assert webContents != null && !webContents.isDestroyed();

        nativeReportEvent(mNativeContextualSuggestionsBridge, webContents, eventId);
    }

    @CalledByNative
    private static ContextualSuggestionsResult createContextualSuggestionsResult(String peekText) {
        return new ContextualSuggestionsResult(peekText);
    }

    @CalledByNative
    private static void addNewClusterToResult(ContextualSuggestionsResult result, String title) {
        result.getClusters().add(new ContextualSuggestionsCluster(title));
    }

    @CalledByNative
    private static void addSuggestionToLastCluster(List<ContextualSuggestionsCluster> clusters,
            String id, String title, String publisher, String url) {
        assert clusters.size() > 0;
        clusters.get(clusters.size() - 1)
                .getSuggestions()
                .add(new SnippetArticle(KnownCategories.CONTEXTUAL, id, title, publisher, url,
                        /*publishTimestamp=*/0, /*score=*/0f, /*fetchTimestamp=*/0,
                        /*isVideoSuggestion=*/false, /*thumbnailDominantColor=*/0));
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeContextualSuggestionsBridge);
    private native void nativeFetchSuggestions(long nativeContextualSuggestionsBridge, String url,
            Callback<ContextualSuggestionsResult> callback);
    private native void nativeFetchSuggestionImage(
            long nativeContextualSuggestionsBridge, String suggestionId, Callback<Bitmap> callback);
    private native void nativeFetchSuggestionFavicon(
            long nativeContextualSuggestionsBridge, String suggestionId, Callback<Bitmap> callback);
    private native void nativeClearState(long nativeContextualSuggestionsBridge);
    private native void nativeReportEvent(
            long nativeContextualSuggestionsBridge, WebContents webContents, int eventId);
}
