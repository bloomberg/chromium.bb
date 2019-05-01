// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.PopupWindow;

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

    TabGridDialogParent(Context context) {
        setUpDialog(context);
    }

    private void setUpDialog(Context context) {
        FrameLayout backgroundView = new FrameLayout(context);
        mDialogContainerView = new LinearLayout(context);
        FrameLayout.LayoutParams containerParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        int sideMargin =
                (int) context.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
        int topMargin =
                (int) context.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
        containerParams.setMargins(sideMargin, topMargin, sideMargin, topMargin);
        mDialogContainerView.setLayoutParams(containerParams);
        mDialogContainerView.setBackgroundColor(Color.WHITE);
        mDialogContainerView.setOrientation(LinearLayout.VERTICAL);
        backgroundView.addView(mDialogContainerView);

        DisplayMetrics displayMetrics = new DisplayMetrics();
        ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay()
                .getMetrics(displayMetrics);
        mPopupWindow = new PopupWindow(
                backgroundView, displayMetrics.widthPixels, displayMetrics.heightPixels);
        mScrimView = new ScrimView(context, null, backgroundView);

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

    void setScrimViewObserver(ScrimView.ScrimObserver scrimViewObserver) {
        mScrimParams =
                new ScrimView.ScrimParams(mDialogContainerView, false, true, 0, scrimViewObserver);
    }

    void updateDialog(View toolbarView, View recyclerView) {
        mDialogContainerView.removeAllViews();
        mDialogContainerView.addView(toolbarView);
        mDialogContainerView.addView(recyclerView);
        recyclerView.setVisibility(View.VISIBLE);
    }

    void showDialog(View parent) {
        if (mCurrentAnimator != null && mCurrentAnimator != mDialogFadeIn) {
            mCurrentAnimator.cancel();
        }
        mPopupWindow.showAtLocation(parent, Gravity.CENTER, 0, 0);
        mScrimView.showScrim(mScrimParams);
        mDialogFadeIn.start();
        mCurrentAnimator = mDialogFadeIn;
    }

    void hideDialog() {
        if (mCurrentAnimator != null && mCurrentAnimator != mDialogFadeOut) {
            mCurrentAnimator.cancel();
        }
        mScrimView.hideScrim(true);
        mDialogFadeOut.start();
        mCurrentAnimator = mDialogFadeOut;
    }
}
