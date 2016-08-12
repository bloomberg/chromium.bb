// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the snippets to display on the NTP using the C++ ContentSuggestionsService.
 */
public class SnippetsBridge implements SuggestionsSource {
    private static final String TAG = "SnippetsBridge";

    private long mNativeSnippetsBridge;
    private SuggestionsSource.Observer mObserver;

    public static boolean isCategoryStatusAvailable(@CategoryStatusEnum int status) {
        // Note: This code is duplicated in content_suggestions_category_status.cc.
        return status == CategoryStatus.AVAILABLE_LOADING || status == CategoryStatus.AVAILABLE;
    }

    public static boolean isCategoryStatusInitOrAvailable(@CategoryStatusEnum int status) {
        // Note: This code is duplicated in content_suggestions_category_status.cc.
        return status == CategoryStatus.INITIALIZING || isCategoryStatusAvailable(status);
    }

    public static boolean isCategoryLoading(@CategoryStatusEnum int status) {
        return status == CategoryStatus.AVAILABLE_LOADING || status == CategoryStatus.INITIALIZING;
    }

    /**
     * Creates a SnippetsBridge for getting snippet data for the current user.
     *
     * @param profile Profile of the user that we will retrieve snippets for.
     */
    public SnippetsBridge(Profile profile) {
        mNativeSnippetsBridge = nativeInit(profile);
    }

    /**
     * Destroys the native service and unregisters observers. This object can't be reused to
     * communicate with any native service and should be discarded.
     */
    public void destroy() {
        assert mNativeSnippetsBridge != 0;
        nativeDestroy(mNativeSnippetsBridge);
        mNativeSnippetsBridge = 0;
        mObserver = null;
    }

    /**
     * Fetches new snippets.
     */
    public static void fetchSnippets(boolean forceRequest) {
        nativeFetchSnippets(forceRequest);
    }

    /**
     * Reschedules the fetching of snippets. Used to support different fetching intervals for
     * different times of day.
     */
    public static void rescheduleFetching() {
        nativeRescheduleFetching();
    }

    @Override
    public int[] getCategories() {
        assert mNativeSnippetsBridge != 0;
        return nativeGetCategories(mNativeSnippetsBridge);
    }

    @Override
    @CategoryStatusEnum
    public int getCategoryStatus(int category) {
        assert mNativeSnippetsBridge != 0;
        return nativeGetCategoryStatus(mNativeSnippetsBridge, category);
    }

    @Override
    public SuggestionsCategoryInfo getCategoryInfo(int category) {
        assert mNativeSnippetsBridge != 0;
        return nativeGetCategoryInfo(mNativeSnippetsBridge, category);
    }

    @Override
    public List<SnippetArticleListItem> getSuggestionsForCategory(int category) {
        assert mNativeSnippetsBridge != 0;
        return nativeGetSuggestionsForCategory(mNativeSnippetsBridge, category);
    }

    @Override
    public void fetchSuggestionImage(SnippetArticleListItem suggestion, Callback<Bitmap> callback) {
        assert mNativeSnippetsBridge != 0;
        nativeFetchSuggestionImage(mNativeSnippetsBridge, suggestion.mId, callback);
    }

    @Override
    public void dismissSuggestion(SnippetArticleListItem suggestion) {
        assert mNativeSnippetsBridge != 0;
        nativeDismissSuggestion(mNativeSnippetsBridge, suggestion.mId);
    }

    @Override
    public void getSuggestionVisited(
            SnippetArticleListItem suggestion, Callback<Boolean> callback) {
        assert mNativeSnippetsBridge != 0;
        nativeGetURLVisited(mNativeSnippetsBridge, callback, suggestion.mUrl);
    }

    /**
     * Sets the recipient for the fetched snippets.
     *
     * An observer needs to be set before the native code attempts to transmit snippets them to
     * java. Upon registration, the observer will be notified of already fetched snippets.
     *
     * @param observer object to notify when snippets are received, or {@code null} if we want to
     *                 stop observing.
     */
    @Override
    public void setObserver(SuggestionsSource.Observer observer) {
        assert mObserver == null || mObserver == observer;

        mObserver = observer;
        nativeSetObserver(mNativeSnippetsBridge, observer == null ? null : this);
    }

    @CalledByNative
    private static List<SnippetArticleListItem> createSuggestionList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addSuggestion(List<SnippetArticleListItem> suggestions, String id,
            String title, String publisher, String previewText, String url, String ampUrl,
            long timestamp, float score) {
        int position = suggestions.size();
        suggestions.add(new SnippetArticleListItem(id, title, publisher, previewText, url, ampUrl,
                timestamp, score, position));
    }

    @CalledByNative
    private static SuggestionsCategoryInfo createSuggestionsCategoryInfo(
            String title, int cardLayout, boolean hasMoreButton) {
        return new SuggestionsCategoryInfo(title, cardLayout, hasMoreButton);
    }

    @CalledByNative
    private void onNewSuggestions(/* @CategoryInt */ int category) {
        assert mNativeSnippetsBridge != 0;
        assert mObserver != null;
        mObserver.onNewSuggestions(category);
    }

    @CalledByNative
    private void onCategoryStatusChanged(/* @CategoryInt */ int category, /* @CategoryStatusEnum */
            int newStatus) {
        if (mObserver != null) mObserver.onCategoryStatusChanged(category, newStatus);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeNTPSnippetsBridge);
    private static native void nativeFetchSnippets(boolean forceRequest);
    private static native void nativeRescheduleFetching();
    private native int[] nativeGetCategories(long nativeNTPSnippetsBridge);
    private native int nativeGetCategoryStatus(long nativeNTPSnippetsBridge, int category);
    private native SuggestionsCategoryInfo nativeGetCategoryInfo(
            long nativeNTPSnippetsBridge, int category);
    private native List<SnippetArticleListItem> nativeGetSuggestionsForCategory(
            long nativeNTPSnippetsBridge, int category);
    private native void nativeFetchSuggestionImage(
            long nativeNTPSnippetsBridge, String suggestionId, Callback<Bitmap> callback);
    private native void nativeDismissSuggestion(long nativeNTPSnippetsBridge, String suggestionId);
    private native void nativeGetURLVisited(
            long nativeNTPSnippetsBridge, Callback<Boolean> callback, String url);
    private native void nativeSetObserver(long nativeNTPSnippetsBridge, SnippetsBridge bridge);
}
