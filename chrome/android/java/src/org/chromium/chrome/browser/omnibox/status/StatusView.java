// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.chromium.chrome.browser.toolbar.top.ToolbarPhone.URL_FOCUS_CHANGE_ANIMATION_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.support.annotation.ColorRes;
import android.support.annotation.DrawableRes;
import android.support.annotation.IntDef;
import android.support.annotation.StringRes;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * StatusView is a location bar's view displaying status (icons and/or text).
 */
public class StatusView extends LinearLayout {
    /**
     * Specifies the types of buttons shown to signify different types of navigation elements.
     */
    @IntDef({NavigationButtonType.PAGE, NavigationButtonType.MAGNIFIER, NavigationButtonType.EMPTY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NavigationButtonType {
        int PAGE = 0;
        int MAGNIFIER = 1;
        int EMPTY = 2;
    }

    /**
     * Specifies which button should be shown in location bar, if any.
     */
    @IntDef({StatusButtonType.NONE, StatusButtonType.SECURITY_ICON,
            StatusButtonType.NAVIGATION_ICON})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StatusButtonType {
        /** No button should be shown. */
        int NONE = 0;
        /** Security button should be shown (includes offline icon). */
        int SECURITY_ICON = 1;
        /** Navigation button should be shown. */
        int NAVIGATION_ICON = 2;
    }

    private View mLocationBarIcon;
    private ImageView mNavigationButton;
    private ImageButton mSecurityButton;
    private TextView mVerboseStatusTextView;
    private View mSeparatorView;
    private View mStatusExtraSpace;
    private View mLocationBarButtonContainer;

    private boolean mAnimationsEnabled;

    private StatusViewAnimator mLocationBarIconActiveAnimator;
    private StatusViewAnimator mLocationBarSecurityButtonShowAnimator;
    private StatusViewAnimator mLocationBarNavigationIconShowAnimator;
    private StatusViewAnimator mLocationBarClearAnimator;

    /**
     * Class animating transition between FrameLayout children.
     * Can take any number of child components; ensures that the
     * - component supplied as first (the 'target' component) will be visible and
     * - every other supplied component will be made invisible at the end of animation.
     */
    private final class StatusViewAnimator {
        private final AnimatorSet mAnimator;
        // Target view that will be displayed at the end of animation.
        private final View[] mTargetViews;
        // Set of source views that should be made invisible at the end.
        private final View[] mSourceViews;

        /**
         * Adapter updating start and end states of animated elements.
         * Ensures that target elements are made visible and source elements
         * are no longer presented at the end of animation.
         */
        private final class StatusViewAnimatorAdapter extends AnimatorListenerAdapter {
            @Override
            public void onAnimationEnd(Animator animation) {
                for (View source : mSourceViews) {
                    source.setVisibility(View.INVISIBLE);
                }
            }

            @Override
            public void onAnimationStart(Animator animation) {
                for (View target : mTargetViews) {
                    target.setVisibility(View.VISIBLE);
                }
            }
        }

        public StatusViewAnimator(View[] targets, View[] sources) {
            mAnimator = new AnimatorSet();
            mTargetViews = targets;
            mSourceViews = sources;

            AnimatorSet.Builder b = null;

            // Phase in these objects
            for (View view : targets) {
                if (b == null) {
                    b = mAnimator.play(ObjectAnimator.ofFloat(view, View.ALPHA, 1));
                } else {
                    b.with(ObjectAnimator.ofFloat(view, View.ALPHA, 1));
                }
            }

            // Phase out these objects
            for (View view : sources) {
                if (b == null) {
                    b = mAnimator.play(ObjectAnimator.ofFloat(view, View.ALPHA, 0));
                } else {
                    b.with(ObjectAnimator.ofFloat(view, View.ALPHA, 0));
                }
            }
            mAnimator.addListener(new StatusViewAnimatorAdapter());
        }

        public void cancel() {
            if (mAnimator.isRunning()) mAnimator.cancel();
        }

        public void animate(long duration) {
            mAnimator.setDuration(duration);
            mAnimator.start();
        }
    }

    public StatusView(Context context, AttributeSet attributes) {
        super(context, attributes);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mNavigationButton = findViewById(R.id.navigation_button);
        mSecurityButton = findViewById(R.id.security_button);
        mVerboseStatusTextView = findViewById(R.id.location_bar_verbose_status);
        mSeparatorView = findViewById(R.id.location_bar_verbose_status_separator);
        mStatusExtraSpace = findViewById(R.id.location_bar_verbose_status_extra_space);
        mLocationBarIcon = findViewById(R.id.location_bar_icon);
        assert mNavigationButton != null : "Missing navigation type view.";

        configureLocationBarIconAnimations();
    }

    /**
     * Specify object to receive click events.
     *
     * @param listener Instance of View.OnClickListener or null.
     */
    public void setStatusClickListener(View.OnClickListener listener) {
        mNavigationButton.setOnClickListener(listener);
        mSecurityButton.setOnClickListener(listener);
        mVerboseStatusTextView.setOnClickListener(listener);
    }

    /**
     * Configure animations used to cross-fade status icon.
     */
    void configureLocationBarIconAnimations() {
        // Animation presenting Security button and hiding all others.
        mLocationBarSecurityButtonShowAnimator = new StatusViewAnimator(
                new View[] {mSecurityButton}, new View[] {mNavigationButton});

        // Animation presenting Navigation button and hiding all others.
        mLocationBarNavigationIconShowAnimator = new StatusViewAnimator(
                new View[] {mNavigationButton}, new View[] {mSecurityButton});

        // Animation clearing up all location bar icons.
        mLocationBarClearAnimator = new StatusViewAnimator(
                new View[] {}, new View[] {mNavigationButton, mSecurityButton});
    }

    /**
     * Toggle use of animations.
     */
    void setAnimationsEnabled(boolean enabled) {
        mAnimationsEnabled = enabled;
    }

    /**
     * Specify status bar button type.
     */
    void setStatusButtonType(@StatusButtonType int type) {
        // Terminate any animation currently taking place.
        // We won't be terminating animation that is currently presenting the
        // same object - property mechanism will prevent this.
        if (mLocationBarIconActiveAnimator != null) {
            mLocationBarIconActiveAnimator.cancel();
        }

        switch (type) {
            case StatusButtonType.SECURITY_ICON:
                mLocationBarIconActiveAnimator = mLocationBarSecurityButtonShowAnimator;
                mLocationBarIcon.setVisibility(View.VISIBLE);
                break;
            case StatusButtonType.NAVIGATION_ICON:
                mLocationBarIconActiveAnimator = mLocationBarNavigationIconShowAnimator;
                mLocationBarIcon.setVisibility(View.VISIBLE);
                break;
            case StatusButtonType.NONE:
            default:
                mLocationBarIconActiveAnimator = mLocationBarClearAnimator;
                // TODO(ender): This call should only be invoked once animation completes. Fix
                // animation system and hide the view once icons are invisible.
                mLocationBarIcon.setVisibility(View.GONE);
                return;
        }

        mLocationBarIconActiveAnimator.animate(
                mAnimationsEnabled ? URL_FOCUS_CHANGE_ANIMATION_DURATION_MS : 0);
    }

    /**
     * Specify navigation button image.
     */
    void setNavigationIcon(Drawable image) {
        mNavigationButton.setImageDrawable(image);
    }

    /**
     * Specify security icon image.
     */
    public void setSecurityIcon(@DrawableRes int imageRes) {
        if (imageRes != 0) {
            mSecurityButton.setImageResource(imageRes);
        } else {
            mSecurityButton.setImageDrawable(null);
        }
    }

    /**
     * Specify security icon tint color.
     */
    public void setSecurityIconTint(@ColorRes int colorRes) {
        // Note: There's no need to apply this each time icon changes:
        // Subsequent calls to setImageDrawable will automatically
        // mutate the drawable and apply the specified tint and tint mode.
        ColorStateList list = null;
        if (colorRes != 0) {
            list = AppCompatResources.getColorStateList(getContext(), colorRes);
        }
        ApiCompatibilityUtils.setImageTintList(mSecurityButton, list);
    }

    /**
     * Specify content description for security icon.
     */
    public void setSecurityIconDescription(@StringRes int descriptionRes) {
        String description = null;
        if (descriptionRes != 0) {
            description = getResources().getString(descriptionRes);
        }
        mSecurityButton.setContentDescription(description);
    }

    /**
     * Select color of Separator view.
     */
    void setSeparatorColor(@ColorRes int separatorColor) {
        mSeparatorView.setBackgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), separatorColor));
    }

    /**
     * Select color of verbose status text.
     */
    void setVerboseStatusTextColor(@ColorRes int textColor) {
        mVerboseStatusTextView.setTextColor(
                ApiCompatibilityUtils.getColor(getResources(), textColor));
    }

    /**
     * Specify content of the verbose status text.
     */
    void setVerboseStatusTextContent(@StringRes int content) {
        mVerboseStatusTextView.setText(content);
    }

    /**
     * Specify visibility of the verbose status text.
     */
    void setVerboseStatusTextVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        mVerboseStatusTextView.setVisibility(visibility);
        mSeparatorView.setVisibility(visibility);
        mStatusExtraSpace.setVisibility(visibility);
    }

    /**
     * Specify width of the verbose status text.
     */
    void setVerboseStatusTextWidth(int width) {
        mVerboseStatusTextView.setMaxWidth(width);
    }

    // TODO(ender): replace these with methods manipulating views directly.
    // Do not depend on these when creating new code!
    View getNavigationButton() {
        return mNavigationButton;
    }

    View getSecurityButton() {
        return mSecurityButton;
    }

    TextView getVerboseStatusTextView() {
        return mVerboseStatusTextView;
    }
}
