// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.view.animation.BounceInterpolator;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.DocumentModeManager;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * A page introducing the Recent Tabs feature.
 */
public class FirstRunIntroRecentsPage extends FirstRunIntroPage {
    /**
     * Constants that control the "loupe" animation ("loupe" looks like a magnifying glass).
     */
    private static final int LOUPE_ANIMATION_DURATION_MS = 500;
    private static final int LOUPE_ANIMATION_DELAY_MS = 1000;
    private static final float LOUPE_ANIMATION_SCALE_FROM = 0.5f;
    private static final float LOUPE_ANIMATION_SCALE_TO = 1.0f;

    /**
     * Property used to animate scrolling of the Loupe image.
     */
    private ImageView mLoupeImageView;
    private AnimatorSet mLoupeAnimation;

    // FirstRunIntroPage:
    @Override
    protected int getIntroDrawableId() {
        return R.drawable.fre_new_recents_menu;
    }

    @Override
    protected int getIntroTitleId() {
        return R.string.fre_new_recents_menu_title;
    }

    @Override
    protected int getIntroTextId() {
        return R.string.fre_new_recents_menu_text;
    }

    @Override
    public boolean shouldSkipPageOnResume(Context appContext) {
        return DocumentModeManager.getInstance(appContext).didOptOutStateChange();
    }

    // Fragment:
    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        TextView description = (TextView) view.findViewById(R.id.text);
        description.setMovementMethod(LinkMovementMethod.getInstance());
        description.setText(SpanApplier.applySpans(
                getString(R.string.fre_new_recents_menu_text),
                new SpanInfo("<link>", "</link>", new ClickableSpan() {
                    @Override
                    public void onClick(View widget) {
                        openSettings();
                    }
                })));

        Button settings = (Button) view.findViewById(R.id.negative_button);
        settings.setText(R.string.fre_settings);
        settings.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        openSettings();
                    }
                });

        mLoupeImageView = new ImageView(view.getContext());
        mLoupeImageView.setImageResource(R.drawable.fre_new_recents_menu_loupe);
        FrameLayout layout = (FrameLayout) view.findViewById(R.id.image_view_wrapper);
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        params.setMargins(
                getResources().getDimensionPixelSize(R.dimen.fre_recents_loupe_margin_left),
                getResources().getDimensionPixelSize(R.dimen.fre_recents_loupe_margin_top),
                0, 0);
        layout.addView(mLoupeImageView, params);
    }

    @Override
    public void setUserVisibleHint(boolean isVisibleToUser) {
        super.setUserVisibleHint(isVisibleToUser);
        if (!isVisibleToUser || getView() == null) return;

        getView().postDelayed(new Runnable() {
            @Override
            public void run() {
                mLoupeAnimation = new AnimatorSet();
                mLoupeAnimation.playTogether(
                        ObjectAnimator.ofFloat(mLoupeImageView, View.SCALE_X,
                                LOUPE_ANIMATION_SCALE_FROM, LOUPE_ANIMATION_SCALE_TO),
                        ObjectAnimator.ofFloat(mLoupeImageView, View.SCALE_Y,
                                LOUPE_ANIMATION_SCALE_FROM, LOUPE_ANIMATION_SCALE_TO));
                mLoupeAnimation.setDuration(LOUPE_ANIMATION_DURATION_MS);
                mLoupeAnimation.setInterpolator(new BounceInterpolator());
                mLoupeAnimation.start();
            }
        }, LOUPE_ANIMATION_DELAY_MS);
    }

    private void openSettings() {
        DocumentModeManager.getInstance(getActivity().getApplicationContext())
                .savePreviousOptOutState();
        getPageDelegate().openDocumentModeSettings();
    }
}
