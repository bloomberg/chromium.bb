// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.animation.DecelerateInterpolator;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Visual representation of a snackbar. On phone it matches the width of the activity; on tablet it
 * has a fixed width and is anchored at the start-bottom corner of the current window.
 */
class SnackbarView {
    private final ViewGroup mView;
    private final ViewGroup mParent;
    private final TemplatePreservingTextView mMessageView;
    private final TextView mActionButtonView;
    private final ImageView mProfileImageView;
    private final int mAnimationDuration;
    private final boolean mIsTablet;
    private Snackbar mSnackbar;

    // Variables used to calculate the virtual keyboard's height.
    private int[] mTempDecorPosition = new int[2];
    private Rect mCurrentVisibleRect = new Rect();
    private Rect mPreviousVisibleRect = new Rect();

    private OnGlobalLayoutListener mLayoutListener = new OnGlobalLayoutListener() {
        @Override
        public void onGlobalLayout() {
            // Why adjust layout params in onGlobalLayout, not in onMeasure()?
            // View#onMeasure() does not work well with getWindowVisibleDisplayFrame(). Especially
            // when device orientation changes, onMeasure is called before
            // getWindowVisibleDisplayFrame's value is updated.
            adjustViewPosition();
        }
    };

    /**
     * Creates an instance of the {@link SnackbarView}.
     * @param parent   The main view of the embedding Activity.
     * @param listener An {@link OnClickListener} that will be called when the action button is
     *                 clicked.
     * @param snackbar The snackbar to be displayed.
     */
    SnackbarView(ViewGroup parent, OnClickListener listener, Snackbar snackbar) {
        Context context = parent.getContext();
        mIsTablet = DeviceFormFactor.isTablet(context);
        mParent = parent;
        mView = (ViewGroup) LayoutInflater.from(context).inflate(R.layout.snackbar, mParent, false);
        mAnimationDuration = mView.getResources()
                .getInteger(android.R.integer.config_mediumAnimTime);
        mMessageView = (TemplatePreservingTextView) mView.findViewById(R.id.snackbar_message);
        mActionButtonView = (TextView) mView.findViewById(R.id.snackbar_button);
        mActionButtonView.setOnClickListener(listener);
        mProfileImageView = (ImageView) mView.findViewById(R.id.snackbar_profile_image);

        updateInternal(snackbar, false);
    }

    void show() {
        mParent.addView(mView);
        adjustViewPosition();
        mView.getViewTreeObserver().addOnGlobalLayoutListener(mLayoutListener);

        // Animation, instead of Animator or ViewPropertyAnimator is prefered here because:
        // 1. Animation xml allows specifying 100% as translation, which is much more convenient
        // than waiting for mView to be laid out to get measured height.
        // 2. Animation avoids SurfaceView's clipping issue when doing translating animation.
        Animation fadeIn = AnimationUtils.loadAnimation(mParent.getContext(), R.anim.snackbar_in);
        mView.startAnimation(fadeIn);
    }

    void dismiss() {
        // Disable action button during animation.
        mActionButtonView.setEnabled(false);
        mView.getViewTreeObserver().removeOnGlobalLayoutListener(mLayoutListener);
        // ViewPropertyAnimator is prefered here because Animation is not canceled when the activity
        // is in backbround.
        mView.animate()
                .alpha(0f)
                .setDuration(mAnimationDuration)
                .setInterpolator(new DecelerateInterpolator())
                .withEndAction(new Runnable() {
                    @Override
                    public void run() {
                        mParent.removeView(mView);
                    }
                }).start();
    }

    void adjustViewPosition() {
        mParent.getWindowVisibleDisplayFrame(mCurrentVisibleRect);
        // Only update if the visible frame has changed, otherwise there will be a layout loop.
        if (!mCurrentVisibleRect.equals(mPreviousVisibleRect)) {
            mPreviousVisibleRect.set(mCurrentVisibleRect);

            mParent.getLocationInWindow(mTempDecorPosition);
            int activityHeight = mTempDecorPosition[1] + mParent.getHeight();
            int visibleHeight = Math.min(mCurrentVisibleRect.bottom, activityHeight);
            int keyboardHeight = activityHeight - visibleHeight;

            MarginLayoutParams lp = (MarginLayoutParams) mView.getLayoutParams();
            lp.bottomMargin = keyboardHeight;
            if (mIsTablet) {
                int margin = mParent.getResources()
                        .getDimensionPixelSize(R.dimen.snackbar_margin_tablet);
                ApiCompatibilityUtils.setMarginStart(lp, margin);
                lp.bottomMargin += margin;
                int width = mParent.getResources()
                        .getDimensionPixelSize(R.dimen.snackbar_width_tablet);
                lp.width = Math.min(width, mParent.getWidth() - 2 * margin);
            }
            mView.setLayoutParams(lp);
        }
    }

    boolean isShowing() {
        return mView.isShown();
    }

    /**
     * Sends an accessibility event to mMessageView announcing that this window was added so that
     * the mMessageView content description is read aloud if accessibility is enabled.
     */
    void announceforAccessibility() {
        mMessageView.announceForAccessibility(mMessageView.getContentDescription()
                + mView.getResources().getString(R.string.bottom_bar_screen_position));
    }

    /**
     * Updates the view to display data from the given snackbar. No-op if the view is already
     * showing the given snackbar.
     * @param snackbar The snackbar to display
     * @return Whether update has actually been executed.
     */
    boolean update(Snackbar snackbar) {
        return updateInternal(snackbar, true);
    }

    private boolean updateInternal(Snackbar snackbar, boolean animate) {
        if (mSnackbar == snackbar) return false;
        mSnackbar = snackbar;
        mMessageView.setMaxLines(snackbar.getSingleLine() ? 1 : Integer.MAX_VALUE);
        mMessageView.setTemplate(snackbar.getTemplateText());
        setViewText(mMessageView, snackbar.getText(), animate);
        String actionText = snackbar.getActionText();

        int backgroundColor = snackbar.getBackgroundColor();
        if (backgroundColor == 0) {
            backgroundColor = ApiCompatibilityUtils.getColor(mView.getResources(),
                    R.color.snackbar_background_color);
        }

        if (mIsTablet) {
            // On tablet, snackbars have rounded corners.
            mView.setBackgroundResource(R.drawable.snackbar_background_tablet);
            GradientDrawable backgroundDrawable = (GradientDrawable) mView.getBackground().mutate();
            backgroundDrawable.setColor(backgroundColor);
        } else {
            mView.setBackgroundColor(backgroundColor);
        }

        if (actionText != null) {
            mActionButtonView.setVisibility(View.VISIBLE);
            setViewText(mActionButtonView, snackbar.getActionText(), animate);
        } else {
            mActionButtonView.setVisibility(View.GONE);
        }
        Bitmap profileImage = snackbar.getProfileImage();
        if (profileImage != null) {
            mProfileImageView.setVisibility(View.VISIBLE);
            mProfileImageView.setImageBitmap(profileImage);
        } else {
            mProfileImageView.setVisibility(View.GONE);
        }
        return true;
    }

    private void setViewText(TextView view, CharSequence text, boolean animate) {
        if (view.getText().toString().equals(text)) return;
        view.animate().cancel();
        if (animate) {
            view.setAlpha(0.0f);
            view.setText(text);
            view.animate().alpha(1.f).setDuration(mAnimationDuration).setListener(null);
        } else {
            view.setText(text);
        }
    }
}
