// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.GridLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.TintedDrawable;

import java.util.List;

import javax.annotation.Nullable;

/**
 * Represents a single section in the {@link PaymentRequestUI} that flips between multiple states.
 *
 * The row is broken up into three major, vertically-centered sections:
 * .............................................................................................
 * . TITLE                                                          |                |         .
 * .................................................................|                |         .
 * . LEFT SUMMARY TEXT                        |  RIGHT SUMMARY TEXT |    ADD or LOGO | CHEVRON .
 * .................................................................|                |         .
 * . MAIN SECTION CONTENT                                           |                |         .
 * .............................................................................................
 *
 * 1) MAIN CONTENT
 *    The main content is on the left side of the UI.  This includes the title of the section and
 *    two bits of optional summary text.  Subclasses may extend this class to append more controls
 *    via the {@link #createMainSectionContent} function.
 *
 * 2) ADD or LOGO
 *    Displays an optional logo (e.g. a credit card image) that floats to the right of the main
 *    content.  May eventually be used to show an "ADD" button.
 *
 * 3) CHEVRON
 *    Drawn to indicate that the current section may be expanded.  Displayed only when the view is
 *    in the {@link #DISPLAY_MODE_EXPANDABLE} state.
 *
 * There are three states that the UI may flip between; see {@link #DISPLAY_MODE_NORMAL},
 * {@link #DISPLAY_MODE_EXPANDABLE}, and {@link #DISPLAY_MODE_FOCUSED} for details.
 *
 * TODO(dfalcantara): Replace this with a RelativeLayout once mocks are finalized.
 */
public abstract class PaymentRequestSection extends LinearLayout {
    public static final String TAG = "PaymentRequestUI";

    /** Listens for clicks on the widgets. */
    public static interface PaymentsSectionListener extends View.OnClickListener {
        /**
         * Called when the user selects a radio button option from an {@link OptionSection}.
         *
         * @param section Section that was changed.
         * @param option  {@link PaymentOption} that was selected.
         */
        void onPaymentOptionChanged(OptionSection section, PaymentOption option);
    }

    /** Normal mode: White background, displays the item assuming the user accepts it as is. */
    static final int DISPLAY_MODE_NORMAL = 0;

    /** Editable mode: White background, displays the item with an edit chevron. */
    static final int DISPLAY_MODE_EXPANDABLE = 1;

    /** Focused mode: Gray background, more padding, no edit chevron. */
    static final int DISPLAY_MODE_FOCUSED = 2;

    protected final PaymentsSectionListener mListener;
    protected final int mSideSpacing;

    private final int mVerticalSpacing;
    private final int mFocusedBackgroundColor;
    private final LinearLayout mMainSection;
    private final ImageView mLogoView;
    private final ImageView mChevronView;

    private TextView mTitleView;
    private LinearLayout mSummaryLayout;
    private TextView mSummaryLeftTextView;
    private TextView mSummaryRightTextView;

    private int mLogoResourceId;
    private int mDisplayMode;
    private boolean mIsSummaryAllowed = true;

    /**
     * Constructs an PaymentRequestSection.
     *
     * @param context     Context to pull resources from.
     * @param sectionName Title of the section to display.
     * @param listener    Listener to alert when something changes in the dialog.
     */
    private PaymentRequestSection(
            Context context, String sectionName, PaymentsSectionListener listener) {
        super(context);
        mListener = listener;
        setOnClickListener(listener);
        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);

        // Set the styling of the view.
        mFocusedBackgroundColor = ApiCompatibilityUtils.getColor(
                getResources(), R.color.payments_section_edit_background);
        mSideSpacing = getResources().getDimensionPixelSize(R.dimen.payments_section_side_spacing);
        mVerticalSpacing =
                getResources().getDimensionPixelSize(R.dimen.payments_section_vertical_spacing);
        setPadding(0, mVerticalSpacing, 0, mVerticalSpacing);

