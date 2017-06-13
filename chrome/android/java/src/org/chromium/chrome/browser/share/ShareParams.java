// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.chromium.chrome.browser.share.ShareHelper.TargetChosenCallback;

/**
 * A container object for passing share parameters to {@link ShareHelper}.
 */
public class ShareParams {
    /**
     * Whether it should share directly with the activity that was most recently used to share.
     * If false, the share selection will be saved.
     */
    private final boolean mShareDirectly;

    /** Whether to save the chosen activity for future direct sharing. */
    private final boolean mSaveLastUsed;

    /** The activity that is used to access package manager. */
    private final Activity mActivity;

    /** The title of the page to be shared. */
    private final String mTitle;

    /**
     * The text to be shared. If both |text| and |url| are supplied, they are concatenated with a
     * space.
     */
    private final String mText;

    /** The URL of the page to be shared. */
    private final String mUrl;

    /** The Uri to the offline MHTML file to be shared. */
    private final Uri mOfflineUri;

    /** The Uri of the screenshot of the page to be shared. */
    private final Uri mScreenshotUri;

    /**
     * Optional callback to be called when user makes a choice. Will not be called if receiving a
     * response when the user makes a choice is not supported (on older Android versions).
     */
    private TargetChosenCallback mCallback;

    private ShareParams(boolean shareDirectly, boolean saveLastUsed, Activity activity,
            String title, String text, String url, @Nullable Uri offlineUri,
            @Nullable Uri screenshotUri, @Nullable TargetChosenCallback callback) {
        mShareDirectly = shareDirectly;
        mSaveLastUsed = saveLastUsed;
        mActivity = activity;
        mTitle = title;
        mText = text;
        mUrl = url;
        mOfflineUri = offlineUri;
        mScreenshotUri = screenshotUri;
        mCallback = callback;
    }

    /**
     * @return Whether it should share directly with the activity that was most recently used to
     * share.
     */
    public boolean shareDirectly() {
        return mShareDirectly;
    }

    /**
     * @return Whether to save the chosen activity for future direct sharing.
     */
    public boolean saveLastUsed() {
        return mSaveLastUsed;
    }

    /**
     * @return The activity that is used to access package manager.
     */
    public Activity getActivity() {
        return mActivity;
    }

    /**
     * @return The title of the page to be shared.
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @return The text to be shared.
     */
    public String getText() {
        return mText;
    }

    /**
     * @return The URL of the page to be shared.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * @return The Uri to the offline MHTML file to be shared.
     */
    @Nullable
    public Uri getOfflineUri() {
        return mOfflineUri;
    }

    /**
     * @return The Uri of the screenshot of the page to be shared.
     */
    @Nullable
    public Uri getScreenshotUri() {
        return mScreenshotUri;
    }

    /**
     * @return The callback to be called when user makes a choice.
     */
    @Nullable
    public TargetChosenCallback getCallback() {
        return mCallback;
    }

    /** The builder for {@link ShareParams} objects. */
    public static class Builder {
        private boolean mShareDirectly;
        private boolean mSaveLastUsed;
        private Activity mActivity;
        private String mTitle;
        private String mText;
        private String mUrl;
        private Uri mOfflineUri;
        private Uri mScreenshotUri;
        private TargetChosenCallback mCallback;

        public Builder(@NonNull Activity activity, @NonNull String title, @NonNull String url) {
            mActivity = activity;
            mUrl = url;
            mTitle = title;
        }

        /**
         * Sets the text to be shared.
         */
        public Builder setText(@NonNull String text) {
            mText = text;
            return this;
        }

        /**
         * Sets whether it should share directly with the activity that was most recently used to
         * share.
         */
        public Builder setShareDirectly(boolean shareDirectly) {
            mShareDirectly = shareDirectly;
            return this;
        }

        /**
         * Sets whether to save the chosen activity for future direct sharing.
         */
        public Builder setSaveLastUsed(boolean saveLastUsed) {
            mSaveLastUsed = saveLastUsed;
            return this;
        }

        /**
         * Sets the URL of the page to be shared.
         */
        public Builder setUrl(@NonNull String url) {
            mUrl = url;
            return this;
        }

        /**
         * Sets the Uri of the offline MHTML file to be shared.
         */
        public Builder setOfflineUri(@Nullable Uri offlineUri) {
            mOfflineUri = offlineUri;
            return this;
        }

        /**
         * Sets the Uri of the screenshot of the page to be shared.
         */
        public Builder setScreenshotUri(@Nullable Uri screenshotUri) {
            mScreenshotUri = screenshotUri;
            return this;
        }

        /**
         * Sets the callback to be called when user makes a choice.
         */
        public Builder setCallback(@Nullable TargetChosenCallback callback) {
            mCallback = callback;
            return this;
        }

        /** @return A fully constructed {@link ShareParams} object. */
        public ShareParams build() {
            return new ShareParams(mShareDirectly, mSaveLastUsed, mActivity, mTitle, mText, mUrl,
                    mOfflineUri, mScreenshotUri, mCallback);
        }
    }
}
