// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * A custom RecyclerView implementation for the tab grid, to handle show/hide logic in class.
 */
class TabListRecyclerView extends RecyclerView {
    public static final long BASE_ANIMATION_DURATION_MS = 218;

    private ValueAnimator mFadeInAnimator;
    private ValueAnimator mFadeOutAnimator;

    public TabListRecyclerView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    void startShowing() {
        cancelAllAnimations();
        setAlpha(0);
        setVisibility(View.VISIBLE);
        mFadeInAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 1);
        mFadeInAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        mFadeInAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mFadeInAnimator.start();
    }

    void startHiding() {
        cancelAllAnimations();
        mFadeOutAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 0);
        mFadeOutAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        mFadeOutAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mFadeOutAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                setVisibility(View.INVISIBLE);
            }
        });
        mFadeOutAnimator.start();
    }

    private void cancelAllAnimations() {
        if (mFadeInAnimator != null) {
            mFadeInAnimator.cancel();
        }
        if (mFadeOutAnimator != null) {
            mFadeOutAnimator.cancel();
        }
    }
}