        TintedDrawable chevron =
                TintedDrawable.constructTintedDrawable(getResources(), R.drawable.ic_expanded);
        chevron.setTint(ApiCompatibilityUtils.getColorStateList(
                getResources(), R.color.payments_section_chevron));

        // Create the main content.
        mMainSection = prepareMainSection(sectionName);
        mLogoView = isLogoNecessary() ? createAndAddImageView(null) : null;
        mChevronView = createAndAddImageView(chevron);
        setDisplayMode(DISPLAY_MODE_NORMAL);
    }

    /**
     * Sets what logo should be displayed.
     *
     * @param resourceId ID of the logo to display.
     */
    protected void setLogoResource(int resourceId) {
        assert isLogoNecessary();
        mLogoResourceId = resourceId;
        mLogoView.setImageResource(resourceId);
        updateLogoVisibility();
    }

    /**
     * Updates what Views are displayed and how they look.
     *
     * @param displayMode What mode the widget is being displayed in.
     */
    public void setDisplayMode(int displayMode) {
        mDisplayMode = displayMode;
        setBackgroundColor(
                displayMode == DISPLAY_MODE_FOCUSED ? mFocusedBackgroundColor : Color.WHITE);
        updateLogoVisibility();
        mChevronView.setVisibility(displayMode == DISPLAY_MODE_EXPANDABLE ? VISIBLE : GONE);

        // The title gains extra spacing when there is another visible view in the main section.
        int numVisibleMainViews = 0;
        for (int i = 0; i < mMainSection.getChildCount(); i++) {
            if (mMainSection.getChildAt(i).getVisibility() == VISIBLE) numVisibleMainViews += 1;
        }
        boolean isTitleMarginNecessary =
                numVisibleMainViews > 1 && displayMode == DISPLAY_MODE_FOCUSED;
        ((ViewGroup.MarginLayoutParams) mTitleView.getLayoutParams()).bottomMargin =
                isTitleMarginNecessary ? mVerticalSpacing : 0;
    }

    /**
     * Changes what is being displayed in the summary.
     *
     * @param leftText  Text to display on the left side.  If null, the whole row hides.
     * @param rightText Text to display on the right side.  If null, only the right View hides.
     */
    public void setSummaryText(
            @Nullable CharSequence leftText, @Nullable CharSequence rightText) {
        mSummaryLeftTextView.setText(leftText);
        mSummaryRightTextView.setText(rightText);
        mSummaryRightTextView.setVisibility(TextUtils.isEmpty(rightText) ? GONE : VISIBLE);
        updateSummaryVisibility();
    }

    /**
     * Subclasses may override this method to add additional controls to the layout.
     *
     * @param mainSectionLayout Layout containing all of the main content of the section.
     */
    protected abstract void createMainSectionContent(LinearLayout mainSectionLayout);

    /**
     * Sets whether or not the summary text can be displayed.
     *
     * @param isAllowed Whether or not do display the summary text.
     */
    protected void setIsSummaryAllowed(boolean isAllowed) {
        mIsSummaryAllowed = isAllowed;
        updateSummaryVisibility();
    }

    /** @return Whether or not the logo should be displayed. */
    protected boolean isLogoNecessary() {
        return false;
    }

    /**
     * Creates the main section.  Subclasses must call super#createMainSection() immediately to
     * guarantee that Views are added in the correct order.
     *
     * @param sectionName Title to display for the section.
     */
    private final LinearLayout prepareMainSection(String sectionName) {
        // The main section is a vertical linear layout that subclasses can append to.
        LinearLayout mainSectionLayout = new LinearLayout(getContext());
        mainSectionLayout.setOrientation(VERTICAL);
        LinearLayout.LayoutParams mainParams = new LayoutParams(0, LayoutParams.WRAP_CONTENT);
        mainParams.weight = 1;
        ApiCompatibilityUtils.setMarginStart(mainParams, mSideSpacing);
        ApiCompatibilityUtils.setMarginEnd(mainParams, mSideSpacing);
        addView(mainSectionLayout, mainParams);

        // The title is always displayed for the row at the top of the main section.
        mTitleView = new TextView(getContext());
        mTitleView.setText(sectionName);
        ApiCompatibilityUtils.setTextAppearance(
                mTitleView, R.style.PaymentsUiSectionHeader);
        mainSectionLayout.addView(
                mTitleView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        // Create the two TextViews for showing the summary text.
        mSummaryLeftTextView = new TextView(getContext());
        ApiCompatibilityUtils.setTextAppearance(
                mSummaryLeftTextView, R.style.PaymentsUiSectionDefaultText);

        mSummaryRightTextView = new TextView(getContext());
        ApiCompatibilityUtils.setTextAppearance(
                mSummaryRightTextView, R.style.PaymentsUiSectionDefaultText);
        ApiCompatibilityUtils.setTextAlignment(mSummaryRightTextView, TEXT_ALIGNMENT_TEXT_END);

        // The main TextView sucks up all the available space.
        LinearLayout.LayoutParams leftLayoutParams = new LinearLayout.LayoutParams(
                0, LayoutParams.WRAP_CONTENT);
        leftLayoutParams.weight = 1;

        LinearLayout.LayoutParams rightLayoutParams = new LinearLayout.LayoutParams(
                LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        ApiCompatibilityUtils.setMarginStart(
                rightLayoutParams,
                getContext().getResources().getDimensionPixelSize(
                        R.dimen.payments_section_small_spacing));

        // The summary section displays up to two TextViews side by side.
        mSummaryLayout = new LinearLayout(getContext());
        mSummaryLayout.addView(mSummaryLeftTextView, leftLayoutParams);
        mSummaryLayout.addView(mSummaryRightTextView, rightLayoutParams);
        mainSectionLayout.addView(mSummaryLayout, new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        setSummaryText(null, null);

        createMainSectionContent(mainSectionLayout);
        return mainSectionLayout;
    }

    private ImageView createAndAddImageView(@Nullable Drawable drawable) {
        ImageView view = new ImageView(getContext());
        view.setImageDrawable(drawable);
        LayoutParams params =
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        ApiCompatibilityUtils.setMarginEnd(params, mSideSpacing);
        addView(view, params);
        return view;
    }

    private void updateSummaryVisibility() {
        boolean show = mIsSummaryAllowed && !TextUtils.isEmpty(mSummaryLeftTextView.getText());
        mSummaryLayout.setVisibility(show ? VISIBLE : GONE);
    }

    private void updateLogoVisibility() {
        if (mLogoView == null) return;
        boolean show = mLogoResourceId != 0 && mDisplayMode != DISPLAY_MODE_FOCUSED;
        mLogoView.setVisibility(show ? VISIBLE : GONE);
    }

    /**
     * Section with a secondary TextView beneath the summary to show additional details.
     *
     * ............................................................................
     * . TITLE                                                          |         .
     * .................................................................|         .
     * . LEFT SUMMARY TEXT                        |  RIGHT SUMMARY TEXT | CHEVRON .
     * .................................................................|         .
     * . EXTRA TEXT                                                     |         .
     * ............................................................................
     */
    public static class ExtraTextSection extends PaymentRequestSection {
        private TextView mExtraTextView;

        public ExtraTextSection(
                Context context, String sectionName, PaymentsSectionListener listener) {
            super(context, sectionName, listener);
            setExtraText(null);
        }

        @Override
        protected void createMainSectionContent(LinearLayout mainSectionLayout) {
            Context context = mainSectionLayout.getContext();

            mExtraTextView = new TextView(context);
            ApiCompatibilityUtils.setTextAppearance(
                    mExtraTextView, R.style.PaymentsUiSectionDescriptiveText);
            mainSectionLayout.addView(mExtraTextView, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        /**
         * Sets the CharSequence that is displayed in the secondary TextView.
         *
         * @param text Text to display.
         */
        public void setExtraText(CharSequence text) {
            mExtraTextView.setText(text);
            mExtraTextView.setVisibility(TextUtils.isEmpty(text) ? GONE : VISIBLE);
        }
    }

    /**
     * Section with an additional Layout for showing a total and how it is broken down.
     *
     * Normal mode:     Just the summary is displayed.
     *                  If no option is selected, the "empty label" is displayed in its place.
     * Expandable mode: Same as Normal, but shows the chevron.
     * Focused mode:    Hides the summary and chevron, then displays the full set of options.
     *
     * ............................................................................
     * . TITLE                                                          |         .
     * .................................................................|         .
     * . LEFT SUMMARY TEXT                        |  RIGHT SUMMARY TEXT |         .
     * .................................................................| CHEVRON .
     * .                                      | Line item 1 |    $13.99 |         .
     * .                                      | Line item 2 |      $.99 |         .
     * .                                      | Line item 3 |     $2.99 |         .
     * ............................................................................
     */
    public static class LineItemBreakdownSection extends PaymentRequestSection {
        private GridLayout mBreakdownLayout;

        public LineItemBreakdownSection(
                Context context, String sectionName, PaymentsSectionListener listener) {
            super(context, sectionName, listener);
        }

        @Override
        protected void createMainSectionContent(LinearLayout mainSectionLayout) {
            Context context = mainSectionLayout.getContext();

            // The breakdown is represented by an end-aligned GridLayout that takes up only as much
            // space as it needs.  The GridLayout ensures a consistent margin between the columns.
            mBreakdownLayout = new GridLayout(context);
            mBreakdownLayout.setColumnCount(2);
            LayoutParams breakdownParams =
                    new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            breakdownParams.gravity = Gravity.END;
            mainSectionLayout.addView(mBreakdownLayout, breakdownParams);
        }

        /**
         * Updates the breakdown, with the last item representing the full total.
         *
         * @param lineItems List of {@link LineItem}s.  The last item is assumed to be the total,
         *                  while the others show how the total is broken down.
         */
        public void update(List<LineItem> lineItems) {
            Context context = mBreakdownLayout.getContext();

            // Update the summary to display information about the total.
            CharSequence totalLabel = null;
            CharSequence totalValue = null;
            if (lineItems.size() != 0) {
                LineItem totalItem = lineItems.get(lineItems.size() - 1);
                totalLabel = totalItem.getLabel();
                totalValue = createValueString(totalItem.getCurrency(), totalItem.getPrice(), true);
            }
            setSummaryText(totalLabel, totalValue);

            // Update the breakdown, using one row per {@link LineItem}.
            int numItems = lineItems.size() - 1;
            mBreakdownLayout.removeAllViews();
            mBreakdownLayout.setRowCount(numItems);
            for (int i = 0; i < numItems; i++) {
                LineItem item = lineItems.get(i);

                TextView description = new TextView(context);
                ApiCompatibilityUtils.setTextAppearance(
                        description, R.style.PaymentsUiSectionDescriptiveText);
                description.setText(item.getLabel());

                TextView amount = new TextView(context);
                ApiCompatibilityUtils.setTextAppearance(
                        amount, R.style.PaymentsUiSectionDescriptiveText);
                amount.setText(createValueString(item.getCurrency(), item.getPrice(), false));

                // Each item is represented by a row in the GridLayout.
                GridLayout.LayoutParams descriptionParams = new GridLayout.LayoutParams(
                        GridLayout.spec(i, 1, GridLayout.END),
                        GridLayout.spec(0, 1, GridLayout.END));
                GridLayout.LayoutParams amountParams = new GridLayout.LayoutParams(
                        GridLayout.spec(i, 1, GridLayout.END),
                        GridLayout.spec(1, 1, GridLayout.END));
                ApiCompatibilityUtils.setMarginStart(amountParams,
                        context.getResources().getDimensionPixelSize(
                                R.dimen.payments_section_descriptive_item_spacing));

                mBreakdownLayout.addView(description, descriptionParams);
                mBreakdownLayout.addView(amount, amountParams);
            }
        }

        /**
         * Builds a CharSequence that displays a value in a particular currency.
         *
         * @param currency    Currency of the value being displayed.
         * @param value       Value to display.
         * @param isValueBold Whether or not to bold the item.
         * @return CharSequence that represents the whole value.
         */
        private CharSequence createValueString(String currency, String value, boolean isValueBold) {
            SpannableStringBuilder valueBuilder = new SpannableStringBuilder();
            valueBuilder.append(currency);
            valueBuilder.append(" ");

            int boldStartIndex = valueBuilder.length();
            valueBuilder.append(value);

            if (isValueBold) {
                valueBuilder.setSpan(new StyleSpan(android.graphics.Typeface.BOLD), boldStartIndex,
                        boldStartIndex + value.length(), 0);
            }

            return valueBuilder;
        }

        @Override
        public void setDisplayMode(int displayMode) {
            super.setDisplayMode(displayMode);
            mBreakdownLayout.setVisibility(displayMode == DISPLAY_MODE_FOCUSED ? VISIBLE : GONE);
        }
    }

    /**
     * Section that allows selecting one thing from a set of mutually-exclusive options.
     *
     * Normal mode:     The summary text displays the selected option, and the icon for the option
     *                  is displayed in the logo section (if it exists).
     *                  If no option is selected, the "empty label" is displayed in its place.
     *                  This is important for shipping options (e.g.) because there will be no
     *                  option selected by default and a prompt can be displayed.
     * Expandable mode: Same as Normal, but shows the chevron.
     * Focused mode:    Hides the summary and chevron, then displays the full set of options.
     *
     * .............................................................................................
     * . TITLE                                                          |                |         .
     * .................................................................|                |         .
     * . LEFT SUMMARY TEXT                        |  RIGHT SUMMARY TEXT |                |         .
     * .................................................................|    ADD or LOGO | CHEVRON .
     * . O Option 1                                              ICON 1 |                |         .
     * . O Option 2                                              ICON 2 |                |         .
     * . O Option 3                                              ICON 3 |                |         .
     * .............................................................................................
     */
    public static class OptionSection extends PaymentRequestSection implements OnClickListener {
        /** Displays a row representing a selectable option. */
        private class OptionRow extends LinearLayout {
            private final PaymentOption mOption;
            private final RadioButton mRadioButton;

            public OptionRow(Context context, PaymentOption item, boolean isSelected) {
                super(context);
                setGravity(Gravity.CENTER_VERTICAL);
                mOption = item;

                // The radio button hugs left.
                mRadioButton = new RadioButton(context);
                mRadioButton.setChecked(isSelected);
                LinearLayout.LayoutParams radioParams = new LinearLayout.LayoutParams(
                        LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                ApiCompatibilityUtils.setMarginEnd(radioParams, mSideSpacing);
                addView(mRadioButton, radioParams);

                // The description takes up all leftover space.
                TextView descriptionView = new TextView(context);
                descriptionView.setText(convertOptionToString(item));
                ApiCompatibilityUtils.setTextAppearance(
                        descriptionView, R.style.PaymentsUiSectionDefaultText);
                LinearLayout.LayoutParams descriptionParams =
                        new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT);
                descriptionParams.weight = 1;
                addView(descriptionView, descriptionParams);

                // If there's an icon to display, it floats to the right of everything.
                int resourceId = item.getDrawableIconId();
                if (resourceId != 0) {
                    ImageView iconView = new ImageView(context);
                    iconView.setImageResource(resourceId);
                    LinearLayout.LayoutParams logoParams = new LinearLayout.LayoutParams(
                            LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                    ApiCompatibilityUtils.setMarginStart(logoParams, mSideSpacing);
                    addView(iconView, logoParams);
                }

                // Clicking on either the radio button or the row itself should toggle its option.
                mRadioButton.setOnClickListener(OptionSection.this);
                setOnClickListener(OptionSection.this);
            }

            /** Sets the selected state of this item, alerting the listener if selected. */
            public void setChecked(boolean isChecked) {
                mRadioButton.setChecked(isChecked);

                if (isChecked) {
                    updateSelectedItem(mOption);
                    mListener.onPaymentOptionChanged(OptionSection.this, mOption);
                }
            }
        }

        /** Text to display in the summary when there is no selected option. */
        private final CharSequence mEmptyLabel;

        /** How much space is between each option. */
        private final int mTopMargin;

        /** Layout containing all the {@link OptionRow}s. */
        private RadioGroup mOptionLayout;

        /**
         * Constructs an OptionSection.
         *
         * @param context     Context to pull resources from.
         * @param sectionName Title of the section to display.
         * @param emptyLabel  An optional string to display when no item is selected.
         * @param listener    Listener to alert when something changes in the dialog.
         */
        public OptionSection(Context context, String sectionName, @Nullable CharSequence emptyLabel,
                PaymentsSectionListener listener) {
            super(context, sectionName, listener);
            mTopMargin = context.getResources().getDimensionPixelSize(
                    R.dimen.payments_section_small_spacing);
            mEmptyLabel = emptyLabel;
            setSummaryText(emptyLabel, null);
        }

        @Override
        public void onClick(View v) {
            for (int i = 0; i < mOptionLayout.getChildCount(); i++) {
                OptionRow row = (OptionRow) mOptionLayout.getChildAt(i);
                row.setChecked(v == row || v == row.mRadioButton);
            }
        }

        @Override
        protected boolean isLogoNecessary() {
            return true;
        }

        @Override
        protected void createMainSectionContent(LinearLayout mainSectionLayout) {
            Context context = mainSectionLayout.getContext();

            mOptionLayout = new RadioGroup(context);
            mOptionLayout.setOrientation(VERTICAL);
            mainSectionLayout.addView(mOptionLayout, new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        /** Updates the View to account for the new {@link SectionInformation} being passed in. */
        public void update(SectionInformation information) {
            PaymentOption selectedItem = information.getSelectedItem();
            updateSelectedItem(selectedItem);
            updateOptionList(information, selectedItem);
        }

        @Override
        public void setDisplayMode(int displayMode) {
            super.setDisplayMode(displayMode);

            if (displayMode == DISPLAY_MODE_FOCUSED) {
                setIsSummaryAllowed(false);
                mOptionLayout.setVisibility(VISIBLE);
            } else {
                setIsSummaryAllowed(true);
                mOptionLayout.setVisibility(GONE);
            }
        }

        private void updateSelectedItem(PaymentOption selectedItem) {
            if (selectedItem == null) {
                setLogoResource(0);
                if (TextUtils.isEmpty(mEmptyLabel)) {
                    setIsSummaryAllowed(false);
                } else {
                    setSummaryText(mEmptyLabel, null);
                }
            } else {
                setLogoResource(selectedItem.getDrawableIconId());
                setSummaryText(convertOptionToString(selectedItem), null);
            }
        }

        private void updateOptionList(SectionInformation information, PaymentOption selectedItem) {
            mOptionLayout.removeAllViews();
            if (information.isEmpty()) return;

            for (int i = 0; i < information.getSize(); i++) {
                PaymentOption item = information.getItem(i);
                OptionRow row = new OptionRow(getContext(), item, item == selectedItem);
                LinearLayout.LayoutParams rowParams = new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
                rowParams.topMargin = mTopMargin;
                mOptionLayout.addView(row, rowParams);
            }
        }

        private CharSequence convertOptionToString(PaymentOption item) {
            return new StringBuilder(item.getLabel()).append("\n").append(item.getSublabel());
        }
    }
}
