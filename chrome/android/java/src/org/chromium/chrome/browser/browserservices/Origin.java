// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;

import org.chromium.net.GURLUtils;

/**
 * A class to canonically represent a web origin in Java. It requires the native library to be
 * loaded as it uses {@link GURLUtils#getOrigin}.
 */
public class Origin {
    private final String mOrigin;

    /**
     * Constructs a canonical Origin from a String.
     */
    public Origin(String uri) {
        mOrigin = GURLUtils.getOrigin(uri);
    }

    /**
     * Constructs a canonical Origin from an Uri.
     */
    public Origin(Uri uri) {
        this(uri.toString());
    }

    /**
     * Returns a Uri representing this Origin.
     */
    public Uri uri() {
        return Uri.parse(mOrigin);
    }

    @Override
    public int hashCode() {
        return mOrigin.hashCode();
    }

    @Override
    public String toString() {
        return mOrigin;
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) return true;
        if (other == null || getClass() != other.getClass()) return false;
        return mOrigin.equals(((Origin) other).mOrigin);
    }
}
