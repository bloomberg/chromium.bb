// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.PopupWindow;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * Parent for TabGridDialog component.
 * TODO(yuezhanggg): Add animations of card scales up to dialog and dialog scales down to card when
 * show/hide dialog.
 */
public class TabGridDialogParent {
    private PopupWindow mPopupWindow;
    private LinearLayout mDialogContainerView;
    private ScrimView mScrimView;
    private ScrimView.ScrimParams mScrimParams;
    private ValueAnimator mDialogFadeIn;
    private ValueAnimator mDialogFadeOut;
    private Animator mCurrentAnimator;
    private final ComponentCallbacks mComponentCallbacks;
    private final FrameLayout.LayoutParams mContainerParams;
    private final ViewGroup mParent;
    private final int mSideMargin;
    private final int mTopMargin;
    private final int mStatusBarHeight;

    TabGridDialogParent(Context context, ViewGroup parent) {
        mParent = parent;
        mSideMargin =
                (int) context.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
        mTopMargin = (int) context.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
        int statusBarHeightResourceId =
                context.getResources().getIdentifier("status_bar_height", "dimen", "android");
        mStatusBarHeight = context.getResources().getDimensionPixelSize(statusBarHeightResourceId);
        mContainerParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        mComponentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration newConfig) {
                updateDialogWithOrientation(context, newConfig.orientation);
            }

            @Override
            public void onLowMemory() {}
        };
        ContextUtils.getApplicationContext().registerComponentCallbacks(mComponentCallbacks);
        setupDialogContent(context);
        setupDialogAnimation();
    }

    private void setupDialogContent(Context context) {
        FrameLayout backgroundView = new FrameLayout(context);
        mDialogContainerView = new LinearLayout(context);
        mDialogContainerView.setLayoutParams(mContainerParams);
        mDialogContainerView.setBackgroundColor(ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.modern_primary_color));
        mDialogContainerView.setOrientation(LinearLayout.VERTICAL);
        mScrimView = new ScrimView(context, null, backgroundView);
        mPopupWindow = new PopupWindow(backgroundView, 0, 0);
        backgroundView.addView(mDialogContainerView);
        updateDialogWithOrientation(context, context.getResources().getConfiguration().orientation);
    }

    private void setupDialogAnimation() {
        mDialogFadeIn = ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 0f, 1f);
        mDialogFadeIn.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        mDialogFadeIn.setDuration(TabListRecyclerView.BASE_ANIMATION_DURATION_MS);
        mDialogFadeIn.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mCurrentAnimator = null;
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                mCurrentAnimator = null;
            }
        });

        mDialogFadeOut = ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 1f, 0f);
        mDialogFadeOut.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        mDialogFadeOut.setDuration(TabListRecyclerView.BASE_ANIMATION_DURATION_MS);
        mDialogFadeOut.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mPopupWindow.dismiss();
                mCurrentAnimator = null;
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                mPopupWindow.dismiss();
                mCurrentAnimator = null;
            }
        });
    }

    private void updateDialogWithOrientation(Context context, int orientation) {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay()
                .getMetrics(displayMetrics);
        mPopupWindow.setHeight(displayMetrics.heightPixels);
        mPopupWindow.setWidth(displayMetrics.widthPixels);
        if (mPopupWindow != null && mPopupWindow.isShowing()) {
            mPopupWindow.dismiss();
            mPopupWindow.showAtLocation(mParent, Gravity.CENTER, 0, 0);
        }
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            mContainerParams.setMargins(
                    mSideMargin, mTopMargin, mSideMargin, mTopMargin + mStatusBarHeight);
        } else {
            mContainerParams.setMargins(
                    mTopMargin, mSideMargin, mTopMargin, mSideMargin + mStatusBarHeight);
        }
    }

    /**
     * Setup mScrimParams with the {@code scrimViewObserver}.
     *
     * @param scrimViewObserver The ScrimObserver to be used to setup mScrimParams.
     */
    void setScrimViewObserver(ScrimView.ScrimObserver scrimViewObserver) {
        mScrimParams =
                new ScrimView.ScrimParams(mDialogContainerView, false, true, 0, scrimViewObserver);
    }

    /**
     * Reset the dialog content with {@code toolbarView} and {@code recyclerView}.
     *
     * @param toolbarView The toolbarview to be added to dialog.
     * @param recyclerView The recyclerview to be added to dialog.
     */
    void resetDialog(View toolbarView, View recyclerView) {
        mDialogContainerView.removeAllViews();
        mDialogContainerView.addView(toolbarView);
        mDialogContainerView.addView(recyclerView);
        recyclerView.setVisibility(View.VISIBLE);
    }

    /**
     * Show {@link PopupWindow} for dialog with animation.
     */
    void showDialog() {
        if (mCurrentAnimator != null && mCurrentAnimator != mDialogFadeIn) {
            mCurrentAnimator.cancel();
        }
        mPopupWindow.showAtLocation(mParent, Gravity.CENTER, 0, 0);
        mScrimView.showScrim(mScrimParams);
        mDialogFadeIn.start();
        mCurrentAnimator = mDialogFadeIn;
    }

    /**
     * Hide {@link PopupWindow} for dialog with animation.
     */
    void hideDialog() {
        if (mCurrentAnimator != null && mCurrentAnimator != mDialogFadeOut) {
            mCurrentAnimator.cancel();
        }
        mScrimView.hideScrim(true);
        mDialogFadeOut.start();
        mCurrentAnimator = mDialogFadeOut;
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        ContextUtils.getApplicationContext().unregisterComponentCallbacks(mComponentCallbacks);
    }
}
