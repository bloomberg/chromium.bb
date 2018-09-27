// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import org.chromium.base.Callback;
import org.chromium.chrome.download.R;

/**
 * Helper class to handle asynchronously loading an image and displaying it when ready.  This class
 * supports both a 'waiting' drawable and an 'unavailable' drawable that will be used in the
 * foreground when the async image isn't present yet.
 */
public class AsyncImageView extends ForegroundRoundedCornerImageView {
    /** An interface that provides a way for this class to query for a {@link Drawable}. */
    @FunctionalInterface
    public interface Factory {
        /**
         * Called by {@link AsyncImageView} to start the process of asynchronously loading a
         * {@link Drawable}.
         *
         * @param consumer The {@link Callback} to notify with the result.
         * @param widthPx  The desired width of the {@link Drawable} if applicable (not required to
         * match).
         * @param heightPx The desired height of the {@link Drawable} if applicable (not required to
         * match).
         * @return         A {@link Runnable} that can be triggered to cancel the outstanding
         * request.
         */
        Runnable get(Callback<Drawable> consumer, int widthPx, int heightPx);
    }

    private final Drawable mUnavailableDrawable;
    private final Drawable mWaitingDrawable;

    /**
     * Used to handle synchronous responses to the callback in
     * {@link #setAsyncImageDrawable(Factory)}.
     */
    private boolean mWaitingForResponse;
    private Runnable mCancelable;

    /** Creates an {@link AsyncImageDrawable instance. */
    public AsyncImageView(Context context) {
        this(context, null, 0);
    }

    /** Creates an {@link AsyncImageDrawable instance. */
    public AsyncImageView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    /** Creates an {@link AsyncImageDrawable instance. */
    public AsyncImageView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        TypedArray types = attrs == null
                ? null
                : context.obtainStyledAttributes(attrs, R.styleable.AsyncImageView, 0, 0);

        mUnavailableDrawable = AutoAnimatorDrawable.wrap(
                UiUtils.getDrawable(context, types, R.styleable.AsyncImageView_unavailableSrc));
        mWaitingDrawable = AutoAnimatorDrawable.wrap(
                UiUtils.getDrawable(context, types, R.styleable.AsyncImageView_waitingSrc));

        if (types != null) types.recycle();
    }

    /**
     * Starts loading a {@link Drawable} from {@code factory}.  This will automatically clear out
     * any outstanding request state and start a new one.
     *
     * @param factory The {@link Factory} to use that will provide the {@link Drawable}.
     */
    public void setAsyncImageDrawable(Factory factory) {
        // This will clear out any outstanding request.
        setImageDrawable(null);

        setForegroundDrawableCompat(mWaitingDrawable);
        mCancelable = factory.get(this ::setAsyncImageDrawableResponse, getWidth(), getHeight());
        if (!mWaitingForResponse) mCancelable = null;
    }

    // RoundedCornerImageView implementation.
    @Override
    public void setImageDrawable(Drawable drawable) {
        // If we had an outstanding async request, cancel it because we're now setting the drawable
        // to something else.
        if (mWaitingForResponse) {
            if (mCancelable != null) mCancelable.run();
            mCancelable = null;
            mWaitingForResponse = false;
            setForegroundDrawableCompat(null);
        }

        super.setImageDrawable(drawable);
    }

    private void setAsyncImageDrawableResponse(Drawable drawable) {
        mCancelable = null;
        mWaitingForResponse = false;
        setForegroundDrawableCompat(drawable == null ? mUnavailableDrawable : null);
        setImageDrawable(drawable);
    }
}