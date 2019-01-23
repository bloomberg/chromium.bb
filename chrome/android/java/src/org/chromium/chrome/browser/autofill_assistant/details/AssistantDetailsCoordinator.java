// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.media.ThumbnailUtils;
import android.os.Build;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.cached_image_fetcher.CachedImageFetcher;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Coordinator responsible for showing details.
 */
public class AssistantDetailsCoordinator {
    private static final int IMAGE_BORDER_RADIUS = 4;
    private static final int PULSING_DURATION_MS = 1_000;
    private static final String DETAILS_TIME_FORMAT = "H:mma";
    private static final String DETAILS_DATE_FORMAT = "EEE, MMM d";

    private final Context mContext;
    private final Runnable mOnVisibilityChanged;

    private final View mView;
    private final GradientDrawable mDefaultImage;
    private final ImageView mImageView;
    private final TextView mTitleView;
    private final TextView mSubtextView;

    private final int mImageWidth;
    private final int mImageHeight;
    private final int mPulseAnimationStartColor;
    private final int mPulseAnimationEndColor;

    private final Set<View> mViewsToAnimate = new HashSet<>();
    private ValueAnimator mPulseAnimation;

    public AssistantDetailsCoordinator(
            ChromeActivity activity, AssistantDetailsModel model, Runnable onVisibilityChanged) {
        mContext = activity;
        mOnVisibilityChanged = onVisibilityChanged;

        mView = LayoutInflater.from(activity).inflate(
                R.layout.autofill_assistant_details, /* root= */ null);

        mDefaultImage = (GradientDrawable) activity.getResources().getDrawable(
                R.drawable.autofill_assistant_default_details);
        mImageView = mView.findViewById(R.id.details_image);
        mTitleView = mView.findViewById(R.id.details_title);
        mSubtextView = mView.findViewById(R.id.details_text);

        mImageWidth = mContext.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mImageHeight = mContext.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mPulseAnimationStartColor = mContext.getResources().getColor(R.color.modern_grey_100);
        mPulseAnimationEndColor = mContext.getResources().getColor(R.color.modern_grey_50);

        // Details view is initially hidden.
        setVisible(false);

        model.addObserver((source, propertyKey) -> {
            if (AssistantDetailsModel.DETAILS == propertyKey) {
                AssistantDetails details = model.get(AssistantDetailsModel.DETAILS);
                if (details != null) {
                    showDetails(details);
                } else {
                    setVisible(false);
                }
            }
        });
    }

    /**
     * Return the view associated to the details.
     */
    public View getView() {
        return mView;
    }

