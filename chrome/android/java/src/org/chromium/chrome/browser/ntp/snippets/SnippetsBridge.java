// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the snippets to display on the NTP using the C++ NTP Snippets Service
 */
public class SnippetsBridge implements SnippetsSource {
    private static final String TAG = "SnippetsBridge";

    private long mNativeSnippetsBridge;
    private SnippetsObserver mObserver;

    public static boolean isCategoryStatusAvailable(int status) {
        // Note: This code is duplicated in content_suggestions_category_status.cc.
        return status == CategoryStatus.AVAILABLE_LOADING || status == CategoryStatus.AVAILABLE;
    }

    public static boolean isCategoryStatusInitOrAvailable(int status) {
        // Note: This code is duplicated in content_suggestions_category_status.cc.
        return status == CategoryStatus.INITIALIZING || isCategoryStatusAvailable(status);
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

    /**
     * Tells the native service to discard a snippet. It will be removed from the native side
     * storage and will also be discarded from subsequent fetch results.
     *
     * @param snippet Snippet to discard.
     */
    @Override
    public void discardSnippet(SnippetArticleListItem snippet) {
        assert mNativeSnippetsBridge != 0;
        nativeDiscardSnippet(mNativeSnippetsBridge, snippet.mId);
    }

    /**
     * Fetches the thumbnail image for a snippet.
     */
    @Override
    public void fetchSnippetImage(SnippetArticleListItem snippet, Callback<Bitmap> callback) {
        nativeFetchImage(mNativeSnippetsBridge, snippet.mId, callback);
    }

    /**
     * Checks whether a snippet has been visited by querying the history for the snippet's URL.
     */
    @Override
    public void getSnippedVisited(SnippetArticleListItem snippet, Callback<Boolean> callback) {
        assert mNativeSnippetsBridge != 0;
        nativeSnippetVisited(mNativeSnippetsBridge, callback, snippet.mUrl);
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
    public void setObserver(SnippetsObserver observer) {
        assert mObserver == null || mObserver == observer;

        mObserver = observer;
        nativeSetObserver(mNativeSnippetsBridge, observer == null ? null : this);
    }

    @Override
    public int getCategoryStatus() {
        assert mNativeSnippetsBridge != 0;
        return nativeGetCategoryStatus(mNativeSnippetsBridge);
    }

    @CalledByNative
    private void onSnippetsAvailable(String[] ids, String[] titles, String[] urls, String[] ampUrls,
            String[] previewText, long[] timestamps, String[] publishers, float[] scores) {
        assert mNativeSnippetsBridge != 0;
        assert mObserver != null;

        List<SnippetArticleListItem> newSnippets = new ArrayList<>(ids.length);
        for (int i = 0; i < ids.length; i++) {
            newSnippets.add(new SnippetArticleListItem(ids[i], titles[i], publishers[i],
                    previewText[i], urls[i], ampUrls[i], timestamps[i], scores[i], i));
        }

        mObserver.onSnippetsReceived(newSnippets);
    }

    @CalledByNative
    private void onCategoryStatusChanged(int newStatus) {
        if (mObserver != null) mObserver.onCategoryStatusChanged(newStatus);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeNTPSnippetsBridge);
    private static native void nativeFetchSnippets(boolean forceRequest);
    private static native void nativeRescheduleFetching();
    private native void nativeDiscardSnippet(long nativeNTPSnippetsBridge, String snippetId);
    private native void nativeSetObserver(long nativeNTPSnippetsBridge, SnippetsBridge bridge);
    private static native void nativeSnippetVisited(long nativeNTPSnippetsBridge,
            Callback<Boolean> callback, String url);
    private native void nativeFetchImage(
            long nativeNTPSnippetsBridge, String snippetId, Callback<Bitmap> callback);
    private native int nativeGetCategoryStatus(long nativeNTPSnippetsBridge);
}
