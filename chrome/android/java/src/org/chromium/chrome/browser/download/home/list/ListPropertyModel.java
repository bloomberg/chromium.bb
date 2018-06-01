// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.modelutil.PropertyObservable;
import org.chromium.components.offline_items_collection.ContentId;

/**
 * A {@link PropertyObservable} that contains two types of properties for the download manager:
 * (1) A set of properties that act directly on the list view itself.
 * (2) A set of properties that are effectively shared across all list items like callbacks.
 */
class ListPropertyModel extends PropertyObservable<ListPropertyModel.PropertyKey> {
    static class PropertyKey {
        // RecyclerView general properties.
        static final PropertyKey ITEM_ANIMATION_DURATION_MS = new PropertyKey();

        // RecyclerView shared list item properties.
        static final PropertyKey CALLBACK_OPEN = new PropertyKey();
        static final PropertyKey CALLBACK_PAUSE = new PropertyKey();
        static final PropertyKey CALLBACK_RESUME = new PropertyKey();
        static final PropertyKey CALLBACK_CANCEL = new PropertyKey();
        static final PropertyKey CALLBACK_SHARE = new PropertyKey();
        static final PropertyKey CALLBACK_REMOVE = new PropertyKey();

        private PropertyKey() {}
    }

    private long mItemAnimationDurationMs;
    private Callback<ContentId> mOpenCallback;
    private Callback<ContentId> mPauseCallback;
    private Callback<ContentId> mResumeCallback;
    private Callback<ContentId> mCancelCallback;
    private Callback<ContentId> mShareCallback;
    private Callback<ContentId> mRemoveCallback;

    /** Sets the duration for add and remove animations. */
    public void setItemAnimationDurationMs(long durationMs) {
        if (mItemAnimationDurationMs == durationMs) return;
        mItemAnimationDurationMs = durationMs;
        notifyPropertyChanged(PropertyKey.ITEM_ANIMATION_DURATION_MS);
    }

    /** @return The duration in milliseconds for add and remove animations. */
    public long getItemAnimationDurationMs() {
        return mItemAnimationDurationMs;
    }

    /** Sets the callback for when a UI action should open a {@link ContentId}. */
    public void setOpenCallback(Callback<ContentId> callback) {
        if (mOpenCallback == callback) return;
        mOpenCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_OPEN);
    }

    /** @return The callback to trigger when a UI action opens a {@link ContentId}. */
    public Callback<ContentId> getOpenCallback() {
        return mOpenCallback;
    }

    /** Sets the callback for when a UI action should pause a {@link ContentId}. */
    public void setPauseCallback(Callback<ContentId> callback) {
        if (mPauseCallback == callback) return;
        mPauseCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_PAUSE);
    }

    /** @return The callback to trigger when a UI action pauses a {@link ContentId}. */
    public Callback<ContentId> getPauseCallback() {
        return mPauseCallback;
    }

    /** Sets the callback for when a UI action should resume a {@link ContentId}. */
    public void setResumeCallback(Callback<ContentId> callback) {
        if (mRemoveCallback == callback) return;
        mResumeCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_RESUME);
    }

    /** @return The callback to trigger when a UI action resumes a {@link ContentId}. */
    public Callback<ContentId> getResumeCallback() {
        return mResumeCallback;
    }

    /** Sets the callback for when a UI action should cancel a {@link ContentId}. */
    public void setCancelCallback(Callback<ContentId> callback) {
        if (mCancelCallback == callback) return;
        mCancelCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_CANCEL);
    }

    /** @return The callback to trigger when a UI action cancels a {@link ContentId}. */
    public Callback<ContentId> getCancelCallback() {
        return mCancelCallback;
    }

    /** Sets the callback for when a UI action should share a {@link ContentId}. */
    public void setShareCallback(Callback<ContentId> callback) {
        if (mShareCallback == callback) return;
        mShareCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_SHARE);
    }

    /** @return The callback to trigger when a UI action shares a {@link ContentId}. */
    public Callback<ContentId> getShareCallback() {
        return mShareCallback;
    }

    /** Sets the callback for when a UI action should remove a {@link ContentId}. */
    public void setRemoveCallback(Callback<ContentId> callback) {
        if (mRemoveCallback == callback) return;
        mRemoveCallback = callback;
        notifyPropertyChanged(PropertyKey.CALLBACK_REMOVE);
    }

    /** @return The callback to trigger when a UI action removes a {@link ContentId}. */
    public Callback<ContentId> getRemoveCallback() {
        return mRemoveCallback;
    }
}