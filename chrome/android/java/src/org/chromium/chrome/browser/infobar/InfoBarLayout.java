// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.List;

/**
 * Layout that arranges an InfoBar's views. An InfoBarLayout consists of:
 * - A message describing the action that the user can take.
 * - A close button on the right side.
 * - (optional) An icon representing the infobar's purpose on the left side.
 * - (optional) Additional "custom" views (e.g. a checkbox and text, or a pair of spinners)
 * - (optional) One or two buttons with text at the bottom.
 *
 * When adding custom views, widths and heights defined in the LayoutParams will be ignored.
 * However, setting a minimum width in another way, like TextView.getMinWidth(), should still be
 * obeyed.
 *
 * Logic for what happens when things are clicked should be implemented by the InfoBarView.
 */
public final class InfoBarLayout extends ViewGroup implements View.OnClickListener {

    /**
     * Parameters used for laying out children.
     */
    private static class LayoutParams extends ViewGroup.LayoutParams {

        public int startMargin;
        public int endMargin;
        public int topMargin;
        public int bottomMargin;

        // Where this view will be laid out. These values are assigned in onMeasure() and used in
        // onLayout().
        public int start;
        public int top;

        LayoutParams(int startMargin, int topMargin, int endMargin, int bottomMargin) {
            super(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            this.startMargin = startMargin;
            this.topMargin = topMargin;
            this.endMargin = endMargin;
            this.bottomMargin = bottomMargin;
        }
    }

    private static class Group {
        public View[] views;

        /**
         * The gravity of each view in Group. Must be either Gravity.START, Gravity.END, or
         * Gravity.FILL_HORIZONTAL.
         */
        public int gravity = Gravity.START;

        /** Whether the views are vertically stacked. */
        public boolean isStacked;

        Group(View... views) {
            this.views = views;
        }

        static View[] filterNullViews(View... views) {
            ArrayList<View> viewsList = new ArrayList<View>();
            for (View v : views) {
                if (v != null) viewsList.add(v);
            }
            return viewsList.toArray(new View[viewsList.size()]);
        }

        void setHorizontalMode(int horizontalSpacing, int startMargin, int endMargin) {
            isStacked = false;
            for (int i = 0; i < views.length; i++) {
                LayoutParams lp = (LayoutParams) views[i].getLayoutParams();
                lp.startMargin = i == 0 ? startMargin : horizontalSpacing;
                lp.topMargin = 0;
                lp.endMargin = i == views.length - 1 ? endMargin : 0;
                lp.bottomMargin = 0;
            }
        }

        void setVerticalMode(int verticalSpacing, int bottomMargin) {
            isStacked = true;
            for (int i = 0; i < views.length; i++) {
                LayoutParams lp = (LayoutParams) views[i].getLayoutParams();
                lp.startMargin = 0;
                lp.topMargin = i == 0 ? 0 : verticalSpacing;
                lp.endMargin = 0;
                lp.bottomMargin = i == views.length - 1 ? bottomMargin : 0;
            }
        }
    }

    private final int mSmallIconSize;
    private final int mSmallIconMargin;
    private final int mBigIconSize;
    private final int mBigIconMargin;
    private final int mMarginAboveButtonGroup;
    private final int mMarginAboveControlGroups;
    private final int mMargin;
    private final int mMinWidth;
    private final int mAccentColor;

    private final InfoBarView mInfoBarView;
    private final ImageButton mCloseButton;
    private final InfoBarControlLayout mMessageLayout;
    private final List<InfoBarControlLayout> mControlLayouts;

    private TextView mMessageTextView;
    private ImageView mIconView;
    private ButtonCompat mPrimaryButton;
    private Button mSecondaryButton;
    private View mCustomButton;

    private Group mMainGroup;
    private Group mButtonGroup;

    private boolean mIsUsingBigIcon;
    private CharSequence mMessageMainText;
    private String mMessageLinkText;

    /**
     * These values are used during onMeasure() to track where the next view will be placed.
     *
     * mWidth is the infobar width.
     * [mStart, mEnd) is the range of unoccupied space on the current row.
     * mTop and mBottom are the top and bottom of the current row.
     *
     * These values, along with a view's gravity, are used to position the next view.
     * These values are updated after placing a view and after starting a new row.
     */
    private int mWidth;
    private int mStart;
    private int mEnd;
    private int mTop;
    private int mBottom;

