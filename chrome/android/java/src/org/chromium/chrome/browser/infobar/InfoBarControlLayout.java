// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.content.res.Resources;
import android.support.v7.widget.SwitchCompat;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RatingBar;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;

/**
 * Lays out a group of controls (e.g. switches, spinners, or additional text) for InfoBars that need
 * more than the normal pair of buttons.
 *
 * This class works with the {@link InfoBarLayout} to define a standard set of controls with
 * standardized spacings and text styling that gets laid out in grid form: https://crbug.com/543205
 *
 * Manually specified margins on the children managed by this layout are EXPLICITLY ignored to
 * enforce a uniform margin between controls across all InfoBar types.  Do NOT circumvent this
 * restriction with creative layout definitions.  If the layout algorithm doesn't work for your new
 * InfoBar, convince Chrome for Android's UX team to amend the master spec and then change the
 * layout algorithm to match.
 *
 * TODO(dfalcantara): Standardize all the possible control types.
 */
public final class InfoBarControlLayout extends ViewGroup {

    /**
     * Extends the regular LayoutParams by determining where a control should be located.
     */
    @VisibleForTesting
    static final class ControlLayoutParams extends LayoutParams {
        public int start;
        public int top;
        public int columnsRequired;
        private boolean mMustBeFullWidth;

        /**
         * Stores values required for laying out this ViewGroup's children.
         *
         * This is set up as a private method to mitigate attempts at adding controls to the layout
         * that aren't provided by the InfoBarControlLayout.
         */
        private ControlLayoutParams() {
            super(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        }
    }

    private final int mMaxInfoBarWidth;
    private final int mMarginBetweenRows;
    private final int mMarginBetweenColumns;

    /**
     * Do not call this method directly; use {@link InfoBarLayout#addControlLayout()}.
     */
    InfoBarControlLayout(Context context) {
        super(context);

        Resources resources = context.getResources();
        mMaxInfoBarWidth = resources.getDimensionPixelSize(R.dimen.infobar_max_width);
        mMarginBetweenRows =
                resources.getDimensionPixelSize(R.dimen.infobar_control_margin_between_rows);
        mMarginBetweenColumns =
                resources.getDimensionPixelSize(R.dimen.infobar_control_margin_between_columns);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        assert getLayoutParams().height == LayoutParams.WRAP_CONTENT
                : "Height of this layout cannot be constrained.";

        int fullWidth = MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED
                ? mMaxInfoBarWidth : MeasureSpec.getSize(widthMeasureSpec);
        int columnWidth = Math.max(0, (fullWidth - mMarginBetweenColumns) / 2);

        int atMostFullWidthSpec = MeasureSpec.makeMeasureSpec(fullWidth, MeasureSpec.AT_MOST);
        int exactlyFullWidthSpec = MeasureSpec.makeMeasureSpec(fullWidth, MeasureSpec.EXACTLY);
        int exactlyColumnWidthSpec = MeasureSpec.makeMeasureSpec(columnWidth, MeasureSpec.EXACTLY);
        int unspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);

        // Figure out how many columns each child requires.
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            measureChild(child, atMostFullWidthSpec, unspecifiedSpec);

            if (child.getMeasuredWidth() <= columnWidth
                    && !getControlLayoutParams(child).mMustBeFullWidth) {
                getControlLayoutParams(child).columnsRequired = 1;
            } else {
                getControlLayoutParams(child).columnsRequired = 2;
            }
        }

        // Pack all the children as tightly into rows as possible without changing their ordering.
        // Stretch out column-width controls if either it is the last control or the next one is
        // a full-width control.
        for (int i = 0; i < getChildCount(); i++) {
            ControlLayoutParams lp = getControlLayoutParams(getChildAt(i));

            if (i == getChildCount() - 1) {
                lp.columnsRequired = 2;
            } else {
                ControlLayoutParams nextLp = getControlLayoutParams(getChildAt(i + 1));
                if (lp.columnsRequired + nextLp.columnsRequired > 2) {
                    // This control is too big to place with the next child.
                    lp.columnsRequired = 2;
                } else {
                    // This and the next control fit on the same line.  Skip placing the next child.
                    i++;
                }
            }
        }

