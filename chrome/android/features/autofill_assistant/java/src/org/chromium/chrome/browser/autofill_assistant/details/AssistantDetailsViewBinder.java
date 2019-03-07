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
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.cached_image_fetcher.CachedImageFetcher;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * This class is responsible for pushing updates to the Autofill Assistant details view. These
 * updates are pulled from the {@link AssistantDetailsModel} when a notification of an update is
 * received.
 */
class AssistantDetailsViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantDetailsModel,
                AssistantDetailsViewBinder.ViewHolder, PropertyKey> {
    private static final int IMAGE_BORDER_RADIUS = 4;
    private static final int PULSING_DURATION_MS = 1_000;
    private static final String DETAILS_TIME_FORMAT = "H:mma";
    private static final String DETAILS_DATE_FORMAT = "EEE, MMM d";

    /**
     * A wrapper class that holds the different views of the header.
     */
    static class ViewHolder {
        final GradientDrawable mDefaultImage;
        final ImageView mImageView;
        final TextView mTitleView;
        final TextView mDescriptionLine1View;
        final TextView mDescriptionLine2View;
        final View mPriceView;
        final TextView mTotalPriceLabelView;
        final TextView mTotalPriceView;

        public ViewHolder(Context context, View detailsView) {
            mDefaultImage = (GradientDrawable) context.getResources().getDrawable(
                    R.drawable.autofill_assistant_default_details);
            mImageView = detailsView.findViewById(R.id.details_image);
            mTitleView = detailsView.findViewById(R.id.details_title);
            mDescriptionLine1View = detailsView.findViewById(R.id.details_line1);
            mDescriptionLine2View = detailsView.findViewById(R.id.details_line2);
            mPriceView = detailsView.findViewById(R.id.details_price);
            mTotalPriceView = detailsView.findViewById(R.id.details_total_price);
            mTotalPriceLabelView = detailsView.findViewById(R.id.details_total_price_label);
        }
    }

    private final Context mContext;

    private final int mImageWidth;
    private final int mImageHeight;
    private final int mPulseAnimationStartColor;
    private final int mPulseAnimationEndColor;

    private ValueAnimator mPulseAnimation;

    AssistantDetailsViewBinder(Context context) {
        mContext = context;
        mImageWidth = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mImageHeight = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_details_image_size);
        mPulseAnimationStartColor = context.getResources().getColor(R.color.modern_grey_300);
        mPulseAnimationEndColor = context.getResources().getColor(R.color.modern_grey_200);
    }

    @Override
    public void bind(AssistantDetailsModel model, ViewHolder view, PropertyKey propertyKey) {
        if (AssistantDetailsModel.DETAILS == propertyKey) {
            AssistantDetails details = model.get(AssistantDetailsModel.DETAILS);
            if (details == null) {
                // Handled by the AssistantDetailsCoordinator.
                return;
            }

            setDetails(details, view);
        } else {
            assert false : "Unhandled property detected in AssistantDetailsViewBinder!";
        }
    }

    private void setDetails(AssistantDetails details, ViewHolder viewHolder) {
        viewHolder.mTitleView.setText(details.getTitle());
        viewHolder.mDescriptionLine1View.setText(makeDescriptionLine1Text(details));
        viewHolder.mDescriptionLine2View.setText(details.getDescriptionLine2());
        viewHolder.mTotalPriceLabelView.setText(details.getTotalPriceLabel());
        viewHolder.mTotalPriceView.setText(details.getTotalPrice());

        // Allow title line wrapping if no or only one description set.
        boolean isDescriptionLine1Empty = viewHolder.mDescriptionLine1View.length() == 0;
        boolean isDescriptionLine2Empty = viewHolder.mDescriptionLine2View.length() == 0;
        if (isDescriptionLine1Empty || isDescriptionLine2Empty) {
            int maxLines = isDescriptionLine1Empty && isDescriptionLine2Empty ? 3 : 2;
            viewHolder.mTitleView.setSingleLine(false);
            viewHolder.mTitleView.setMaxLines(maxLines);
            viewHolder.mTitleView.setEllipsize(TextUtils.TruncateAt.END);
        } else {
            viewHolder.mTitleView.setSingleLine(true);
            viewHolder.mTitleView.setEllipsize(null);
        }

        hideIfEmpty(viewHolder.mDescriptionLine1View);
        hideIfEmpty(viewHolder.mDescriptionLine2View);

        // If no price provided, hide the price view (containing separator, price label, and price).
        viewHolder.mPriceView.setVisibility(
                details.getTotalPrice().isEmpty() ? View.GONE : View.VISIBLE);

        viewHolder.mImageView.setVisibility(View.VISIBLE);
        if (details.getImageUrl().isEmpty()) {
            if (details.getShowImagePlaceholder()) {
                viewHolder.mImageView.setImageDrawable(viewHolder.mDefaultImage);
            } else {
                viewHolder.mImageView.setVisibility(View.GONE);
            }
        } else {
            // Download image and then set it in the view.
            CachedImageFetcher.getInstance().fetchImage(details.getImageUrl(),
                    CachedImageFetcher.ASSISTANT_DETAILS_UMA_CLIENT_NAME, image -> {
                        if (image != null) {
                            viewHolder.mImageView.setImageDrawable(getRoundedImage(image));
                        }
                    });
        }

        setTextStyles(details, viewHolder);
    }

    private String makeDescriptionLine1Text(AssistantDetails details) {
        if (!details.getDescriptionLine1().isEmpty()) {
            return details.getDescriptionLine1();
        } else {
            return makeDatetimeText(details);
        }
    }

    private String makeDatetimeText(AssistantDetails details) {
        List<String> parts = new ArrayList<>();
        Date date = details.getDate();
        if (date != null) {
            parts.add(formatDetailsTime(date));
            parts.add(formatDetailsDate(date));
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

    private void hideIfEmpty(TextView view) {
        view.setVisibility(view.length() == 0 ? View.GONE : View.VISIBLE);
    }

    private void setTextStyles(AssistantDetails details, ViewHolder viewHolder) {
        setTitleStyle(details.getUserApprovalRequired(), details.getHighlightTitle(), viewHolder);
        setSubtextStyle(viewHolder.mDescriptionLine1View, details.getUserApprovalRequired(),
                details.getHighlightLine1(), viewHolder);
        setSubtextStyle(viewHolder.mDescriptionLine2View, details.getUserApprovalRequired(),
                details.getHighlightLine2(), viewHolder);

        if (shouldStartOrContinuePlaceholderAnimation(details, viewHolder)) {
            startOrContinuePlaceholderAnimations(viewHolder);
        } else {
            stopPlaceholderAnimations();
        }
    }

    private boolean shouldStartOrContinuePlaceholderAnimation(
            AssistantDetails details, ViewHolder viewHolder) {
        boolean isAtLeastOneFieldEmpty = viewHolder.mTitleView.length() == 0
                || viewHolder.mDescriptionLine1View.length() == 0
                || viewHolder.mDescriptionLine2View.length() == 0
                || viewHolder.mImageView.getDrawable() == viewHolder.mDefaultImage;
        return details.getAnimatePlaceholders() && isAtLeastOneFieldEmpty;
    }

    private void setTitleStyle(boolean approvalRequired, boolean highlight, ViewHolder viewHolder) {
        TextView titleView = viewHolder.mTitleView;
        if (approvalRequired && !highlight) {
            // De-emphasize title if user needs to accept details but the title should not be
            // highlighted.
            titleView.setTextColor(ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.modern_grey_300));
        } else {
            // Normal style: bold black text.
            ApiCompatibilityUtils.setTextAppearance(
                    titleView, R.style.TextAppearance_BlackCaptionDefault);
            if (highlight) {
                titleView.setTypeface(titleView.getTypeface(), Typeface.BOLD);
            }
        }
    }

    private void setSubtextStyle(
            TextView view, boolean approvalRequired, boolean highlight, ViewHolder viewHolder) {
        // Emphasized style.
        if (approvalRequired && highlight) {
            view.setTypeface(view.getTypeface(), Typeface.BOLD_ITALIC);
        } else if (approvalRequired) {
            // De-emphasized style.
            view.setTextColor(ApiCompatibilityUtils.getColor(
                    mContext.getResources(), R.color.modern_grey_300));
        } else {
            // getUserApprovalRequired == false, normal style.
            ApiCompatibilityUtils.setTextAppearance(view, R.style.TextAppearance_BlackCaption);
        }
    }

    private Drawable getRoundedImage(Bitmap bitmap) {
        RoundedBitmapDrawable roundedBitmap =
                RoundedBitmapDrawableFactory.create(mContext.getResources(),
                        ThumbnailUtils.extractThumbnail(bitmap, mImageWidth, mImageHeight));
        roundedBitmap.setCornerRadius(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                IMAGE_BORDER_RADIUS, mContext.getResources().getDisplayMetrics()));
        return roundedBitmap;
    }

    private void startOrContinuePlaceholderAnimations(ViewHolder viewHolder) {
        if (mPulseAnimation != null) {
            return;
        }
        mPulseAnimation = ValueAnimator.ofInt(mPulseAnimationStartColor, mPulseAnimationEndColor);
        mPulseAnimation.setDuration(PULSING_DURATION_MS);
        mPulseAnimation.setEvaluator(new ArgbEvaluator());
        mPulseAnimation.setRepeatCount(ValueAnimator.INFINITE);
        mPulseAnimation.setRepeatMode(ValueAnimator.REVERSE);
        mPulseAnimation.setInterpolator(CompositorAnimator.ACCELERATE_INTERPOLATOR);
        mPulseAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationCancel(Animator animation) {
                viewHolder.mTitleView.setBackgroundColor(Color.TRANSPARENT);
                viewHolder.mDescriptionLine1View.setBackgroundColor(Color.TRANSPARENT);
                viewHolder.mDescriptionLine2View.setBackgroundColor(Color.TRANSPARENT);
                viewHolder.mDefaultImage.setColor(Color.TRANSPARENT);
            }
        });
        mPulseAnimation.addUpdateListener(animation -> {
            int animatedValue = (int) animation.getAnimatedValue();
            viewHolder.mTitleView.setBackgroundColor(
                    viewHolder.mTitleView.length() == 0 ? animatedValue : Color.TRANSPARENT);
            viewHolder.mDescriptionLine1View.setBackgroundColor(
                    viewHolder.mDescriptionLine1View.length() == 0 ? animatedValue
                                                                   : Color.TRANSPARENT);
            viewHolder.mDescriptionLine2View.setBackgroundColor(
                    viewHolder.mDescriptionLine2View.length() == 0 ? animatedValue
                                                                   : Color.TRANSPARENT);
            viewHolder.mDefaultImage.setColor(
                    viewHolder.mImageView.getDrawable() == viewHolder.mDefaultImage
                            ? animatedValue
                            : Color.TRANSPARENT);
        });
        mPulseAnimation.start();
    }

    private void stopPlaceholderAnimations() {
        if (mPulseAnimation != null) {
            mPulseAnimation.cancel();
            mPulseAnimation = null;
        }
    }
}
