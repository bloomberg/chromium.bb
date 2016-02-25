// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides access to the snippets to display on the NTP using the C++ NTP Snippets Service
 */
public class SnippetsBridge {
    private static final String TAG = "SnippetsBridge";

    private long mNativeSnippetsBridge;

    /**
     * An observer for notifying when new snippets are loaded
     */
    public interface SnippetsObserver {
        @CalledByNative("SnippetsObserver")
        public void onSnippetsAvailable(
                String[] titles, String[] urls, String[] thumbnailUrls, String[] previewText);
    }

    /**
     * Creates a SnippetsBridge for getting snippet data for the current user
     *
     * @param profile Profile of the user that we will retrieve snippets for.
     */
    public SnippetsBridge(Profile profile, final SnippetsObserver observer) {
        mNativeSnippetsBridge = nativeInit(profile);
        SnippetsObserver wrappedObserver = new SnippetsObserver() {
            @Override
            public void onSnippetsAvailable(
                    String[] titles, String[] urls, String[] thumbnailUrls, String[] previewText) {
                // Don't notify observer if we've already been destroyed.
                if (mNativeSnippetsBridge != 0) {
                    observer.onSnippetsAvailable(titles, urls, thumbnailUrls, previewText);
                }
            }
        };
        nativeSetObserver(mNativeSnippetsBridge, wrappedObserver);
    }

    void destroy() {
        assert mNativeSnippetsBridge != 0;
        nativeDestroy(mNativeSnippetsBridge);
        mNativeSnippetsBridge = 0;
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeNTPSnippetsBridge);
    private native void nativeSetObserver(long nativeNTPSnippetsBridge, SnippetsObserver observer);
}
