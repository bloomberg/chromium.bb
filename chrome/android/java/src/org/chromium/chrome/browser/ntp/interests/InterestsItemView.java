// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.interests;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.media.ThumbnailUtils;
import android.os.AsyncTask;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.support.v7.widget.AppCompatTextView;
import android.text.TextUtils;
import android.util.LruCache;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.interests.InterestsPage.InterestsClickListener;
import org.chromium.chrome.browser.ntp.interests.InterestsService.Interest;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;

/**
 * Displays the interest name along with an image of it. This item can be clicked.
 */
class InterestsItemView extends AppCompatTextView implements OnClickListener {

    private static final String TAG = "InterestsItemView";

    // Color codes to provide a variety of background colours for the letter tiles.
    // TODO(peconn): Move this, along with getTileColor into a separate class.
    private static final int[] COLORS = {0xfff16364, 0xfff58559, 0xfff9a43e, 0xffe4c62e, 0xff67bf74,
            0xff59a2be, 0xff2093cd, 0xffad62a7};

    /**
     * Drawing-related values that can be shared between instances of InterestsItemView.
     */
    static final class DrawingData {

        private final int mPadding;
        private final int mMinHeight;
        private final int mImageSize;
        private final int mTextSize;
        private final int mImageTextSize;

        private final RoundedIconGenerator mIconGenerator;

        /**
         * Initialize shared values used for drawing the image.
         *
         * @param context The view context in which the InterestsItemView will be drawn.
         */
        DrawingData(Context context) {
            Resources res = context.getResources();
            mPadding = res.getDimensionPixelOffset(R.dimen.ntp_list_item_padding);
            mMinHeight = res.getDimensionPixelSize(R.dimen.ntp_interest_item_min_height);
            mTextSize = res.getDimensionPixelSize(R.dimen.ntp_interest_item_text_size);
            mImageSize = res.getDimensionPixelSize(R.dimen.ntp_interest_item_image_size);
            mImageTextSize = res.getDimensionPixelSize(R.dimen.ntp_interest_item_image_text_size);
            mIconGenerator = new RoundedIconGenerator(
                mImageSize, mImageSize, mImageSize / 2, Color.GRAY, mImageTextSize);
        }
    }

    private Interest mInterest;

    private final Context mContext;
    private final DrawingData mDrawingData;
    private final LruCache<String, Promise<Drawable>> mImageCache;
    private final InterestsClickListener mListener;

    /**
     * @param context The view context in which this item will be shown.
     * @param interest The interest to display.
     * @param listener Callback object for when a view is pressed.
     * @param imageCache A cache to store downloaded images.
     * @param drawingData Information about the view size.
     */
    InterestsItemView(Context context, Interest interest, InterestsClickListener listener,
            LruCache<String, Promise<Drawable>> imageCache, DrawingData drawingData) {
        super(context);

        mContext = context;
        mListener = listener;
        mImageCache = imageCache;
        mDrawingData = drawingData;

        setTextSize(TypedValue.COMPLEX_UNIT_PX, mDrawingData.mTextSize);
        setMinimumHeight(mDrawingData.mMinHeight);
        setGravity(Gravity.CENTER);
        setTextAlignment(View.TEXT_ALIGNMENT_CENTER);

        setOnClickListener(this);

        reset(interest);
    }

    /**
     * Resets the view contents so that it can be reused in the grid view.
     *
     * @param interest The interest to display.
     */
    public void reset(Interest interest) {
        // Reset Drawable state so ripples don't continue when the View is reused.
        jumpDrawablesToCurrentState();

        // Exit early if this View is already displaying the Interest given.
        if (mInterest != null
                && TextUtils.equals(interest.getName(), mInterest.getName())
                && TextUtils.equals(interest.getImageUrl(), mInterest.getImageUrl())) {
            mInterest = interest;
            return;
        }

        mInterest = interest;

        setText(mInterest.getName());

        Promise<Drawable> promise = mImageCache.get(mInterest.getImageUrl());
        if (promise == null) {
            promise = new Promise<Drawable>();
            mImageCache.put(mInterest.getImageUrl(),
                    ImageDownloadTask.start(mInterest.getImageUrl(), getResources()));
        }

        // If not fulfilled, display a letter tile while waiting for the image to download.
        if (!promise.isFulfilled()) {
            mDrawingData.mIconGenerator.setBackgroundColor(getTileColor(mInterest.getName()));
            setImage(new BitmapDrawable(mContext.getResources(),
                    mDrawingData.mIconGenerator.generateIconForText(mInterest.getName())));
        }

        // Once fulfilled, display the image (unless this view has been repurposed for a different
        // Interest).
        final String urlWhenDownloadRequested = mInterest.getImageUrl();
        promise.then(new Callback<Drawable>() {
            @Override
            public void onResult(Drawable drawable) {
                if (drawable == null) return;
                if (TextUtils.equals(mInterest.getImageUrl(), urlWhenDownloadRequested)) return;

                setImage(drawable);
            }
        });
    }

    /**
     * @return The image URL for the interest.
     */
    public String getImageUrl() {
        return mInterest.getImageUrl();
    }

    /**
     * @return The name of the interest.
     */
    public String getName() {
        return mInterest.getName();
    }

    private void setImage(Drawable image) {
        image.setBounds(new Rect(0, 0, mDrawingData.mImageSize, mDrawingData.mImageSize));
        setCompoundDrawables(null, image, null, null);
    }

    private int getTileColor(String str) {
        // Rough copy of LetterTileDrawable.pickColor.
        // TODO(peconn): Move this to a more general class.
        return COLORS[Math.abs(str.hashCode() % COLORS.length)];
    }

    @Override
    public void onClick(View v) {
        mListener.onInterestClicked(getName());
    }

    /*
     * An AsyncTask that downloads an image and formats it.
     */
    private static class ImageDownloadTask extends AsyncTask<Void, Void, Drawable> {

        public static Promise<Drawable> start(String url, Resources resources) {
            Promise<Drawable> promise = new Promise<Drawable>();
            new ImageDownloadTask(url, promise, resources).executeOnExecutor(THREAD_POOL_EXECUTOR);
            return promise;
        }

        private final String mUrl;
        private final Resources mResources;
        private Promise<Drawable> mPromise;

        private ImageDownloadTask(String url, Promise<Drawable> promise, Resources resources) {
            mUrl = url;
            mPromise = promise;
            mResources = resources;
        }

        @Override
        protected Drawable doInBackground(Void... voids) {
            // This is run on a background thread.
            try {
                // TODO(peconn): Replace this with something from the C++ Chrome stack.
                URL imageUrl = new URL(mUrl);
                InputStream in = imageUrl.openStream();

                Bitmap raw = BitmapFactory.decodeStream(in);
                int dimension = Math.min(raw.getHeight(), raw.getWidth());
                RoundedBitmapDrawable img = RoundedBitmapDrawableFactory.create(mResources,
                        ThumbnailUtils.extractThumbnail(raw, dimension, dimension));
                img.setCircular(true);

                return img;
            } catch (IOException e) {
                Log.e(TAG, "Error downloading image: " + e.toString());
            }
            return null;
        }

        @Override
        protected void onPostExecute(Drawable image) {
            RecordHistogram.recordBooleanHistogram(
                    "NewTabPage.Interests.ImageDownloadSuccess", image != null);
            mPromise.fulfill(image);
        }
    }
}