    /**
     * Constructs a layout for the specified InfoBar. After calling this, be sure to set the
     * message, the buttons, and/or the custom content using setMessage(), setButtons(), and
     * setCustomContent().
     *
     * @param context The context used to render.
     * @param infoBarView InfoBarView that listens to events.
     * @param iconResourceId ID of the icon to use for the InfoBar.
     * @param iconBitmap Bitmap for the icon to use, if the resource ID wasn't passed through.
     * @param message The message to show in the infobar.
     */
    public InfoBarLayout(Context context, InfoBarView infoBarView, int iconResourceId,
            Bitmap iconBitmap, CharSequence message) {
        super(context);
        mControlLayouts = new ArrayList<InfoBarControlLayout>();

        mInfoBarView = infoBarView;

        // Grab the dimensions.
        Resources res = getResources();
        mSmallIconSize = res.getDimensionPixelSize(R.dimen.infobar_small_icon_size);
        mSmallIconMargin = res.getDimensionPixelSize(R.dimen.infobar_small_icon_margin);
        mBigIconSize = res.getDimensionPixelSize(R.dimen.infobar_big_icon_size);
        mBigIconMargin = res.getDimensionPixelSize(R.dimen.infobar_big_icon_margin);
        mMarginAboveButtonGroup =
                res.getDimensionPixelSize(R.dimen.infobar_margin_above_button_row);
        mMarginAboveControlGroups =
                res.getDimensionPixelSize(R.dimen.infobar_margin_above_control_groups);
        mMargin = res.getDimensionPixelOffset(R.dimen.infobar_margin);
        mMinWidth = res.getDimensionPixelSize(R.dimen.infobar_min_width);
        mAccentColor = ApiCompatibilityUtils.getColor(res, R.color.infobar_accent_blue);

        // Set up the close button. Apply padding so it has a big touch target.
        mCloseButton = new ImageButton(context);
        mCloseButton.setId(R.id.infobar_close_button);
        mCloseButton.setImageResource(R.drawable.btn_close);
        TypedArray a = getContext().obtainStyledAttributes(
                new int [] {R.attr.selectableItemBackground});
        Drawable closeButtonBackground = a.getDrawable(0);
        a.recycle();
        mCloseButton.setBackground(closeButtonBackground);
        mCloseButton.setPadding(mMargin, mMargin, mMargin, mMargin);
        mCloseButton.setOnClickListener(this);
        mCloseButton.setContentDescription(res.getString(R.string.infobar_close));
        mCloseButton.setLayoutParams(new LayoutParams(0, -mMargin, -mMargin, -mMargin));

        // Set up the icon.
        if (iconResourceId != 0 || iconBitmap != null) {
            mIconView = new ImageView(context);
            if (iconResourceId != 0) {
                mIconView.setImageResource(iconResourceId);
            } else if (iconBitmap != null) {
                mIconView.setImageBitmap(iconBitmap);
            }
            mIconView.setLayoutParams(new LayoutParams(0, 0, mSmallIconMargin, 0));
            mIconView.getLayoutParams().width = mSmallIconSize;
            mIconView.getLayoutParams().height = mSmallIconSize;
            mIconView.setFocusable(false);
        }

        // Set up the message view.
        mMessageMainText = message;
        mMessageLayout = new InfoBarControlLayout(context);
        mMessageTextView = mMessageLayout.addMainMessage(prepareMainMessageString());
    }

    /**
     * Returns the {@link TextView} corresponding to the main InfoBar message.
     */
    TextView getMessageTextView() {
        return mMessageTextView;
    }

    /**
     * Returns the {@link InfoBarControlLayout} containing the TextView showing the main InfoBar
     * message and associated controls, which is sandwiched between its icon and close button.
     */
    InfoBarControlLayout getMessageLayout() {
        return mMessageLayout;
    }

    /**
     * Sets the message to show on the infobar.
     * TODO(dfalcantara): Do some magic here to determine if TextViews need to have line spacing
     *                    manually added.  Android changed when these values were applied between
     *                    KK and L: https://crbug.com/543205
     */
    public void setMessage(CharSequence message) {
        mMessageMainText = message;
        mMessageTextView.setText(prepareMainMessageString());
    }

