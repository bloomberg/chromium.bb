// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.ColorRes;
import android.support.annotation.IntDef;
import android.support.annotation.StringRes;
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

    private ImageView mNavigationButton;
    private ImageButton mSecurityButton;
    private TextView mVerboseStatusTextView;
    private View mSeparatorView;
    private View mStatusExtraSpace;

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

        assert mNavigationButton != null : "Missing navigation type view.";
    }

    /**
     * Specify navigation button image.
     */
    public void setNavigationIcon(Drawable image) {
        mNavigationButton.setImageDrawable(image);
    }

    /**
     * Select color of Separator view.
     */
    public void setSeparatorColor(@ColorRes int separatorColor) {
        mSeparatorView.setBackgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), separatorColor));
    }

    /**
     * Select color of verbose status text.
     */
    public void setVerboseStatusTextColor(@ColorRes int textColor) {
        mVerboseStatusTextView.setTextColor(
                ApiCompatibilityUtils.getColor(getResources(), textColor));
    }

    /**
     * Specify content of the verbose status text.
     */
    public void setVerboseStatusTextContent(@StringRes int content) {
        mVerboseStatusTextView.setText(content);
    }

    /**
     * Specify visibility of the verbose status text.
     */
    public void setVerboseStatusTextVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        mVerboseStatusTextView.setVisibility(visibility);
        mSeparatorView.setVisibility(visibility);
        mStatusExtraSpace.setVisibility(visibility);
    }

    /**
     * Specify width of the verbose status text.
     */
    public void setVerboseStatusTextWidth(int width) {
        mVerboseStatusTextView.setMaxWidth(width);
    }

    // TODO(ender): replace these with methods manipulating views directly.
    // Do not depend on these when creating new code!
    public ImageView getNavigationButton() {
        return mNavigationButton;
    }

    public ImageButton getSecurityButton() {
        return mSecurityButton;
    }

    public TextView getVerboseStatusTextView() {
        return mVerboseStatusTextView;
    }
}
