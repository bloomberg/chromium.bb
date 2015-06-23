// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.ColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksBridge.SalientImageCallback;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksModel;
import org.chromium.chrome.browser.widget.CustomShapeDrawable.CircularDrawable;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * Container view for image of a bookmark. If there is no appropriate image stored in server, we
 * show pure color as placeholder; otherwise, real image will be loaded asynchronously with animated
 * transition.
 * <p>
 * Note this image is actually a FrameLayout that contains images. Therefore some xml attributes for
 * ImageView are not applicable to this class.
 */
public class EnhancedBookmarkSalientImageView extends FrameLayout {

    /**
     * Base class for different kinds of drawables to be used to define shapes of salient images.
     */
    public abstract static class BaseSalientDrawable extends Drawable {
        protected Paint mPaint = new Paint();
        private int mBitmapWidth;
        private int mBitmapHeight;
        private boolean mShouldScale = true;

        @Override
        protected void onBoundsChange(Rect bounds) {
            // Subclasses must override this function. It should first set up correct boundary of
            // mRect and then call super.onBoundsChange().
            if (mBitmapWidth > 0 && mBitmapHeight > 0) {
                BitmapShader shader = (BitmapShader) mPaint.getShader();

                float scale = 1.0f;
                if (mShouldScale) {
                    scale = Math.max((float) bounds.width() / mBitmapWidth,
                            (float) bounds.height() / mBitmapHeight);
                }
                float dx = (bounds.width() - mBitmapWidth * scale) * 0.5f + bounds.left;
                float dy = (bounds.height() - mBitmapHeight * scale) * 0.5f + bounds.top;

                Matrix matrix = new Matrix();
                matrix.setScale(scale, scale);
                matrix.postTranslate(dx, dy);
                shader.setLocalMatrix(matrix);
            }
        }

        protected BaseSalientDrawable(Bitmap bitmap) {
            mPaint.setAntiAlias(true);
            mPaint.setShader(
                    new BitmapShader(bitmap, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP));
            mBitmapWidth = bitmap.getWidth();
            mBitmapHeight = bitmap.getHeight();
        }

        protected BaseSalientDrawable(int color) {
            mPaint.setAntiAlias(true);
            mPaint.setColor(color);
            mBitmapWidth = 0;
            mBitmapHeight = 0;
        }

        /**
         * @param shouldScale True to let bitmap be scaled to match the size of
         *                    the drawable. False to let it maintain its original size,
         *                    regardless of the size of the drawable.
         */
        public void shouldScale(boolean shouldScale) {
            mShouldScale = shouldScale;
        }

        @Override
        public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }

        @Override
        public void setAlpha(int alpha) {
            mPaint.setAlpha(alpha);
        }

        @Override
        public void setColorFilter(ColorFilter cf) {
            mPaint.setColorFilter(cf);
        }
    }

    private static final int SALIENT_IMAGE_ANIMATION_MS = 300;

    private final ImageView mSolidColorImage;
    private final ImageView mSalientImage;
    private SalientImageCallback mSalientImageCallback;

    /**
     * Create instance of a image view container.
     */
    public EnhancedBookmarkSalientImageView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mSolidColorImage = new ImageView(context, attrs);
        mSalientImage = new ImageView(context, attrs);
        mSalientImage.setVisibility(View.INVISIBLE);
        addView(mSalientImage);
        addView(mSolidColorImage);
    }

    /**
     * Load the image for a specific URL using a given bookmark model. If no image is found or is
     * still loading, a place holder with a given color is shown.
     * @param model Enhanced bookmark model from which to get image.
     * @param url Url for representing bookmark.
     * @param color Color of place holder.
     */
    public void load(EnhancedBookmarksModel model, String url, int color) {
        // Reset conditions.
        clearAnimation();
        mSolidColorImage.setImageDrawable(null);
        mSalientImage.setImageDrawable(null);
        mSolidColorImage.setVisibility(View.VISIBLE);
        mSalientImage.setVisibility(View.INVISIBLE);

        mSalientImageCallback = new SalientImageCallback() {
            @Override
            public void onSalientImageReady(Bitmap image, String imageUrl) {
                if (image == null) return;
                // Since we are reusing this view in GridView, it is possible that after the view is
                // reset to represent another bookmark, this method is finally called from native
                // side. Thus check if this callback is for current request.
                if (this != mSalientImageCallback) return;

                mSalientImage.setVisibility(View.VISIBLE);
                if (mSolidColorImage.getDrawable() != null) {
                    mSolidColorImage.animate().alpha(0.0f).setDuration(SALIENT_IMAGE_ANIMATION_MS)
                            .setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE)
                            .setListener(new AnimatorListenerAdapter() {
                                @Override
                                public void onAnimationEnd(android.animation.Animator animation) {
                                    mSolidColorImage.setVisibility(View.INVISIBLE);
                                    mSolidColorImage.setAlpha(1.0f);
                                }
                            });
                    mSalientImage.setImageDrawable(new CircularDrawable(image));
                    mSalientImage.setAlpha(0.0f);
                    mSalientImage.animate().alpha(1.0f).setDuration(SALIENT_IMAGE_ANIMATION_MS)
                            .setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
                } else {
                    mSalientImage.setImageDrawable(new CircularDrawable(image));
                    mSolidColorImage.setVisibility(View.INVISIBLE);
                }
            }
        };

        boolean isSalientImageLoaded = model.salientImageForUrl(url, mSalientImageCallback);

        // If there is no image or the image is still loading asynchronously, show pure color.
        if (!isSalientImageLoaded) {
            mSolidColorImage.setImageDrawable(new CircularDrawable(color));
        }
    }
}