    /**
     * Sets the message to show for a link in the message, if an InfoBar requires a link
     * (e.g. "Learn more").
     */
    public void setMessageLinkText(String linkText) {
        mMessageLinkText = linkText;
        mMessageTextView.setText(prepareMainMessageString());
    }

    /**
     * Adds an {@link InfoBarControlLayout} to house additional InfoBar controls, like toggles and
     * spinners.
     */
    public InfoBarControlLayout addControlLayout() {
        InfoBarControlLayout controlLayout = new InfoBarControlLayout(getContext());
        mControlLayouts.add(controlLayout);
        return controlLayout;
    }

    /**
     * Adds one or two buttons to the layout.
     *
     * @param primaryText Text for the primary button.
     * @param secondaryText Text for the secondary button, or null if there isn't a second button.
     */
    public void setButtons(String primaryText, String secondaryText) {
        if (TextUtils.isEmpty(primaryText)) return;

        mPrimaryButton = new ButtonCompat(getContext(), mAccentColor);
        mPrimaryButton.setId(R.id.button_primary);
        mPrimaryButton.setOnClickListener(this);
        mPrimaryButton.setText(primaryText);
        mPrimaryButton.setTextColor(Color.WHITE);

        if (TextUtils.isEmpty(secondaryText)) return;

        mSecondaryButton = ButtonCompat.createBorderlessButton(getContext());
        mSecondaryButton.setId(R.id.button_secondary);
        mSecondaryButton.setOnClickListener(this);
        mSecondaryButton.setText(secondaryText);
        mSecondaryButton.setTextColor(mAccentColor);
    }

    /** Adds a custom view to show in the button row. */
    public void setCustomViewInButtonRow(View view) {
        mCustomButton = view;
    }

    /**
     * Adjusts styling to account for the big icon layout.
     */
    public void setIsUsingBigIcon() {
        mIsUsingBigIcon = true;

        LayoutParams lp = (LayoutParams) mIconView.getLayoutParams();
        lp.width = mBigIconSize;
        lp.height = mBigIconSize;
        lp.endMargin = mBigIconMargin;

        Resources res = getContext().getResources();
        String typeface = res.getString(R.string.infobar_message_typeface);
        int textStyle = res.getInteger(R.integer.infobar_message_textstyle);
        float textSize = res.getDimension(R.dimen.infobar_big_icon_message_size);
        mMessageTextView.setTypeface(Typeface.create(typeface, textStyle));
        mMessageTextView.setSingleLine();
        mMessageTextView.setTextSize(TypedValue.COMPLEX_UNIT_PX, textSize);
    }

    /**
     * Returns the primary button, or null if it doesn't exist.
     */
    public ButtonCompat getPrimaryButton() {
        return mPrimaryButton;
    }

    /**
     * Returns the icon, or null if it doesn't exist.
     */
    public ImageView getIcon() {
        return mIconView;
    }

    /**
     * Must be called after the message, buttons, and custom content have been set, and before the
     * first call to onMeasure().
     */
    void onContentCreated() {
        mMainGroup = new Group(mMessageLayout);

        View[] buttons = Group.filterNullViews(mCustomButton, mSecondaryButton, mPrimaryButton);
        if (buttons.length != 0) mButtonGroup = new Group(buttons);

        // Add the child views in the desired focus order.
        if (mIconView != null) addView(mIconView);
        for (View v : mMainGroup.views) addView(v);
        for (View v : mControlLayouts) addView(v);
        if (mButtonGroup != null) {
            for (View v : mButtonGroup.views) addView(v);
        }
        addView(mCloseButton);
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new LayoutParams(0, 0, 0, 0);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        // Place all the views in the positions already determined during onMeasure().
        int width = right - left;
        boolean isRtl = ApiCompatibilityUtils.isLayoutRtl(this);

        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            int childLeft = lp.start;
            int childRight = lp.start + child.getMeasuredWidth();

            if (isRtl) {
                int tmp = width - childRight;
                childRight = width - childLeft;
                childLeft = tmp;
            }

            child.layout(childLeft, lp.top, childRight, lp.top + child.getMeasuredHeight());
        }
    }