        // Measure all children, assuming they all have to fit within the width of the layout.
        // Height is unconstrained.
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            ControlLayoutParams lp = getControlLayoutParams(child);
            int spec = lp.columnsRequired == 1 ? exactlyColumnWidthSpec : exactlyFullWidthSpec;
            measureChild(child, spec, unspecifiedSpec);
        }

        // Pack all the children as tightly into rows as possible without changing their ordering.
        int layoutHeight = 0;
        int nextChildStart = 0;
        int nextChildTop = 0;
        int currentRowHeight = 0;
        int columnsAvailable = 2;
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            ControlLayoutParams lp = getControlLayoutParams(child);

            // If there isn't enough room left for the control, move to the next row.
            if (columnsAvailable < lp.columnsRequired) {
                layoutHeight += currentRowHeight + mMarginBetweenRows;
                nextChildStart = 0;
                nextChildTop = layoutHeight;
                currentRowHeight = 0;
                columnsAvailable = 2;
            }

            lp.top = nextChildTop;
            lp.start = nextChildStart;
            currentRowHeight = Math.max(currentRowHeight, child.getMeasuredHeight());
            columnsAvailable -= lp.columnsRequired;
            nextChildStart += lp.columnsRequired * (columnWidth + mMarginBetweenColumns);
        }

        // Compute the ViewGroup's height, accounting for the final row's height.
        layoutHeight += currentRowHeight;
        setMeasuredDimension(resolveSize(fullWidth, widthMeasureSpec),
                resolveSize(layoutHeight, heightMeasureSpec));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        int width = right - left;
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);

        // Child positions were already determined during the measurement pass.
        for (int childIndex = 0; childIndex < getChildCount(); childIndex++) {
            View child = getChildAt(childIndex);
            int childLeft = getControlLayoutParams(child).start;
            if (isRtl) childLeft = width - childLeft - child.getMeasuredWidth();

            int childTop = getControlLayoutParams(child).top;
            int childRight = childLeft + child.getMeasuredWidth();
            int childBottom = childTop + child.getMeasuredHeight();
            child.layout(childLeft, childTop, childRight, childBottom);
        }
    }

    /**
     * Creates a standard toggle switch and adds it to the layout.
     *
     * -------------------------------------------------
     * | ICON | MESSAGE                       | TOGGLE |
     * -------------------------------------------------
     * If an icon is not provided, the ImageView that would normally show it is hidden.
     *
     * @param iconResourceId ID of the drawable to use for the icon, or 0 to hide the ImageView.
     * @param toggleMessage  Message to display for the toggle.
     * @param toggleId       ID to use for the toggle.
     * @param isChecked      Whether the toggle should start off checked.
     */
    public View addSwitch(
            int iconResourceId, CharSequence toggleMessage, int toggleId, boolean isChecked) {
        LinearLayout switchLayout = (LinearLayout) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_toggle, this, false);
        addView(switchLayout, new ControlLayoutParams());

        ImageView iconView = (ImageView) switchLayout.findViewById(R.id.control_toggle_icon);
        if (iconResourceId == 0) {
            switchLayout.removeView(iconView);
        } else {
            iconView.setImageResource(iconResourceId);
        }

        TextView messageView = (TextView) switchLayout.findViewById(R.id.control_toggle_message);
        messageView.setText(toggleMessage);

        SwitchCompat switchView =
                (SwitchCompat) switchLayout.findViewById(R.id.control_toggle_switch);
        switchView.setId(toggleId);
        switchView.setChecked(isChecked);

        return switchLayout;
    }

    /**
     * Creates a standard spinner and adds it to the layout.
     *
     * The layout currently consists of just the Spinner control, but this may change as the spec
     * is updated.
     *
     * TODO(dfalcantara): Standardize the spinner text colors and spacings by standardizing the
     *                    ArrayAdapter that gets attached to the spinner.  https://crbug.com/543205
     */
    public View addSpinner(int spinnerId) {
        Spinner spinner = (Spinner) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_spinner, this, false);
        addView(spinner, new ControlLayoutParams());
        spinner.setId(spinnerId);
        return spinner;
    }

    /**
     * Creates and adds a full-width control with additional text describing what an InfoBar is for.
     */
    public View addDescription(CharSequence message) {
        ControlLayoutParams params = new ControlLayoutParams();
        params.mMustBeFullWidth = true;

        TextView descriptionView = (TextView) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_description, this, false);
        addView(descriptionView, params);

        descriptionView.setText(message);
        descriptionView.setMovementMethod(LinkMovementMethod.getInstance());
        return descriptionView;
    }

    /**
     * Creates and adds a control that shows a review rating score.
     *
     * @param rating Fractional rating out of 5 stars.
     */
    public View addRatingBar(float rating) {
        View ratingLayout = LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_rating, this, false);
        addView(ratingLayout, new ControlLayoutParams());

        RatingBar ratingView = (RatingBar) ratingLayout.findViewById(R.id.control_rating);
        ratingView.setRating(rating);
        return ratingView;
    }

    /**
     * Do NOT call this method directly from outside {@link InfoBarLayout#InfoBarLayout()}.
     *
     * Adds a full-width control showing the main InfoBar message.  For other text, you should call
     * {@link InfoBarControlLayout#addDescription(CharSequence)} instead.
     */
    TextView addMainMessage(CharSequence mainMessage) {
        ControlLayoutParams params = new ControlLayoutParams();
        params.mMustBeFullWidth = true;

        TextView messageView = (TextView) LayoutInflater.from(getContext()).inflate(
                R.layout.infobar_control_message, this, false);
        addView(messageView, params);

        messageView.setText(mainMessage);
        messageView.setMovementMethod(LinkMovementMethod.getInstance());
        return messageView;
    }

    /**
     * @return The {@link ControlLayoutParams} for the given child.
     */
    @VisibleForTesting
    static ControlLayoutParams getControlLayoutParams(View child) {
        return (ControlLayoutParams) child.getLayoutParams();
    }

}