    /**
     * Show or hide the details within its parent and call the {@code mOnVisibilityChanged}
     * listener.
     */
    private void setVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        boolean changed = mView.getVisibility() != visibility;
        if (changed) {
            mView.setVisibility(visibility);
            mOnVisibilityChanged.run();
        }
    }

    /**
     * Update the details.
     */
    private void showDetails(AssistantDetails details) {
        String detailsText = makeDetailsText(details);
        mTitleView.setText(details.getTitle());
        mSubtextView.setText(detailsText);

        if (mImageView.getDrawable() == null) {
            // Set default image if no image was set before.
            mImageView.setImageDrawable(mDefaultImage);
        }

        setTextStyles(details);

        // Download image and then set it in the model.
        if (!details.getUrl().isEmpty()) {
            CachedImageFetcher.getInstance().fetchImage(details.getUrl(), image -> {
                if (image != null) {
                    mImageView.setImageDrawable(getRoundedImage(image));
                }
            });
        }

        setVisible(true);
    }

    private void setTextStyles(AssistantDetails details) {
        setTitleStyle(details);
        setSubtextStyle(details);
    }

    private void setTitleStyle(AssistantDetails details) {
        boolean animateBackground = false;

        if (details.getUserApprovalRequired() && !details.getHighlightTitle()) {
            // De-emphasize title if user needs to accept details but the title should not be
            // highlighted.
            mTitleView.setTextColor(ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.modern_grey_300));
        } else {
            // Normal style: bold black text.
            ApiCompatibilityUtils.setTextAppearance(
                    mTitleView, R.style.TextAppearance_BlackCaptionDefault);
            mTitleView.setTypeface(mTitleView.getTypeface(), Typeface.BOLD);

            if (mTitleView.length() == 0 && details.getShowPlaceholdersForEmptyFields()) {
                animateBackground = true;
            }
        }

        if (animateBackground) {
            addViewToAnimation(mTitleView);
        } else {
            removeViewFromAnimation(mTitleView);
        }
    }

    private void setSubtextStyle(AssistantDetails details) {
        boolean animateBackground = false;

        if (details.getUserApprovalRequired()) {
            if (details.getHighlightDate()) {
                // Emphasized style.
                mSubtextView.setTypeface(mSubtextView.getTypeface(), Typeface.BOLD_ITALIC);
            } else {
                // De-emphasized style.
                mSubtextView.setTextColor(ApiCompatibilityUtils.getColor(
                        mContext.getResources(), R.color.modern_grey_300));
            }
        } else {
            // Normal style.
            ApiCompatibilityUtils.setTextAppearance(
                    mSubtextView, R.style.TextAppearance_BlackCaption);

            if (mSubtextView.length() == 0 && details.getShowPlaceholdersForEmptyFields()) {
                animateBackground = true;
            }
        }

        if (animateBackground) {
            addViewToAnimation(mSubtextView);
        } else {
            removeViewFromAnimation(mSubtextView);
        }
    }

    private String makeDetailsText(AssistantDetails details) {
        List<String> parts = new ArrayList<>();
        Date date = details.getDate();
        if (date != null) {
            parts.add(formatDetailsTime(date));
            parts.add(formatDetailsDate(date));
        }

        String description = details.getDescription();
        if (description != null && !description.isEmpty()) {
            parts.add(description);
        }

        // TODO(crbug.com/806868): Use a view instead of this dot text.
        return TextUtils.join(" • ", parts);
    }

    private Locale getLocale() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                ? mContext.getResources().getConfiguration().getLocales().get(0)
                : mContext.getResources().getConfiguration().locale;
    }

    private String formatDetailsTime(Date date) {
        DateFormat df = DateFormat.getTimeInstance(DateFormat.SHORT, Locale.getDefault());
        String timeFormatPattern = (df instanceof SimpleDateFormat)
                ? ((SimpleDateFormat) df).toPattern()
                : DETAILS_TIME_FORMAT;
        return new SimpleDateFormat(timeFormatPattern, getLocale()).format(date);
    }

    private String formatDetailsDate(Date date) {
        return new SimpleDateFormat(DETAILS_DATE_FORMAT, getLocale()).format(date);
    }

    private Drawable getRoundedImage(Bitmap bitmap) {
        RoundedBitmapDrawable roundedBitmap =
                RoundedBitmapDrawableFactory.create(mContext.getResources(),
                        ThumbnailUtils.extractThumbnail(bitmap, mImageWidth, mImageHeight));
        roundedBitmap.setCornerRadius(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                IMAGE_BORDER_RADIUS, mContext.getResources().getDisplayMetrics()));
        return roundedBitmap;
    }

    private void addViewToAnimation(View view) {
        if (mViewsToAnimate.add(view) && mPulseAnimation == null) {
            mPulseAnimation =
                    ValueAnimator.ofInt(mPulseAnimationStartColor, mPulseAnimationEndColor);
            mPulseAnimation.setDuration(PULSING_DURATION_MS);
            mPulseAnimation.setEvaluator(new ArgbEvaluator());
            mPulseAnimation.setRepeatCount(ValueAnimator.INFINITE);
            mPulseAnimation.setRepeatMode(ValueAnimator.REVERSE);
            mPulseAnimation.setInterpolator(CompositorAnimator.ACCELERATE_INTERPOLATOR);
            mPulseAnimation.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationCancel(Animator animation) {
                    mTitleView.setBackgroundColor(Color.WHITE);
                    mSubtextView.setBackgroundColor(Color.WHITE);
                    mDefaultImage.setColor(mPulseAnimationStartColor);
                }
            });
            mPulseAnimation.addUpdateListener(animation -> {
                int animatedValue = (int) animation.getAnimatedValue();
                for (View viewToAnimate : mViewsToAnimate) {
                    viewToAnimate.setBackgroundColor(animatedValue);
                }
                mDefaultImage.setColor(animatedValue);
            });
            mPulseAnimation.start();
        }
    }

    private void removeViewFromAnimation(View view) {
        if (mViewsToAnimate.remove(view) && mPulseAnimation != null) {
            mPulseAnimation.cancel();
            mPulseAnimation = null;
        }

        // Reset background to white.
        view.setBackgroundColor(Color.WHITE);
    }
}