    /**
     * Measures *and* assigns positions to all of the views in the infobar. These positions are
     * saved in each view's LayoutParams (lp.start and lp.top) and used during onLayout(). All of
     * the interesting logic happens inside onMeasure(); onLayout() just assigns the already-
     * determined positions and mirrors everything for RTL, if needed.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        assert getLayoutParams().height == LayoutParams.WRAP_CONTENT
                : "InfoBar heights cannot be constrained.";

        // Measure all children without imposing any size constraints on them. This determines how
        // big each child wants to be.
        int unspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        for (int i = 0; i < getChildCount(); i++) {
            measureChild(getChildAt(i), unspecifiedSpec, unspecifiedSpec);
        }

        // Avoid overlapping views, division by zero, infinite heights, and other fun problems that
        // could arise with extremely narrow infobars.
        mWidth = Math.max(MeasureSpec.getSize(widthMeasureSpec), mMinWidth);
        mTop = mBottom = 0;
        placeGroups();

        setMeasuredDimension(mWidth, resolveSize(mBottom, heightMeasureSpec));
    }

    /**
     * Assigns positions to all of the views in the infobar. The icon enforces a start margin on
     * all control groups that aren't the final button group.  The message and close button are
     * placed on the main row, with control groups placed beneath them.
     */
    private void placeGroups() {
        // Enforce the general top-padding on the InfoBar controls.
        mTop = mBottom = mMargin;

        // Place the icon, which forces all control groups to be inset by the size of the icon.
        if (mIconView == null) {
            startRowWithIconMargin();
        } else {
            startRowWithoutIconMargin();
            placeChild(mIconView, Gravity.START);
        }

        // Place the close button.
        placeChild(mCloseButton, Gravity.END);

        // Place the main control group, which is sandwiched between the icon and the close button.
        // When laid out vertically, control groups ignore where the close button's bottom is, but
        // that value helps define how tall the InfoBar is.  Save the bottom before overwriting it.
        int closeButtonBottom = mBottom;
        mTop = mBottom = mMargin;
        placeChild(mMessageLayout, Gravity.FILL_HORIZONTAL);

        // Place each of the control groups.
        for (InfoBarControlLayout layout : mControlLayouts) {
            startRowWithIconMargin();
            mTop = mBottom + mMarginAboveControlGroups;
            mBottom = mTop;
            placeChild(layout, Gravity.FILL_HORIZONTAL);
        }

        // Place the buttons.
        if (mButtonGroup != null) {
            startRowWithoutIconMargin();
            mTop = mBottom + mMarginAboveButtonGroup;
            mBottom = mTop;

            updateButtonGroupLayoutProperties();
            placeButtonGroup(mButtonGroup);

            if (mCustomButton != null) {
                // The custom button is start-aligned to its parent.
                LayoutParams primaryButtonLP = (LayoutParams) mPrimaryButton.getLayoutParams();
                LayoutParams customButtonLP = (LayoutParams) mCustomButton.getLayoutParams();
                customButtonLP.start = mMargin;

                if (!mButtonGroup.isStacked) {
                    // Center the custom button vertically relative to the primary button.
                    customButtonLP.top = primaryButtonLP.top
                            + (mPrimaryButton.getMeasuredHeight()
                            - mCustomButton.getMeasuredHeight()) / 2;
                }
            }
        }

        // Apply a bottom padding to the layout, then calculate its final height.
        mTop = mBottom + mMargin;
        mBottom = mTop;
        mBottom = Math.max(mBottom, closeButtonBottom);
    }

    /**
     * Places a group of views on the current row, or stacks them over multiple rows if
     * group.isStacked is true. mStart, mEnd, and mBottom are updated to reflect the space taken by
     * the group.
     */
    private void placeButtonGroup(Group group) {
        if (group.gravity == Gravity.END) {
            for (int i = group.views.length - 1; i >= 0; i--) {
                placeChild(group.views[i], group.gravity);
                if (group.isStacked && i != 0) startRowWithIconMargin();
            }
        } else {  // group.gravity is Gravity.START or Gravity.FILL_HORIZONTAL
            for (int i = 0; i < group.views.length; i++) {
                placeChild(group.views[i], group.gravity);
                if (group.isStacked && i != group.views.length - 1) {
                    if (group == mButtonGroup) {
                        startRowWithoutIconMargin();
                    } else {
                        startRowWithIconMargin();
                    }
                }
            }
        }
    }

