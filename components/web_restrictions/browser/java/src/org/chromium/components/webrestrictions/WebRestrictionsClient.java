// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webrestrictions;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This class acts as an interface that talks to the content provider which actually implements the
 * web restriction provider.
 */
@JNINamespace("web_restrictions")
public class WebRestrictionsClient {
    static class ShouldProceedResult {
        private final boolean mShouldProceed;
        private final String mErrorPage;

        ShouldProceedResult(boolean shouldProceed, String errorPage) {
            mShouldProceed = shouldProceed;
            mErrorPage = errorPage;
        }

        @CalledByNative("ShouldProceedResult")
        boolean shouldProceed() {
            return mShouldProceed;
        }

        @CalledByNative("ShouldProceedResult")
        String getErrorPage() {
            return mErrorPage;
        }
    }

    // Handle to allow mocking for C++ unit testing
    private static WebRestrictionsClient sMock;

    private Uri mQueryUri;
    private Uri mRequestUri;
    private ContentObserver mContentObserver;
    private ContentResolver mContentResolver;

    /**
     * Start the web restriction provider and setup the content resolver.
     */
    WebRestrictionsClient() {}

    void init(String authority, final long nativeProvider) {
        assert !TextUtils.isEmpty(authority);
        Uri baseUri = new Uri.Builder().scheme("content").authority(authority).build();
        mQueryUri = Uri.withAppendedPath(baseUri, "authorized");
        mRequestUri = Uri.withAppendedPath(baseUri, "requested");
        mContentResolver = ContextUtils.getApplicationContext().getContentResolver();
        mContentObserver = new ContentObserver(null) {
            @Override
            public void onChange(boolean selfChange) {
                onChange(selfChange, null);
            }

            @Override
            public void onChange(boolean selfChange, Uri uri) {
                nativeNotifyWebRestrictionsChanged(nativeProvider);
            }
        };
        mContentResolver.registerContentObserver(baseUri, true, mContentObserver);
    }

    /**
     * Simple helper method to expose the constructor over JNI.
     */
    @CalledByNative
    private static WebRestrictionsClient create(String authority, long nativeProvider) {
        // Check for a mock for testing.
        WebRestrictionsClient client = sMock == null ? new WebRestrictionsClient() : sMock;
        client.init(authority, nativeProvider);
        return client;
    }

    /**
     * @return whether the web restriction provider supports requesting access for a blocked url.
     */
    @CalledByNative
    boolean supportsRequest() {
        return mContentResolver != null && mContentResolver.getType(mRequestUri) != null;
    }

    /**
     * Called when the ContentResolverWebRestrictionsProvider is about to be destroyed.
     */
    @CalledByNative
    void onDestroy() {
        mContentResolver.unregisterContentObserver(mContentObserver);
    }

    /**
     * Whether we can proceed with loading the {@code url}.
     * In case the url is not to be loaded, the web restriction provider can return an optional
     * error page to show instead.
     */
    @CalledByNative
    ShouldProceedResult shouldProceed(final String url) {
        String select = String.format("url = '%s'", url);
        Cursor result = mContentResolver.query(mQueryUri, null, select, null, null);
        boolean shouldProceed = result == null || result.getInt(0) > 0;
        String errorPage = shouldProceed ? null : result.getString(1);
        return new ShouldProceedResult(shouldProceed, errorPage);
    }

    /**
     * Request permission to load the {@code url}.
     * The web restriction provider returns a {@code boolean} variable indicating whether it was
     * able to forward the request to the appropriate authority who can approve it.
     */
    @CalledByNative
    boolean requestPermission(final String url) {
        ContentValues values = new ContentValues(1);
        values.put("url", url);
        return mContentResolver.insert(mRequestUri, values) != null;
    }

    native void nativeNotifyWebRestrictionsChanged(long ptrProvider);

    /**
     * Allow a mock for of the class for C++ unit testing.
     * @param mock the mock
     */
    protected static void registerMockForTesting(WebRestrictionsClient mock) {
        sMock = mock;
    }
}
