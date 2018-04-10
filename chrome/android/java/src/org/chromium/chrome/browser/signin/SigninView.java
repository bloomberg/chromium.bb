// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Animatable2;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.annotation.Nullable;
import android.support.graphics.drawable.Animatable2Compat;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.ui.widget.ButtonCompat;

/** View that wraps signin screen and caches references to UI elements. */
public class SigninView extends LinearLayout {
    private SigninScrollView mScrollView;
    private ImageView mHeaderImage;
    private TextView mTitle;
    private View mAccountPicker;
    private ImageView mAccountImage;
    private TextView mAccountName;
    private TextView mAccountEmail;
    private TextView mSyncDescription;
    private TextView mPersonalizationDescription;
    private TextView mGoogleServicesDescription;
    private TextView mDetailsDescription;
    private ButtonCompat mAcceptButton;
    private Button mRefuseButton;
    private Button mMoreButton;
    private View mAcceptButtonEndPadding;

    private Runnable mStopAnimationsRunnable;

    public SigninView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = (SigninScrollView) findViewById(R.id.signin_scroll_view);
        mHeaderImage = (ImageView) findViewById(R.id.signin_header_image);
        mTitle = (TextView) findViewById(R.id.signin_title);
        mAccountPicker = findViewById(R.id.signin_account_picker);
        mAccountImage = (ImageView) findViewById(R.id.account_image);
        mAccountName = (TextView) findViewById(R.id.account_name);
        mAccountEmail = (TextView) findViewById(R.id.account_email);
        mSyncDescription = (TextView) findViewById(R.id.signin_sync_description);
        mPersonalizationDescription =
                (TextView) findViewById(R.id.signin_personalization_description);
        mGoogleServicesDescription =
                (TextView) findViewById(R.id.signin_google_services_description);
        mDetailsDescription = (TextView) findViewById(R.id.signin_details_description);
        mAcceptButton = (ButtonCompat) findViewById(R.id.positive_button);
        mRefuseButton = (Button) findViewById(R.id.negative_button);
        mMoreButton = (Button) findViewById(R.id.more_button);
        mAcceptButtonEndPadding = findViewById(R.id.positive_button_end_padding);
    }

    public SigninScrollView getScrollView() {
        return mScrollView;
    }

    public ImageView getHeaderImage() {
        return mHeaderImage;
    }

    public TextView getTitleView() {
        return mTitle;
    }

    public View getAccountPickerView() {
        return mAccountPicker;
    }

    public ImageView getAccountImageView() {
        return mAccountImage;
    }

    public TextView getAccountNameView() {
        return mAccountName;
    }

    public TextView getAccountEmailView() {
        return mAccountEmail;
    }

    public TextView getSyncDescriptionView() {
        return mSyncDescription;
    }

    public TextView getPersonalizationDescriptionView() {
        return mPersonalizationDescription;
    }

    public TextView getGoogleServicesDescriptionView() {
        return mGoogleServicesDescription;
    }

    public TextView getDetailsDescriptionView() {
        return mDetailsDescription;
    }

    public ButtonCompat getAcceptButton() {
        return mAcceptButton;
    }

    public Button getRefuseButton() {
        return mRefuseButton;
    }

    public Button getMoreButton() {
        return mMoreButton;
    }

    public View getAcceptButtonEndPadding() {
        return mAcceptButtonEndPadding;
    }

    void startAnimations() {
        Drawable headerImage = getHeaderImage().getDrawable();
        if (headerImage instanceof Animatable2Compat) {
            Animatable2Compat animatable = (Animatable2Compat) headerImage;
            Animatable2Compat.AnimationCallback animationCallback =
                    new Animatable2Compat.AnimationCallback() {
                        @Override
                        public void onAnimationEnd(Drawable drawable) {
                            restartAnimations(animatable);
                        }
                    };
            mStopAnimationsRunnable = () -> {
                animatable.unregisterAnimationCallback(animationCallback);
                animatable.stop();
            };
            animatable.registerAnimationCallback(animationCallback);
            animatable.start();
            return;
        }

        // Animatable2 was added in API level 23 (Android M).
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
        if (headerImage instanceof Animatable2) {
            Animatable2 animatable = (Animatable2) headerImage;
            Animatable2.AnimationCallback animationCallback = new Animatable2.AnimationCallback() {
                @Override
                @TargetApi(Build.VERSION_CODES.M)
                public void onAnimationEnd(Drawable drawable) {
                    restartAnimations(animatable);
                }
            };
            mStopAnimationsRunnable = () -> {
                animatable.unregisterAnimationCallback(animationCallback);
                animatable.stop();
            };
            animatable.registerAnimationCallback(animationCallback);
            animatable.start();
        }
    }

    void stopAnimations() {
        if (mStopAnimationsRunnable == null) return;
        mStopAnimationsRunnable.run();
        mStopAnimationsRunnable = null;
    }

    private void restartAnimations(Animatable animatable) {
        // In some cases (Animatable2Compat from Support Library, etc.), onAnimationEnd is invoked
        // before the animation is marked as ended, so calls to start() here may be ignored.
        // This issue is worked around by deferring the call to start().
        ThreadUtils.postOnUiThread(() -> {
            if (mStopAnimationsRunnable == null) return;
            animatable.start();
        });
    }
}