    /**
     * Places a single view on the current row, and updates the view's layout parameters to remember
     * its position. mStart, mEnd, and mBottom are updated to reflect the space taken by the view.
     */
    private void placeChild(View child, int gravity) {
        LayoutParams lp = (LayoutParams) child.getLayoutParams();

        int availableWidth = Math.max(0, mEnd - mStart - lp.startMargin - lp.endMargin);
        if (child.getMeasuredWidth() > availableWidth || gravity == Gravity.FILL_HORIZONTAL) {
            measureChildWithFixedWidth(child, availableWidth);
        }

        if (gravity == Gravity.START || gravity == Gravity.FILL_HORIZONTAL) {
            lp.start = mStart + lp.startMargin;
            mStart = lp.start + child.getMeasuredWidth() + lp.endMargin;
        } else {  // gravity == Gravity.END
            lp.start = mEnd - lp.endMargin - child.getMeasuredWidth();
            mEnd = lp.start - lp.startMargin;
        }

        lp.top = mTop + lp.topMargin;
        mBottom = Math.max(mBottom, lp.top + child.getMeasuredHeight() + lp.bottomMargin);
    }

    /**
     * Advances the current position to the next row and adds margins on the left, right, and top
     * of the new row.
     */
    private void startRowWithoutIconMargin() {
        mStart = mMargin;
        mEnd = mWidth - mMargin;
        mTop = mBottom;
    }

    /**
     * Advances the current position to the next row and adds margins on the left, right, and top
     * of the new row, accounting for the margins imposed by the icon.
     */
    private void startRowWithIconMargin() {
        startRowWithoutIconMargin();

        if (mIconView != null) {
            if (mIsUsingBigIcon) {
                mStart += mBigIconSize + mBigIconMargin;
            } else {
                mStart += mSmallIconSize + mSmallIconMargin;
            }
        }
    }

    /**
     * @return The width of the group, including the items' margins.
     */
    private int getWidthWithMargins(Group group) {
        if (group.isStacked) return getWidthWithMargins(group.views[0]);

        int width = 0;
        for (View v : group.views) {
            width += getWidthWithMargins(v);
        }
        return width;
    }

    private int getWidthWithMargins(View child) {
        LayoutParams lp = (LayoutParams) child.getLayoutParams();
        return child.getMeasuredWidth() + lp.startMargin + lp.endMargin;
    }

    private void measureChildWithFixedWidth(View child, int width) {
        int widthSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        child.measure(widthSpec, heightSpec);
    }

    /**
     * Updates the layout properties (margins, gravity, etc) of the button group to prepare for
     * placing it on its own row.
     */
    private void updateButtonGroupLayoutProperties() {
        int startEndMargin = 0;
        mButtonGroup.setHorizontalMode(mMargin / 2, startEndMargin, startEndMargin);
        mButtonGroup.gravity = Gravity.END;

        if (mButtonGroup.views.length >= 2) {
            int availableWidth = mEnd - mStart;
            int extraWidth = availableWidth - getWidthWithMargins(mButtonGroup);
            if (extraWidth < 0) {
                // Group is too wide to fit on a single row, so stack the group items vertically.
                mButtonGroup.setVerticalMode(mMargin / 2, 0);
                mButtonGroup.gravity = Gravity.FILL_HORIZONTAL;
            }
        }
    }

    /**
     * Listens for View clicks.
     * Classes that override this function MUST call this one.
     * @param view View that was clicked on.
     */
    @Override
    public void onClick(View view) {
        mInfoBarView.setControlsEnabled(false);

        if (view.getId() == R.id.infobar_close_button) {
            mInfoBarView.onCloseButtonClicked();
        } else if (view.getId() == R.id.button_primary) {
            mInfoBarView.onButtonClicked(true);
        } else if (view.getId() == R.id.button_secondary) {
            mInfoBarView.onButtonClicked(false);
        }
    }

    /**
     * Prepares text to be displayed as the InfoBar's main message, including setting up a
     * clickable link if the InfoBar requires it.
     */
    private CharSequence prepareMainMessageString() {
        SpannableStringBuilder fullString = new SpannableStringBuilder();

        if (mMessageMainText != null) fullString.append(mMessageMainText);

        // Concatenate the text to display for the link and make it clickable.
        if (mMessageLinkText != null) {
            if (fullString.length() > 0) fullString.append(" ");
            int spanStart = fullString.length();

            fullString.append(mMessageLinkText);
            fullString.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    mInfoBarView.onLinkClicked();
                }
            }, spanStart, fullString.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        return fullString;
    }
}
