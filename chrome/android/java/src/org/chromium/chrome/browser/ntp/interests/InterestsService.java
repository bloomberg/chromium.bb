// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.interests;

import android.text.TextUtils;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Retrieve the user's interests.
 */
public class InterestsService {

    private long mNativeInterestsService;

    /**
     * A user's interest.
     */
    public static class Interest {
        private final String mName;
        private final String mImageUrl;
        private final double mRelevance;

        public Interest(String name, String imageUrl, double relevance) {
            mName = name;
            mImageUrl = imageUrl;
            mRelevance = relevance;
        }

        @Override
        public String toString() {
            return "Interest [mName=" + mName + ", mImageUrl=" + mImageUrl + ", mRelevance="
                    + mRelevance + "]";
        }

        @Override
        public int hashCode() {
            final int prime = 31;
            int result = 1;
            result = prime * result + ((mImageUrl == null) ? 0 : mImageUrl.hashCode());
            result = prime * result + ((mName == null) ? 0 : mName.hashCode());
            long temp = Double.doubleToLongBits(mRelevance);
            return prime * result + (int) (temp ^ (temp >>> 32));
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) return true;
            if (!(obj instanceof Interest)) return false;
            return TextUtils.equals(toString(), obj.toString());
        }

        public String getName() {
            return mName;
        }

        public String getImageUrl() {
            return mImageUrl;
        }

        public double getRelevance() {
            return mRelevance;
        }
    }
    /**
     * Interface for receiving the interests of a user.
     */
    public interface GetInterestsCallback {
        /**
         * Callback method for fetching the interests of a user.
         *
         * @param interests The array of interests. Null if error.
         */
        @CalledByNative("GetInterestsCallback")
        public void onInterestsAvailable(Interest[] interests);
    }

    /**
     * InterestsService constructor requires a valid user profile object.
     *
     * @param profile The profile for which to fetch the interests
     */
    @VisibleForTesting
    public InterestsService(Profile profile) {
        mNativeInterestsService = nativeInit(profile);
    }

    /**
     * Cleans up the C++ side of this class. This instance must not be used after calling destroy().
     */
    public void destroy() {
        assert mNativeInterestsService != 0;
        nativeDestroy(mNativeInterestsService);
        mNativeInterestsService = 0;
    }

    @VisibleForTesting
    public void getInterests(final GetInterestsCallback callback) {
        nativeGetInterests(mNativeInterestsService, callback);
    }

    /*
     * Helper methods for the native part.
     */
    @CalledByNative
    private static Interest createInterest(String name, String imageUrl, double relevance) {
        return new Interest(name, imageUrl, relevance);
    }

    @CalledByNative
    private static Interest[] createInterestsArray(int size) {
        return new Interest[size];
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeInterestsService);
    private native void nativeGetInterests(
            long nativeInterestsService, GetInterestsCallback callback);
}
