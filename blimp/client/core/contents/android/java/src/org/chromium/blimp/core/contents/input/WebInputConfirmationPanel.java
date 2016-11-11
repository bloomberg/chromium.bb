// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.contents.input;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;

import org.chromium.blimp.core.R;

/**
 * A layout containing OK, Cancel buttons and a progress spinner which can be included at the bottom
 * of a popup.
 */
public class WebInputConfirmationPanel extends RelativeLayout {
    /**
     * Interface that gets notified of the user actions on this layout.
     */
    public interface Listener {
        /**
         * The user has pressed OK button.
         */
        public void onConfirm();

        /**
         * The user has pressed cancel button.
         */
        public void onCancel();
    }

    private Listener mListener;

    /**
     * Builds a new {@link WebInputConfirmationPanel}.
     * @param context The {@link Context} of the activity.
     * @param attrs The {@link AttributeSet} associated with this layout.
     */
    public WebInputConfirmationPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflate(context, R.layout.web_input_bottom_panel, this);
    }

    /**
     * Registers the listener that will be notified of changes to this panel.
     * @param listener The listener to be notified.
     */
    public void setListener(Listener listener) {
        mListener = listener;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        final Button ok = (Button) findViewById(R.id.btn_ok);
        final Button cancel = (Button) findViewById(R.id.btn_cancel);

        ok.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mListener == null) return;

                startAnimation();
                mListener.onConfirm();
            }
        });

        cancel.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mListener == null) return;

                mListener.onCancel();
            }
        });
    }

    /**
     * Starts animations for the widget.
     */
    public void startAnimation() {
        final Button ok = (Button) findViewById(R.id.btn_ok);
        final Button cancel = (Button) findViewById(R.id.btn_cancel);
        final ProgressBar spinner = (ProgressBar) findViewById(R.id.submit_spinner);

        // Animation for hiding the OK/Cancel buttons and showing the spinner.
        ObjectAnimator okAlphaAnim = ObjectAnimator.ofFloat(ok, View.ALPHA, 0.f);
        ObjectAnimator cancelAlphaAnim = ObjectAnimator.ofFloat(cancel, View.ALPHA, 0.f);
        ObjectAnimator spinnerAlphaAnim = ObjectAnimator.ofFloat(spinner, View.ALPHA, 0.f, 1.f);

        int translateX =
                getResources().getDimensionPixelSize(R.dimen.web_input_dialog_button_translation_x);
        ObjectAnimator okTranslateAnim =
                ObjectAnimator.ofFloat(ok, View.TRANSLATION_X, 0, translateX);
        ObjectAnimator cancelTranslateAnim =
                ObjectAnimator.ofFloat(cancel, View.TRANSLATION_X, 0, translateX);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(okAlphaAnim, cancelAlphaAnim, spinnerAlphaAnim, okTranslateAnim,
                cancelTranslateAnim);
        animatorSet.setDuration(250);

        animatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                spinner.setVisibility(View.VISIBLE);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                ok.setVisibility(View.INVISIBLE);
                cancel.setVisibility(View.INVISIBLE);
            }
        });

        animatorSet.start();
    }
}
