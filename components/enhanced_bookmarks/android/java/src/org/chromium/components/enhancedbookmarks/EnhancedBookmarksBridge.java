// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enhancedbookmarks;

import org.chromium.base.JNINamespace;
import org.chromium.components.bookmarks.BookmarkId;

/**
 * Access gate to C++ side enhanced bookmarks functionalities.
 */
@JNINamespace("enhanced_bookmarks::android")
public final class EnhancedBookmarksBridge {
    private long mNativeEnhancedBookmarksBridge;

    public EnhancedBookmarksBridge(long nativeBookmarkModel) {
        mNativeEnhancedBookmarksBridge = nativeInit(nativeBookmarkModel);
    }

    public void destroy() {
        assert mNativeEnhancedBookmarksBridge != 0;
        nativeDestroy(mNativeEnhancedBookmarksBridge);
        mNativeEnhancedBookmarksBridge = 0;
    }

    public String getBookmarkDescription(BookmarkId id) {
        return nativeGetBookmarkDescription(mNativeEnhancedBookmarksBridge, id.getId(),
                id.getType());
    }

    public void setBookmarkDescription(BookmarkId id, String description) {
        nativeSetBookmarkDescription(mNativeEnhancedBookmarksBridge, id.getId(), id.getType(),
                description);
    }

    private native long nativeInit(long bookmarkModelPointer);

    private native void nativeDestroy(long nativeEnhancedBookmarksBridge);

    private native String nativeGetBookmarkDescription(long nativeEnhancedBookmarksBridge, long id,
            int type);
    private native void nativeSetBookmarkDescription(long nativeEnhancedBookmarksBridge, long id,
            int type, String description);
}
